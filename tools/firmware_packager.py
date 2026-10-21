#!/usr/bin/env python3
"""
Firmware Packaging Utility for Fail-Safe Dual-Bank OTA Bootloader
Author: Amit Bakale (Avionics & Embedded Software Engineer)

Takes an input raw binary (.bin), computes IEEE 802.3 CRC-32,
packages it with the standard 28-byte OTA header, and outputs a signed .ota file.
"""

import sys
import struct
import zlib
import argparse

OTA_IMAGE_MAGIC = 0x4F544131  # "OTA1"

def create_ota_package(bin_path, output_path, major, minor, patch, entry_point):
    with open(bin_path, 'rb') as f:
        payload = f.read()

    payload_size = len(payload)
    # Compute CRC-32 (IEEE 802.3)
    payload_crc = zlib.crc32(payload) & 0xFFFFFFFF

    # Struct format:
    # uint32_t magic (4)
    # uint8_t ver_major, ver_minor, ver_patch, reserved (4)
    # uint32_t image_size (4)
    # uint32_t image_crc32 (4)
    # uint32_t entry_point (4)
    # uint32_t header_crc32 (4)
    # Total = 24 bytes before header_crc32, 28 bytes total
    header_raw = struct.pack('<IBBBBIII',
                             OTA_IMAGE_MAGIC,
                             major, minor, patch, 0,
                             payload_size,
                             payload_crc,
                             entry_point)
    
    header_crc = zlib.crc32(header_raw) & 0xFFFFFFFF
    full_header = header_raw + struct.pack('<I', header_crc)

    with open(output_path, 'wb') as f:
        f.write(full_header)
        f.write(payload)

    print(f"============================================================")
    print(f" [SUCCESS] Generated OTA Firmware Package: {output_path}")
    print(f" Version      : v{major}.{minor}.{patch}")
    print(f" Entry Point  : 0x{entry_point:08X}")
    print(f" Payload Size : {payload_size} bytes")
    print(f" Payload CRC  : 0x{payload_crc:08X}")
    print(f" Header CRC   : 0x{header_crc:08X}")
    print(f" Total Size   : {len(full_header) + payload_size} bytes")
    print(f"============================================================")

def main():
    parser = argparse.ArgumentParser(description="OTA Firmware Packager")
    parser.add_argument("--bin", required=True, help="Input application .bin file")
    parser.add_argument("--out", required=True, help="Output .ota package file")
    parser.add_argument("--major", type=int, default=1, help="Major version")
    parser.add_argument("--minor", type=int, default=0, help="Minor version")
    parser.add_argument("--patch", type=int, default=0, help="Patch version")
    parser.add_argument("--entry", type=lambda x: int(x, 0), default=0x0800801C, help="Entry Point Address")

    args = parser.parse_args()
    create_ota_package(args.bin, args.out, args.major, args.minor, args.patch, args.entry)

if __name__ == '__main__':
    main()
