#!/usr/bin/env python3
"""Bakar ROM-binarerna till en C-header for pico-firmwaren.

Kor: python tools\\rom2h.py   -> src\\uk101_roms.h
"""
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
ROMS = ROOT / "roms"
OUT = ROOT / "src" / "uk101_roms.h"

TABLE = [
    ("uk101_rom_basic", "basic.rom", 8192, "8 KB Microsoft BASIC, laddas pa $A000"),
    ("uk101_rom_cegmon", "cegmon.rom", 2048, "2 KB CEGMON-monitor, laddas pa $F800"),
    ("uk101_rom_chargen", "chargen.rom", 2048, "teckengenerator, 256 tecken a 8 rader"),
]


def emit(fh, name, data, comment):
    fh.write(f"\n/* {comment} */\n")
    fh.write(f"static const uint8_t {name}[{len(data)}] = {{\n")
    for i in range(0, len(data), 16):
        row = ", ".join(f"0x{b:02X}" for b in data[i:i + 16])
        fh.write(f"    {row},\n")
    fh.write("};\n")


def main():
    with OUT.open("w", encoding="utf-8", newline="\n") as fh:
        fh.write("/* uk101_roms.h - GENERERAD FIL, andra inte for hand.\n"
                 " * Skapas av tools/rom2h.py ur roms/*.rom.\n"
                 " *\n"
                 " * ROM-innehallet ar upphovsrattsskyddat av sina respektive agare\n"
                 " * (Microsoft BASIC 1979, CEGMON 1980) och foljer med har pa samma\n"
                 " * grund som i ovriga UK101-bevarandeprojekt.\n"
                 " */\n"
                 "#ifndef UK101_ROMS_H\n#define UK101_ROMS_H\n\n#include <stdint.h>\n")
        for name, fn, size, comment in TABLE:
            data = (ROMS / fn).read_bytes()
            if len(data) != size:
                raise SystemExit(f"{fn}: {len(data)} byte, vantade {size}")
            emit(fh, name, data, comment)
        fh.write("\n#endif /* UK101_ROMS_H */\n")
    print(f"skrev {OUT} ({OUT.stat().st_size} byte)")


if __name__ == "__main__":
    main()
