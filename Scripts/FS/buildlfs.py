#!/usr/bin/env python3
"""
@file    buildlfs.py
@brief   Build LittleFS image from local directory or create empty filesystem
@author  Haris
@date    2026
"""

from littlefs import LittleFS
import sys
import os


# ==== CONFIGURATION (MUST MATCH MCU) ====
BLOCK_SIZE = 256
BLOCK_COUNT = 1008


def copy_to_lfs(fs, host_path, lfs_path="/"):
    for entry in os.listdir(host_path):
        host_entry = os.path.join(host_path, entry)
        lfs_entry = f"{lfs_path.rstrip('/')}/{entry}"

        if os.path.isdir(host_entry):
            try:
                fs.mkdir(lfs_entry)
                print(f"[MKDIR] {lfs_entry}")
            except Exception:
                pass

            copy_to_lfs(fs, host_entry, lfs_entry)

        else:
            try:
                with open(host_entry, "rb") as f:
                    data = f.read()

                with fs.open(lfs_entry, "wb") as f:
                    f.write(data)

                print(f"[FILE ] {lfs_entry} ({len(data)} bytes)")

            except Exception as e:
                print(f"[ERROR] {host_entry}: {e}")


def main():
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python3 buildlfs.py <output.bin>")
        print("  python3 buildlfs.py <output.bin> <input_folder>")
        return

    output_file = sys.argv[1]
    input_dir = None

    if len(sys.argv) >= 3:
        input_dir = sys.argv[2]

        if not os.path.isdir(input_dir):
            print(f"[ERROR] Invalid input directory: {input_dir}")
            return

    total_size = BLOCK_SIZE * BLOCK_COUNT
    buffer = bytearray([0xFF] * total_size)

    fs = LittleFS(
        block_size=BLOCK_SIZE,
        block_count=BLOCK_COUNT,
        read_size=1,
        prog_size=256,
        cache_size=256,
        lookahead_size=256,
        mount=False
    )

    fs.context.buffer = buffer

    # Format filesystem
    try:
        fs.format()
        print("[INFO] Filesystem formatted")
    except Exception as e:
        print(f"[ERROR] format failed: {e}")
        return

    # Mount filesystem
    try:
        fs.mount()
    except Exception as e:
        print(f"[ERROR] mount failed: {e}")
        return

    # Optional copy
    if input_dir is not None:
        print(f"[INFO] Copying data from: {input_dir}")
        copy_to_lfs(fs, input_dir, "/")
    else:
        print("[INFO] Creating empty filesystem")

    # Flush
    try:
        fs.unmount()
    except:
        pass

    # Save image
    with open(output_file, "wb") as f:
        f.write(buffer)

    print(f"[DONE] Image created: {output_file}")
    print(f"[INFO] Size: {len(buffer)} bytes")


if __name__ == "__main__":
    main()