#!/usr/bin/env python3
"""
@file    buildsys.py
@brief   Build EEPROM system image from key:value system parameters.
"""

import sys
import os
import struct
import zlib
import random
from datetime import datetime

MAGIC = 0xA5A6A7A8
HEADER_SIZE = 8
CRC_SIZE = 4
PADDING_VALUE = 0x00

def generate_mac():
    return "02:%02X:%02X:%02X:%02X:%02X" % tuple(random.randint(0, 255) for _ in range(5))

def generate_serial(params):
    now = datetime.now()
    year = now.year % 1000
    month = now.month
    board = "EPP"
    extradata = "0450000"
    device_id = "%08d" % random.randint(1, 99999999)
    return f"OEPT{board}0{year:03d}{month:02d}{extradata}{device_id}"

GENERATORS = {
    "HW_SERIAL": generate_serial,
    "MAC_ADDRESS": lambda params: generate_mac(),
}

def log_step(msg): print(f"\n[STEP] {msg}")
def log_info(msg): print(f"[INFO] {msg}")
def log_ok(msg): print(f"[ OK ] {msg}")
def log_err(msg): print(f"[FAIL] {msg}")

def parse_system_params(path):
    params = {}
    with open(path, "r", encoding="ascii") as f:
        for line_number, line in enumerate(f, start=1):
            line = line.strip()
            if not line:
                continue
            if ":" not in line:
                raise ValueError(f"Invalid parameter at line {line_number}: missing ':'")
            key, value = line.split(":", 1)
            key = key.strip()
            value = value.strip()
            if not key:
                raise ValueError(f"Invalid parameter at line {line_number}: empty key")
            if key in params:
                raise ValueError(f"Duplicate parameter '{key}' at line {line_number}")
            if value == "<gen>":
                generator = GENERATORS.get(key)
                if generator is None:
                    raise ValueError(f"No generator defined for parameter '{key}'")
                value = generator(params)
            params[key] = value
    return params

def build_system_image(params, image_size):
    payload = "".join(f"{key}:{value}\r\n" for key, value in params.items())
    payload_bytes = payload.encode("ascii")
    payload_size = len(payload_bytes)
    required_size = HEADER_SIZE + payload_size + CRC_SIZE
    if required_size > image_size:
        raise ValueError(
            "System image overflow\n"
            f"       Image size   : {image_size} B\n"
            f"       Required     : {required_size} B\n"
            f"       Exceeded by  : {required_size - image_size} B"
        )
    crc = zlib.crc32(payload_bytes) & 0xFFFFFFFF
    header = struct.pack("<II", MAGIC, payload_size)
    crc_bytes = struct.pack("<I", crc)
    image = header + payload_bytes + crc_bytes
    image += bytes([PADDING_VALUE]) * (image_size - len(image))
    return image, payload_size, crc

def main():
    try:
        if len(sys.argv) != 4:
            print("Usage:")
            print("  python3 buildsys.py <systemParams.txt> <image_size> <output.bin>")
            print("\nExample:")
            print("  python3 buildsys.py systemParams.txt 128 system.bin")
            return 1
        system_file = sys.argv[1]
        output_file = sys.argv[3]
        try:
            image_size = int(sys.argv[2], 0)
        except ValueError:
            log_err(f"Invalid image size: {sys.argv[2]}")
            return 1
        log_step("INPUT VALIDATION")
        if not os.path.isfile(system_file):
            log_err(f"System file not found: {system_file}")
            return 1
        if image_size <= 0:
            log_err("Image size must be greater than zero")
            return 1
        log_ok(f"System file: {system_file}")
        log_ok(f"Image size: {image_size} B")
        log_ok(f"Output: {output_file}")
        log_step("PARSE SYSTEM PARAMS")
        params = parse_system_params(system_file)
        if not params:
            log_err("No system parameters found")
            return 1
        log_info("Resolved parameters:")
        for key, value in params.items():
            print(f"   {key:30} = {value}")
        log_step("BUILD SYSTEM IMAGE")
        image, payload_size, crc = build_system_image(params, image_size)
        log_info(f"Payload size: {payload_size} B")
        log_info(f"CRC32: 0x{crc:08X}")
        log_info(f"Final image size: {len(image)} B")
        log_step("WRITE OUTPUT")
        with open(output_file, "wb") as f:
            f.write(image)
        log_ok(f"Image created: {output_file}")
        return 0
    except Exception as e:
        log_err("UNHANDLED EXCEPTION")
        print(e)
        return 1

if __name__ == "__main__":
    sys.exit(main())