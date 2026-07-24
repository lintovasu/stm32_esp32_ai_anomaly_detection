#!/usr/bin/env python3
"""
Generates manifest.json for a firmware release. This is the file the
ESP32-S3 fetches and trusts -- it carries the SHA-256 for each slot
binary, so the hash always comes from the same place as the binary
itself rather than being typed by hand into an MQTT message.

Usage:
    generate_manifest.py --version 1.4.0 \
        --slotA build/app_slotA/firmware_v1.4.0_slotA.bin \
        --slotB build/app_slotB/firmware_v1.4.0_slotB.bin \
        --notes "Fixed MPU6050 RMS overflow at high vibration" \
        --out manifest.json
"""
import argparse
import hashlib
import json
import os
import sys
from datetime import datetime, timezone


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--version", required=True)
    p.add_argument("--slotA", required=True, help="path to slot A .bin")
    p.add_argument("--slotB", required=True, help="path to slot B .bin")
    p.add_argument("--notes", default="")
    p.add_argument("--base-url", default="",
                    help="e.g. https://github.com/OWNER/REPO/releases/download/vX.Y.Z "
                         "-- prefixed to filenames in the manifest so the ESP32 can "
                         "build a direct download URL without extra lookups")
    p.add_argument("--out", default="manifest.json")
    args = p.parse_args()

    for path in (args.slotA, args.slotB):
        if not os.path.isfile(path):
            print(f"ERROR: {path} not found", file=sys.stderr)
            sys.exit(1)

    def entry(path):
        name = os.path.basename(path)
        return {
            "filename": name,
            "url": f"{args.base_url.rstrip('/')}/{name}" if args.base_url else name,
            "size": os.path.getsize(path),
            "sha256": sha256_of(path),
        }

    manifest = {
        "version": args.version,
        "released_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "notes": args.notes,
        "slotA": entry(args.slotA),
        "slotB": entry(args.slotB),
    }

    with open(args.out, "w") as f:
        json.dump(manifest, f, indent=2)

    print(f"Wrote {args.out}:")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
