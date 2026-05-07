#!/usr/bin/env python3
"""
@file    fsAnalyzer.py
@brief   Load LittleFS image, print tree, and clone filesystem
@author  Haris
@date    2026
"""

from littlefs import LittleFS
import sys
import os
import shutil


# ==== CONFIGURATION (MUST MATCH MCU SETTINGS) ====
BLOCK_SIZE = 256
BLOCK_COUNT = 1008


# ============================================
# PRINT FILESYSTEM TREE
# ============================================
def print_tree(fs, path="/", indent=0):
    try:
        entries = fs.listdir(path)
    except Exception as e:
        print(" " * indent + f"[ERROR] Cannot access {path}: {e}")
        return

    for entry in entries:
        full_path = f"{path.rstrip('/')}/{entry}"

        try:
            fs.listdir(full_path)
            print(" " * indent + f"[DIR ] {full_path}")
            print_tree(fs, full_path, indent + 4)
        except:
            print(" " * indent + f"[FILE] {full_path}")


# ============================================
# CLONE FILESYSTEM TO LOCAL DIRECTORY
# ============================================
def clone_filesystem(fs, path, out_root):
    try:
        entries = fs.listdir(path)
    except:
        return

    for entry in entries:
        fs_path = f"{path.rstrip('/')}/{entry}"
        local_path = os.path.join(out_root, fs_path.lstrip("/"))

        try:
            fs.listdir(fs_path)
            os.makedirs(local_path, exist_ok=True)
            print(f"[MKDIR] {local_path}")
            clone_filesystem(fs, fs_path, out_root)

        except:
            try:
                with fs.open(fs_path, "rb") as f:
                    data = f.read()

                os.makedirs(os.path.dirname(local_path), exist_ok=True)

                with open(local_path, "wb") as out:
                    out.write(data)

                print(f"[FILE ] {local_path} ({len(data)} bytes)")

            except Exception as e:
                print(f"[ERROR] cloning {fs_path}: {e}")


# ============================================
# PREPARE OUTPUT DIRECTORY
# ============================================
def prepare_output_dir(out_dir):
    full_path = os.path.abspath(out_dir)

    if os.path.exists(full_path):
        print(f"[INFO] Removing existing directory: {full_path}")
        shutil.rmtree(full_path)

    os.makedirs(full_path, exist_ok=True)
    print(f"[INFO] Created directory: {full_path}")

    return full_path


# ============================================
# MAIN ENTRY POINT
# ============================================
def main():
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python3 fsAnalyzer.py <fs.bin> [output_dir]")
        return

    bin_file = sys.argv[1]
    out_dir = sys.argv[2] if len(sys.argv) > 2 else "mcufs"

    fs = LittleFS(
        block_size=BLOCK_SIZE,
        block_count=BLOCK_COUNT,
        read_size=1,
        prog_size=256,
        cache_size=256,
        lookahead_size=256,
        block_cycles=100
    )

    with open(bin_file, "rb") as f:
        raw = f.read()

    print(f"[INFO] Loaded image size: {len(raw)} bytes")

    fs.context.buffer = bytearray(raw)

    try:
        fs.mount()
    except Exception as e:
        print(f"[ERROR] Mount failed: {e}")
        return

    print("\n=== FILESYSTEM TREE ===")
    print_tree(fs, "/")

    out_dir_full = prepare_output_dir(out_dir)

    print(f"\n=== CLONING FILESYSTEM TO {out_dir_full} ===")
    clone_filesystem(fs, "/", out_dir_full)

    print("\n[DONE]")


if __name__ == "__main__":
    main()