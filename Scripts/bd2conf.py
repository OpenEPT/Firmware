import struct

INPUT_FILE = "configIn.bin"
OUTPUT_FILE = "configRecovered.txt"

MAGIC_EXPECTED = 0xA5A6A7A8
HEADER_SIZE = 32

def read_header(data):
    # Magic
    magic = struct.unpack_from("<I", data, 0)[0]

    if magic != MAGIC_EXPECTED:
        raise ValueError(f"Invalid MAGIC: 0x{magic:08X}")

    # Content size
    size = struct.unpack_from("<I", data, 5)[0]

    return size

def extract_content(data, size):
    start = HEADER_SIZE
    end = start + size

    content = data[start:end]

    return content

def parse_content(content_bytes):
    try:
        text = content_bytes.decode("ascii")
    except:
        raise ValueError("Content is not valid ASCII")

    lines = text.split("\r\n")

    # ukloni prazne linije
    lines = [l for l in lines if l.strip()]

    return lines

def main():
    with open(INPUT_FILE, "rb") as f:
        data = f.read()

    if len(data) < HEADER_SIZE:
        raise ValueError("File too small")

    size = read_header(data)

    content = extract_content(data, size)
    lines = parse_content(content)

    with open(OUTPUT_FILE, "w") as f:
        for line in lines:
            f.write(line + "\n")

    print(f"[OK] Generated {OUTPUT_FILE}")
    print(f"Recovered {len(lines)} parameters")

if __name__ == "__main__":
    main()