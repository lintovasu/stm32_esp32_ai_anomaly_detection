# STM32F407 + ESP32-S3 OTA — Build & Integration Guide

Read `DESIGN.md` first for the full picture. This file is the "how to
actually wire it into your existing project" checklist.

## 1. Bootloader project (new, standalone)

Create a **separate** STM32CubeIDE project (or Makefile project) for the
bootloader:

1. New project, same MCU (STM32F407VGTx or whichever exact part you have).
2. Add `bootloader/src/*.c` and `bootloader/inc/*.h` to it.
3. Replace the default linker script with `linker/bootloader.ld`.
4. You still need a startup file (`startup_stm32f407xx.s`) and the
   `.isr_vector` table — copy the one CubeIDE generates for a normal
   project; the bootloader's vector table is standard, only the app's
   gets relocated at runtime.
5. Link against STM32F4xx HAL (just the FLASH and RCC modules — you
   don't need much else in the bootloader).
6. Build. This produces `bootloader.bin`, exactly 32 KB or less.
7. Flash it to `0x08000000` — this only needs doing once per board,
   ever (unless you update the bootloader itself, which is a separate,
   more careful process outside the scope of this v1).

## 2. Application project changes (your existing iot-firmware STM32 code)

1. Add `app_ota_client/src/*.c` and `app_ota_client/inc/*.h` to your
   existing app project.
2. Also add `bootloader/inc/flash_metadata.h`, `bootloader/inc/crc32.h`,
   `bootloader/inc/flash_ll.h` and their `.c` files — the app needs the
   same metadata struct definition and flash helpers.
3. Replace your app's linker script with a copy of
   `linker/app_common.ld`, but you now need **two build configurations**:
   - `Slot A` config: `FLASH ORIGIN = 0x08010000, LENGTH = 448K`
   - `Slot B` config: `FLASH ORIGIN = 0x08080000, LENGTH = 512K`
   In CubeIDE this is easiest as two separate Build Configurations
   (Project > Properties > C/C++ Build > Settings) each pointing at its
   own linker script variant.
4. In your app's startup sequence, call `ota_receiver_init()` once
   after `HAL_Init()`.
5. A few seconds after boot, once your existing sensor/MQTT self-checks
   pass, call `ota_confirm_good()`.
6. Wire the three OTA UART frame types (BEGIN/DATA/END) into your
   existing UART RX handling — route them to `ota_receiver_begin()`,
   `ota_receiver_data()`, `ota_receiver_end()` respectively. Since
   `ota_receiver_begin()` blocks for ~1s doing sector erase, call it
   from a low-priority FreeRTOS task, not your UART ISR directly (post
   a queue message from the ISR, handle the erase in a task).
7. After `ota_receiver_end()` returns `OTA_OK`, ACK the ESP32, then call
   `ota_reboot_to_apply_update()`.

## 3. Initial flashing (factory image)

First-ever flash of a board:
1. Flash `bootloader.bin` to `0x08000000`.
2. Build your app with the **Slot A** linker config, flash the result
   to `0x08010000`.
3. Leave the metadata sector (`0x08008000`) erased (all `0xFF`) — the
   bootloader's `flash_metadata_read()` will see an invalid magic and
   call `flash_metadata_init_defaults()` automatically on first boot,
   which sets `active_slot = SLOT_A, confirmed_good = 1`.
4. Power on. Bootloader jumps straight to Slot A.

## 4. ESP32-S3 side

1. Add `esp32_host/stm32_ota_push.c` to your ESP-IDF project.
2. Wire `stm32_send_frame()` / `stm32_wait_ack()` to your existing
   HDLC+CRC16 UART transport (same one you use for sensor telemetry).
3. Hook `stm32_ota_push()` up to wherever your MQTT OTA-trigger handler
   lives — after `esp_https_ota` (or a plain HTTPS GET) downloads and
   SHA256-verifies the new STM32 firmware `.bin`, call
   `stm32_ota_push(buffer, size)`.
4. Build your **new STM32 app binary** for whichever slot is currently
   *inactive* — e.g. if the fleet is running Slot A, your release
   pipeline should produce the Slot B–linked `.bin` as the OTA payload.
   In practice this usually means: build both Slot A and Slot B
   variants every release, and have the ESP32/cloud side track which
   slot each device is currently on so it pushes the right one. (A
   simpler alternative: always build for "the other slot" from
   whatever the device last reported in its MQTT telemetry.)

## 5. Testing the rollback path before trusting it in the field

Deliberately flash a "bad" Slot B image (e.g. one that hangs before
calling `ota_confirm_good()`) via the normal OTA flow, and confirm:
- Device resets into the bad image `MAX_BOOT_ATTEMPTS + 1` times
- Bootloader flips `active_slot` back to Slot A automatically
- Device comes back up healthy on Slot A afterward

This is the single most important test to run before shipping this to
real hardware — a bootloader bug here is the difference between "OTA
occasionally fails" and "device is bricked in the field."

## 6. CI setup — automatic build + release on every version tag

Files added for this: `ci/Makefile.bootloader`, `ci/Makefile.app`,
`ci/scripts/generate_manifest.py`, `.github/workflows/firmware-release.yml`.

**Why Makefiles, when the project was built in CubeIDE:** CI runs headless —
no GUI, so the CubeIDE project files (`.project`, `.cproject`) can't drive
the build directly. The Makefiles mirror the same `arm-none-eabi-gcc` flags
CubeIDE uses under the hood, targeting the same source files and linker
scripts you already have. You can keep using CubeIDE locally for day-to-day
development; CI just needs its own headless path to the same output.

**What you need to do to switch over:**

1. **Fill in the real source list.** `ci/Makefile.app`'s `APP_SRCS` line is
   a placeholder glob (`Src/*.c Core/Src/*.c`) — point it at your actual
   project's source layout. Same for `ci/Makefile.bootloader`'s `SRCS` list
   if your HAL file selection differs.
2. **Vendor or submodule your STM32Cube `Drivers/` folder** (CMSIS + HAL)
   into the repo if it isn't already — CI has no access to anything outside
   the checked-out repo, so it can't reach a Drivers folder that only
   exists on your local machine.
3. **Add a `startup/startup_stm32f407xx.s`** to the repo if you don't have
   one tracked yet — CubeIDE generates this automatically but it needs to
   be committed for CI to see it.
4. **Push a version tag to trigger a release:**
   ```
   git tag v1.4.0
   git push --tags
   ```
   This alone kicks off the whole pipeline: build bootloader → build Slot A
   → build Slot B → generate `manifest.json` with SHA-256 for both → sanity
   check all artifacts are non-empty and manifest is valid JSON → publish a
   GitHub Release with all four files attached.

**What comes out the other end**, attached to the release:
```
firmware_v1.4.0_slotA.bin
firmware_v1.4.0_slotB.bin
bootloader.bin
manifest.json
```

**How the ESP32 side then uses it:** your MQTT trigger message only needs
to carry the manifest URL (`.../releases/download/v1.4.0/manifest.json`).
The ESP32 fetches that, reads `slotA`/`slotB` sub-objects for the size and
SHA-256, downloads whichever `.bin` matches the device's currently-inactive
slot (per the status-reporting addition from earlier), and verifies against
the hash *from the manifest*, not a value hand-typed anywhere. This is the
piece that closes the "typed a hash into an MQTT message" gap from before.

**What this CI setup does NOT do (deliberately, for now):**
- Doesn't auto-trigger fleet-wide MQTT rollout on release — that's a
  policy decision (all devices at once vs. staged rollout vs. manual
  approval) left as the commented-out step at the bottom of the workflow.
  Wire it up once you've decided how you want rollout to behave.
- Doesn't run hardware-in-the-loop tests — this pipeline only proves the
  code compiles, links within its slot's size budget, and produces valid
  artifacts. It doesn't flash real hardware and check it boots. Worth
  adding later (e.g. a self-hosted runner with a board on a USB/ST-Link)
  once the basic pipeline is solid.

## 7. Known simplifications in this v1 (documented, not fixed)

- No cryptographic signature check on the staged image — CRC32 only
  catches transport corruption, not a malicious/tampered image. Add
  signature verification in the bootloader before promoting a slot if
  the deployment is network-exposed in a way that matters to you.
- No dual-copy metadata sector — a power loss during the metadata write
  itself (a few-hundred-microsecond window) could in theory corrupt
  metadata. Sector 3 is reserved and unused for exactly this future
  improvement (write to whichever of two copies has the older
  sequence number, mirror A/B style).
- OTA data transfer requires in-order chunks, no resume-after-disconnect.
  Fine for a short UART hop; would need a chunk bitmap to support
  resuming a partial transfer.
