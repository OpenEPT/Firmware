#!/usr/bin/env python3
"""
@file    extractmi.py
@brief   Extract EEPROM image (system + LittleFS)
"""

import sys
import os
import struct
import zlib
import subprocess
import tempfile
import shutil

# ==== CONSTANTS (MORAJU BITI ISTI KAO U buildmi) ====
TOTAL_SIZE = 256 * 1024
SYSTEM_SIZE = 4096
BLOCK_SIZE = 256
BLOCK_COUNT = 1008

MAGIC = 0xA5A6A7A8


# ============================================
# LOG HELPERS
# ============================================
def log_step(msg):
    print(f"\n[STEP] {msg}")

def log_info(msg):
    print(f"[INFO] {msg}")

def log_ok(msg):
    print(f"[ OK ] {msg}")

def log_err(msg):
    print(f"[FAIL] {msg}")


# ============================================
# PARSE SYSTEM REGION
# ============================================
def parse_system_region(data):
    log_step("PARSE SYSTEM REGION")

    if len(data) != SYSTEM_SIZE:
        raise Exception("Invalid system region size")

    magic, size = struct.unpack("<II", data[:8])

    log_info(f"MAGIC: 0x{magic:08X}")
    log_info(f"SIZE : {size}")

    if magic != MAGIC:
        raise Exception("Invalid MAGIC")

    payload = data[8:8 + size]
    crc_stored = struct.unpack("<I", data[8 + size:8 + size + 4])[0]

    crc_calc = zlib.crc32(payload) & 0xFFFFFFFF

    log_info(f"CRC stored: 0x{crc_stored:08X}")
    log_info(f"CRC calc  : 0x{crc_calc:08X}")

    if crc_calc != crc_stored:
        raise Exception("CRC mismatch")

    log_ok("CRC OK")

    params = {}

    lines = payload.decode("ascii").split("\r\n")

    for line in lines:
        if not line:
            continue

        if ":" not in line:
            log_err(f"Invalid line: {line}")
            continue

        key, value = line.split(":", 1)
        params[key] = value

    log_ok(f"Parsed {len(params)} parameters")

    return params


# ============================================
# EXTRACT FILESYSTEM
# ============================================
def extract_filesystem(fs_data, out_dir):
    log_step("EXTRACT FILESYSTEM")

    tmp_file = tempfile.NamedTemporaryFile(delete=False)
    tmp_file.write(fs_data)
    tmp_file.close()

    log_info(f"Temporary FS image: {tmp_file.name}")

    result = subprocess.run(
        ["python3", "../FS/mountlfs.py", tmp_file.name, out_dir],
        capture_output=True,
        text=True
    )

    print(result.stdout)

    if result.returncode != 0:
        log_err("mountlfs FAILED")
        print(result.stderr)
        os.unlink(tmp_file.name)
        return False

    log_ok("Filesystem extracted")

    os.unlink(tmp_file.name)
    return True


# ============================================
# WRITE SYSTEM PARAMS FILE
# ============================================
def write_system_params(params, out_dir):
    log_step("WRITE SYSTEM PARAMS")

    path = os.path.join(out_dir, "systemParams.txt")

    with open(path, "w") as f:
        for k, v in params.items():
            f.write(f"{k}:{v}\n")

    log_ok(f"Written: {path}")


# ============================================
# MAIN
# ============================================
def main():
    try:
        if len(sys.argv) < 3:
            print("Usage:")
            print("  python3 extractmi.py <image.bin> <output_dir>")
            return

        image_file = sys.argv[1]
        out_dir = sys.argv[2]

        log_step("INPUT VALIDATION")

        if not os.path.isfile(image_file):
            log_err(f"Image not found: {image_file}")
            return

        log_ok(f"Image: {image_file}")
        log_ok(f"Output dir: {out_dir}")

        # =============================
        log_step("READ IMAGE")

        with open(image_file, "rb") as f:
            data = f.read()

        log_info(f"Image size: {len(data)} B")

        if len(data) != TOTAL_SIZE:
            log_err("Invalid image size")
            return

        # =============================
        log_step("SPLIT IMAGE")

        system_data = data[:SYSTEM_SIZE]
        fs_data = data[SYSTEM_SIZE:]

        log_info(f"System region: {len(system_data)} B")
        log_info(f"FS region    : {len(fs_data)} B")

        # =============================
        params = parse_system_region(system_data)

        # =============================
        log_step("PREPARE OUTPUT DIR")

        if os.path.exists(out_dir):
            log_info(f"Removing existing directory: {out_dir}")
            shutil.rmtree(out_dir)

        os.makedirs(out_dir)
        os.makedirs(os.path.join(out_dir, "fs"))

        log_ok("Output directory ready")

        # =============================
        write_system_params(params, out_dir)

        # =============================
        success = extract_filesystem(fs_data, os.path.join(out_dir, "fs"))

        if not success:
            return

        log_ok("EXTRACTION COMPLETE")

    except Exception as e:
        log_err("UNHANDLED EXCEPTION")
        print(e)


if __name__ == "__main__":
    main()