#!/usr/bin/env python3
"""Extraherar ROM-innehall ur Grant Searles VHDL-ROM-filer till rena binarer."""
import re, sys, hashlib, pathlib

def extract(src: pathlib.Path, dst: pathlib.Path, expect: int) -> bytes:
    text = src.read_text(errors="replace")
    body = text[text.index(":="):]
    data = bytes(int(m, 16) for m in re.findall(r'x"([0-9A-Fa-f]{2})"', body))
    if len(data) != expect:
        raise SystemExit(f"{src.name}: fick {len(data)} bytes, vantade {expect}")
    dst.write_bytes(data)
    return data

if __name__ == "__main__":
    src_dir = pathlib.Path(r"C:\PON\uk101\src")
    out_dir = pathlib.Path(r"C:\PON\uk101-neo6502\roms")
    jobs = [("BasicRom.vhd", "basic.rom", 8192),
            ("CegmonRom.vhd", "cegmon.rom", 2048),
            ("CharRom.vhd", "chargen.rom", 2048)]
    for name, out, size in jobs:
        d = extract(src_dir / name, out_dir / out, size)
        print(f"{out:14s} {len(d):5d} bytes  md5={hashlib.md5(d).hexdigest()}  sha1={hashlib.sha1(d).hexdigest()}")
