import struct

INPUT_FILE = "paramTemplate.txt"
OUTPUT_FILE = "configOut.bin"

MAGIC = 0xA5A6A7A8
HEADER_SIZE = 32

# ===== LIMITS =====
MAX_KEY_LEN = 32
MAX_VALUE_LEN = 64

def validate_line(line):
    if ":" not in line:
        raise ValueError(f"Invalid line (no separator): {line}")

    key, value = line.split(":", 1)

    key = key.strip()
    value = value.strip()

    if len(key) == 0 or len(value) == 0:
        raise ValueError(f"Empty key or value: {line}")

    if len(key) > MAX_KEY_LEN:
        raise ValueError(f"Key too long ({len(key)} > {MAX_KEY_LEN}): {key}")

    if len(value) > MAX_VALUE_LEN:
        raise ValueError(f"Value too long ({len(value)} > {MAX_VALUE_LEN}): {value}")

    return key, value

def read_txt_file(path):
    lines = []
    with open(path, "r") as f:
        for idx, line in enumerate(f, 1):
            line = line.strip()

            if not line:
                continue

            try:
                key, value = validate_line(line)
                lines.append(f"{key}:{value}")
            except Exception as e:
                raise ValueError(f"[Line {idx}] {e}")

    return lines

def build_content(lines):
    content = ""
    for line in lines:
        content += line + "\r\n"
    return content.encode("ascii")

def build_header(content_size):
    header = bytearray(HEADER_SIZE)

    struct.pack_into("<I", header, 0, MAGIC)
    header[4] = 0x20
    struct.pack_into("<I", header, 5, content_size)
    header[9] = 0x20

    return header

def main():
    lines = read_txt_file(INPUT_FILE)
    content = build_content(lines)

    header = build_header(len(content))

    with open(OUTPUT_FILE, "wb") as f:
        f.write(header)
        f.write(content)

    print(f"[OK] Generated {OUTPUT_FILE}")
    print(f"Content size: {len(content)} bytes")

if __name__ == "__main__":
    main()