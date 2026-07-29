#!/usr/bin/env python3
"""
Publishes an OTA "update available" trigger message to the MQTT broker --
the same JSON shape the ESP32 side (main.c's handle_ota_trigger, listening
on MQTT_TOPIC_OTA) expects:

    {"manifest_url": "<url>", "slot": "A" | "B"}

Usage:
    notify_mqtt.py --manifest-url <url> --slot A|B
    notify_mqtt.py --manifest-url <url> --slot B --broker test.mosquitto.org --port 1883
"""
import argparse
import json
import sys

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("ERROR: paho-mqtt not installed. Run: pip install paho-mqtt", file=sys.stderr)
    sys.exit(1)

DEFAULT_BROKER = "broker.hivemq.com"
DEFAULT_PORT = 1883
DEFAULT_TOPIC = "linto/sensors/node1/ota/update"


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--manifest-url", required=True)
    p.add_argument("--slot", required=True, choices=["A", "B"])
    p.add_argument("--broker", default=DEFAULT_BROKER)
    p.add_argument("--port", type=int, default=DEFAULT_PORT)
    p.add_argument("--topic", default=DEFAULT_TOPIC)
    args = p.parse_args()

    payload = json.dumps({"manifest_url": args.manifest_url, "slot": args.slot})

    client = mqtt.Client()

    print(f"Connecting to {args.broker}:{args.port} ...")
    client.connect(args.broker, args.port, keepalive=10)
    client.loop_start()

    info = client.publish(args.topic, payload, qos=1)
    info.wait_for_publish(timeout=10)

    client.loop_stop()
    client.disconnect()

    if not info.is_published():
        print("ERROR: publish did not complete", file=sys.stderr)
        sys.exit(1)

    print(f"Published to {args.topic}: {payload}")


if __name__ == "__main__":
    main()
