"""Convert a raw RGBA image to a C header with RGB565 color array + 1-bit alpha mask.
Usage: python rgba_to_header.py <input.rgba> <width> <height> <output.h> <symbol>
"""
import sys
from pathlib import Path

def main():
    if len(sys.argv) != 6:
        print(__doc__)
        sys.exit(1)
    in_path, W, H, out_path, symbol = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4], sys.argv[5]
    data = Path(in_path).read_bytes()
    if len(data) != W * H * 4:
        print(f"expected {W*H*4} bytes, got {len(data)}")
        sys.exit(1)

    colors = []
    mask_bits = []
    for i in range(W * H):
        r, g, b, a = data[i*4:i*4+4]
        rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        colors.append(rgb565)
        mask_bits.append(1 if a >= 128 else 0)

    # Pack 8 bits per byte, MSB-first.
    mask_bytes = []
    for i in range(0, W * H, 8):
        byte = 0
        for b in range(8):
            if i + b < W * H and mask_bits[i + b]:
                byte |= (1 << (7 - b))
        mask_bytes.append(byte)

    out = [
        f"// Auto-generated from {Path(in_path).name}",
        "#pragma once",
        "#include <stdint.h>",
        f"#define {symbol}_W {W}",
        f"#define {symbol}_H {H}",
        f"const uint16_t {symbol}_rgb565[{W*H}] = {{",
    ]
    for i in range(0, len(colors), 12):
        chunk = colors[i:i+12]
        out.append("  " + ", ".join(f"0x{c:04X}" for c in chunk) + ",")
    out.append("};")
    out.append(f"const uint8_t {symbol}_mask[{len(mask_bytes)}] = {{")
    for i in range(0, len(mask_bytes), 16):
        chunk = mask_bytes[i:i+16]
        out.append("  " + ", ".join(f"0x{c:02X}" for c in chunk) + ",")
    out.append("};")

    Path(out_path).write_text("\n".join(out))
    print(f"wrote {out_path}: {W}x{H}, {len(colors)*2}B colors + {len(mask_bytes)}B mask")

if __name__ == "__main__":
    main()
