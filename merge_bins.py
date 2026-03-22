#!/usr/bin/env python3
"""
Create BambuHelper release firmware files.

Usage:
    python merge_bins.py              # auto-reads version from config.h
    python merge_bins.py v2.5         # override version
    python merge_bins.py --ota        # OTA binary only
    python merge_bins.py --full       # WebFlasher binary only

Output:
    firmware/v2.4-Beta1/BambuHelper-WebFlasher-v2.4-Beta1.bin
    firmware/v2.4-Beta1/BambuHelper-OTA-v2.4-Beta1.bin
"""

import argparse
import os
import re
import sys

BUILD_DIR = '.pio/build/esp32s3'
BOOTLOADER = os.path.join(BUILD_DIR, 'bootloader.bin')
PARTITIONS = os.path.join(BUILD_DIR, 'partitions.bin')
FIRMWARE = os.path.join(BUILD_DIR, 'firmware.bin')
CONFIG_H = os.path.join('include', 'config.h')

# ESP32-S3 standard flash offsets
BOOTLOADER_OFFSET = 0x0
PARTITIONS_OFFSET = 0x8000
FIRMWARE_OFFSET = 0x10000


def read_version_from_config():
    """Extract FW_VERSION from config.h."""
    if not os.path.exists(CONFIG_H):
        return None
    with open(CONFIG_H, 'r') as f:
        for line in f:
            m = re.match(r'#define\s+FW_VERSION\s+"([^"]+)"', line)
            if m:
                return m.group(1)
    return None


def get_paths(version):
    """Return output directory and file paths for a given version."""
    out_dir = os.path.join('firmware', version)
    merged = os.path.join(out_dir, f'BambuHelper-WebFlasher-{version}.bin')
    ota = os.path.join(out_dir, f'BambuHelper-OTA-{version}.bin')
    return out_dir, merged, ota


def create_ota_binary(out_dir, out_path):
    """Copy firmware.bin as OTA update file."""
    if not os.path.exists(FIRMWARE):
        print(f"Error: {FIRMWARE} not found. Run 'pio run' first.")
        return False

    os.makedirs(out_dir, exist_ok=True)

    with open(FIRMWARE, 'rb') as src, open(out_path, 'wb') as dst:
        dst.write(src.read())

    size = os.path.getsize(out_path)
    print(f"  OTA binary: {out_path} ({size / 1024:.1f} KB)")
    return True


def merge_binaries(out_dir, out_path):
    """Merge bootloader + partitions + firmware into a single flashable binary."""
    for path in [BOOTLOADER, PARTITIONS, FIRMWARE]:
        if not os.path.exists(path):
            print(f"Error: {path} not found. Run 'pio run' first.")
            return False

    os.makedirs(out_dir, exist_ok=True)

    with open(out_path, 'wb') as out:
        with open(BOOTLOADER, 'rb') as f:
            bl = f.read()
            out.write(bl)

        out.write(b'\xFF' * (PARTITIONS_OFFSET - len(bl)))

        with open(PARTITIONS, 'rb') as f:
            pt = f.read()
            out.write(pt)

        out.write(b'\xFF' * (FIRMWARE_OFFSET - PARTITIONS_OFFSET - len(pt)))

        with open(FIRMWARE, 'rb') as f:
            fw = f.read()
            out.write(fw)

    total = os.path.getsize(out_path)
    print(f"  WebFlasher: {out_path} ({total / 1024:.1f} KB)")
    return True


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Create BambuHelper release firmware')
    parser.add_argument('version', nargs='?', default=None, help='Version (default: read from config.h)')
    parser.add_argument('--ota', action='store_true', help='OTA binary only')
    parser.add_argument('--full', action='store_true', help='WebFlasher binary only')
    args = parser.parse_args()

    version = args.version or read_version_from_config()
    if not version:
        print("Error: could not read FW_VERSION from config.h. Pass version as argument.")
        sys.exit(1)

    out_dir, merged_path, ota_path = get_paths(version)
    print(f"Version: {version}\n")

    if args.ota:
        success = create_ota_binary(out_dir, ota_path)
    elif args.full:
        success = merge_binaries(out_dir, merged_path)
    else:
        s1 = merge_binaries(out_dir, merged_path)
        s2 = create_ota_binary(out_dir, ota_path)
        success = s1 and s2

        if success:
            print(f"\n{'='*60}")
            print(f"Release {version} ready in {out_dir}/")
            print(f"{'='*60}")
            print(f"\nGitHub Release - attach both files:")
            print(f"  ...WebFlasher-{version}.bin  - new users (ESP Web Flasher @ 0x0)")
            print(f"  ...OTA-{version}.bin         - existing users (Web UI update)")

    sys.exit(0 if success else 1)
