#include "ota_apply.h"
#include "ota_push.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "psa/crypto.h"
#include "cJSON.h"

static const char *TAG = "ota_apply";

/* ------------------------------------------------------------
 * Generic "GET a URL into a growable heap buffer" helper. Used both
 * for the small manifest.json and the (much larger) firmware .bin.
 * Caller must free() the returned buffer. Returns NULL on failure.
 * ------------------------------------------------------------ */
static uint8_t *http_get_to_buffer(const char *url, size_t *out_len)
{
    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach, /* TLS cert validation
            for https:// URLs (e.g. GitHub Releases). Requires
            CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y in sdkconfig -- enable
            via `idf.py menuconfig` -> Component config -> mbedTLS ->
            Certificate Bundle, if not already on. */
        .timeout_ms = 15000,
        /* GitHub release downloads redirect to objects.githubusercontent.com
         * with a long signed URL (500-1000+ chars of query string). The
         * default 512-byte buffer is too small to build a request against
         * that redirected URL, causing "Out of buffer" / ESP_FAIL on open.
         * Sized with generous headroom here. */
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "http_client_init failed");
        return NULL;
    }

    /* GitHub release-download URLs always 302-redirect to a signed
     * objects.githubusercontent.com URL. The manual open/fetch_headers/
     * read flow used below does NOT follow redirects automatically the
     * way esp_http_client_perform() does -- we have to detect a 3xx
     * status ourselves and explicitly call esp_http_client_set_redirection()
     * (which points the client at the Location header it already
     * captured), then reopen. Bounded to avoid an infinite loop if a
     * server ever redirects in a cycle. */
    const int MAX_REDIRECTS = 5;
    int64_t content_length = -1;
    int status = 0;

    for (int redirect_count = 0; redirect_count <= MAX_REDIRECTS; redirect_count++) {
        esp_err_t err = esp_http_client_open(client, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "http open failed: %s", esp_err_to_name(err));
            esp_http_client_cleanup(client);
            return NULL;
        }

        content_length = esp_http_client_fetch_headers(client);
        status = esp_http_client_get_status_code(client);

        if (status >= 300 && status < 400) {
            ESP_LOGI(TAG, "redirect (%d), following...", status);
            esp_http_client_close(client);
            esp_err_t rerr = esp_http_client_set_redirection(client);
            if (rerr != ESP_OK) {
                ESP_LOGE(TAG, "set_redirection failed: %s", esp_err_to_name(rerr));
                esp_http_client_cleanup(client);
                return NULL;
            }
            continue; /* loop: open() again, now pointed at the new URL */
        }

        /* Not a redirect -- either a real 200 with content, or a real
         * error status. Break out and handle below. */
        break;
    }

    if (status < 200 || status >= 300) {
        ESP_LOGE(TAG, "http request failed, final status %d", status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return NULL;
    }

    if (content_length <= 0) {
        ESP_LOGE(TAG, "bad content length: %lld", (long long)content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return NULL;
    }

    /* Prefer PSRAM for anything large (the firmware .bin) -- ESP32-S3
     * modules with PSRAM handle this easily; without PSRAM, a few
     * hundred KB may not fit alongside WiFi/MQTT/FreeRTOS overhead.
     * heap_caps_malloc falls back gracefully if MALLOC_CAP_SPIRAM
     * isn't available (returns NULL, so check as usual -- shown here
     * via plain malloc for portability; swap to heap_caps_malloc(len,
     * MALLOC_CAP_SPIRAM) if you want to force PSRAM specifically). */
    uint8_t *buf = malloc((size_t)content_length);
    if (!buf) {
        ESP_LOGE(TAG, "malloc(%lld) failed", (long long)content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return NULL;
    }

    int total_read = 0;
    while (total_read < content_length) {
        int r = esp_http_client_read(client, (char *)(buf + total_read),
                                      content_length - total_read);
        if (r < 0) {
            ESP_LOGE(TAG, "http read error at offset %d", total_read);
            free(buf);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return NULL;
        }
        if (r == 0) break; /* EOF */
        total_read += r;
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (total_read != content_length) {
        ESP_LOGE(TAG, "short read: got %d, expected %lld", total_read, (long long)content_length);
        free(buf);
        return NULL;
    }

    *out_len = (size_t)total_read;
    return buf;
}

/* Computes SHA-256 and compares against a lowercase hex string (as
 * generate_manifest.py's hashlib.sha256().hexdigest() produces).
 *
 * Uses the PSA Crypto API (psa_hash_compute), not the legacy
 * mbedtls_sha256_*() functions -- ESP-IDF v6.0 upgraded to Mbed TLS
 * 4.0, which removed the legacy API and mbedtls/sha256.h entirely.
 * PSA crypto is auto-initialized by ESP-IDF at startup, so no
 * separate psa_crypto_init() call is needed here. */
static bool verify_sha256(const uint8_t *data, size_t len, const char *expected_hex)
{
    uint8_t hash[32];
    size_t hash_length = 0;

    psa_status_t status = psa_hash_compute(PSA_ALG_SHA_256, data, len,
                                            hash, sizeof(hash), &hash_length);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_hash_compute failed: %d", (int)status);
        return false;
    }

    char hex[65];
    for (int i = 0; i < 32; i++) {
        snprintf(&hex[i * 2], 3, "%02x", hash[i]);
    }

    bool match = (strcasecmp(hex, expected_hex) == 0);
    if (!match) {
        ESP_LOGE(TAG, "SHA-256 mismatch: got %s, expected %s", hex, expected_hex);
    }
    return match;
}

bool ota_apply_from_manifest(const char *manifest_url, const char *target_slot)
{
    ESP_LOGI(TAG, "fetching manifest: %s", manifest_url);

    size_t manifest_len = 0;
    uint8_t *manifest_buf = http_get_to_buffer(manifest_url, &manifest_len);
    if (!manifest_buf) {
        ESP_LOGE(TAG, "failed to fetch manifest");
        return false;
    }

    /* cJSON_ParseWithLength wants a NUL-terminated string for the
     * simple Parse() call, so grow by one for the terminator. */
    char *manifest_json = malloc(manifest_len + 1);
    if (!manifest_json) {
        free(manifest_buf);
        return false;
    }
    memcpy(manifest_json, manifest_buf, manifest_len);
    manifest_json[manifest_len] = '\0';
    free(manifest_buf);

    cJSON *root = cJSON_Parse(manifest_json);
    free(manifest_json);
    if (!root) {
        ESP_LOGE(TAG, "manifest.json parse failed");
        return false;
    }

    const char *slot_key = (strcasecmp(target_slot, "B") == 0) ? "slotB" : "slotA";
    cJSON *slot_obj = cJSON_GetObjectItem(root, slot_key);
    if (!slot_obj) {
        ESP_LOGE(TAG, "manifest missing '%s' object", slot_key);
        cJSON_Delete(root);
        return false;
    }

    cJSON *url_item    = cJSON_GetObjectItem(slot_obj, "url");
    cJSON *sha_item     = cJSON_GetObjectItem(slot_obj, "sha256");
    cJSON *size_item    = cJSON_GetObjectItem(slot_obj, "size");

    if (!cJSON_IsString(url_item) || !cJSON_IsString(sha_item) || !cJSON_IsNumber(size_item)) {
        ESP_LOGE(TAG, "manifest '%s' object missing url/sha256/size", slot_key);
        cJSON_Delete(root);
        return false;
    }

    char firmware_url[512];
    char expected_sha256[65];
    strncpy(firmware_url, url_item->valuestring, sizeof(firmware_url) - 1);
    firmware_url[sizeof(firmware_url) - 1] = '\0';
    strncpy(expected_sha256, sha_item->valuestring, sizeof(expected_sha256) - 1);
    expected_sha256[sizeof(expected_sha256) - 1] = '\0';
    uint32_t expected_size = (uint32_t)size_item->valuedouble;

    cJSON_Delete(root);

    ESP_LOGI(TAG, "downloading firmware: %s (%lu bytes expected)",
             firmware_url, (unsigned long)expected_size);

    size_t fw_len = 0;
    uint8_t *fw_buf = http_get_to_buffer(firmware_url, &fw_len);
    if (!fw_buf) {
        ESP_LOGE(TAG, "failed to download firmware");
        return false;
    }

    if (fw_len != expected_size) {
        ESP_LOGE(TAG, "size mismatch: got %u, manifest says %lu",
                 (unsigned)fw_len, (unsigned long)expected_size);
        free(fw_buf);
        return false;
    }

    if (!verify_sha256(fw_buf, fw_len, expected_sha256)) {
        /* Never forward an unverified image to the STM32 -- this is
         * the check that matters most here. */
        free(fw_buf);
        return false;
    }

    ESP_LOGI(TAG, "firmware verified, pushing to STM32...");
    bool ok = stm32_ota_push(fw_buf, (uint32_t)fw_len);

    free(fw_buf);
    return ok;
}
