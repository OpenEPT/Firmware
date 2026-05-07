#!/usr/bin/env python3
"""
@file    buildmi.py
@brief   Build full EEPROM image (system + LittleFS)
"""

import sys
import os
import struct
import subprocess
import tempfile
import shutil
import zlib
import random
from datetime import datetime

# ==== CONSTANTS ====
TOTAL_SIZE = 256 * 1024
SYSTEM_SIZE = 4096
BLOCK_SIZE = 256
BLOCK_COUNT = 1008

MAGIC = 0xA5A6A7A8


# ============================================
# GENERATORS
# ============================================
def generate_mac():
    return "02:%02X:%02X:%02X:%02X:%02X" % tuple(random.randint(0, 255) for _ in range(5))


def generate_serial(params):
    # minimal verzija (možeš proširiti)
    now = datetime.now()
    year = now.year % 1000
    month = now.month

    board = "EPP"
    extradata = "0450000"
    device_id = "%08d" % random.randint(1, 99999999)

    return f"OEPT{board}0{year:03d}{month:02d}{extradata}{device_id}"


# ============================================
# PARSE SYSTEM PARAMS
# ============================================
def parse_system_params(path):
    params = {}

    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            key, value = line.split(":", 1)

            if value == "<gen>":
                if key == "HW_SERIAL":
                    value = generate_serial(params)
                elif key == "MAC_ADDRESS":
                    value = generate_mac()

            params[key] = value

    return params


# ============================================
# BUILD SYSTEM REGION
# ============================================
def build_system_region(params):
    payload = ""

    for k, v in params.items():
        payload += f"{k}:{v}\r\n"

    payload_bytes = payload.encode("ascii")
    size = len(payload_bytes)

    crc = zlib.crc32(payload_bytes) & 0xFFFFFFFF

    header = struct.pack("<II", MAGIC, size)
    crc_bytes = struct.pack("<I", crc)

    region = header + payload_bytes + crc_bytes

    if len(region) > SYSTEM_SIZE:
        raise Exception("System region overflow")

    region += bytes(SYSTEM_SIZE - len(region))

    return region


# ============================================
# BUILD FS REGION
# ============================================
def build_fs_region(fs_root):
    tmp_file = tempfile.NamedTemporaryFile(delete=False)
    tmp_file.close()

    subprocess.run(
        ["python3", "../FS/buildlfs.py", tmp_file.name, fs_root],
        check=True
    )

    with open(tmp_file.name, "rb") as f:
        data = f.read()

    os.unlink(tmp_file.name)

    return data

def log_step(msg):
    print(f"\n[STEP] {msg}")

def log_info(msg):
    print(f"[INFO] {msg}")

def log_ok(msg):
    print(f"[ OK ] {msg}")

def log_err(msg):
    print(f"[FAIL] {msg}")
# ============================================
# MAIN
# ============================================
def main():
    try:
        if len(sys.argv) < 4:
            print("Usage:")
            print("  python3 buildmi.py <systemParams.txt> <fs_root> <output.bin>")
            return

        system_file = sys.argv[1]
        fs_root = sys.argv[2]
        output_file = sys.argv[3]

        log_step("INPUT VALIDATION")

        if not os.path.isfile(system_file):
            log_err(f"System file not found: {system_file}")
            return

        if not os.path.isdir(fs_root):
            log_err(f"FS root not found: {fs_root}")
            return

        log_ok(f"System file: {system_file}")
        log_ok(f"FS root: {fs_root}")
        log_ok(f"Output: {output_file}")

        # =============================
        log_step("PARSE SYSTEM PARAMS")
        params = parse_system_params(system_file)

        log_info("Resolved parameters:")
        for k, v in params.items():
            print(f"   {k:30} = {v}")

        # =============================
        log_step("BUILD SYSTEM REGION")
        system_region = build_system_region(params)

        log_ok(f"System region size: {len(system_region)} B")

        # =============================
        log_step("BUILD FILESYSTEM")

        tmp_file = tempfile.NamedTemporaryFile(delete=False)
        tmp_file.close()

        log_info(f"Temporary FS image: {tmp_file.name}")

        result = subprocess.run(
            ["python3", "../FS/buildlfs.py", tmp_file.name, fs_root],
            capture_output=True,
            text=True
        )

        print(result.stdout)

        if result.returncode != 0:
            log_err("buildlfs FAILED")
            print(result.stderr)
            return

        log_ok("buildlfs finished")

        with open(tmp_file.name, "rb") as f:
            fs_region = f.read()

        os.unlink(tmp_file.name)

        log_ok(f"Filesystem size: {len(fs_region)} B")

        expected_fs_size = BLOCK_SIZE * BLOCK_COUNT
        if len(fs_region) != expected_fs_size:
            log_err(f"FS size mismatch! Expected {expected_fs_size}, got {len(fs_region)}")
            return

        # =============================
        log_step("MERGE IMAGE")

        final = system_region + fs_region

        log_info(f"Final size: {len(final)} B (expected {TOTAL_SIZE})")

        if len(final) != TOTAL_SIZE:
            log_err("Final image size mismatch!")
            return

        # =============================
        log_step("WRITE OUTPUT")

        with open(output_file, "wb") as f:
            f.write(final)

        log_ok(f"Image created: {output_file}")

    except Exception as e:
        log_err("UNHANDLED EXCEPTION")
        print(e)
        
if __name__ == "__main__":
    main()