"""Convert raw PCM file to a C header with a byte array.
Usage: python raw_to_header.py <input.raw> <output.h> <symbol_name>
"""
import sys
from pathlib import Path

def main():
    if len(sys.argv) != 4:
        print(__doc__)
        sys.exit(1)
    in_path, out_path, symbol = sys.argv[1], sys.argv[2], sys.argv[3]
    data = Path(in_path).read_bytes()
    lines = [
        "// Auto-generated from " + Path(in_path).name,
        f"#pragma once",
        f"#include <stdint.h>",
        f"#define {symbol}_len {len(data)}",
        f"const uint8_t {symbol}[] = {{",
    ]
    per_line = 16
    for i in range(0, len(data), per_line):
        chunk = data[i:i+per_line]
        lines.append(" " + ", ".join(f"0x{b:02X}" for b in chunk) + ",")
    lines.append("};")
    Path(out_path).write_text("\n".join(lines))
    print(f"wrote {out_path}: {len(data)} bytes -> {symbol}")

if __name__ == "__main__":
    main()
