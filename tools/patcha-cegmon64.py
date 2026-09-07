#!/usr/bin/env python3
"""Gor en 64-kolumners CEGMON av den vanliga.

    python tools\\patcha-cegmon64.py   -> roms64\\cegmon.rom

Originalet visar 48 tecken per rad, placerade med tolv kolumners marginal
inne i den 64 byte langa raden. Marginalen fanns for att en TV klippte bort
kanterna. Pa HDMI finns ingen overscan, sa de tolv kolumnerna ar bara bortkastade.

CEGMON haller sin skarmgeometri i RAM pa $0222-$0226 och kopierar dit den fran
en mall i ROM vid start. Det ar mallen som patchas har, sa geometrin galler
redan vid uppstart och inga POKE behovs.

    $0222     radbredd minus 1        47  ->  63
    $0223/24  fonstrets start      $D08C  ->  $D000
    $0225/26  nedersta radens start $D3CC ->  $D3C0

Rullningsrutinen som ocksa kopieras dit har fonsterstarten inbakad som
operand i tva instruktioner, sa de foljer med.

Radsteget i minnet ar och forblir 64 byte. Program som pokar skarmen raknar
pa det steget och fortsatter darfor fungera. Det ar hela poangen med att
stanna pa 64 och inte ga till 80.
"""
import pathlib
import shutil

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "roms"
DST = ROOT / "roms64"

# offset i cegmon.rom -> (vantat varde, nytt varde, vad det ar)
PATCHAR = [
    (956, 0x2F, 0x3F, "radbredd minus 1: 47 -> 63"),
    (957, 0x8C, 0x00, "fonsterstart lagbyte: $D08C -> $D000"),
    (959, 0xCC, 0xC0, "nedersta raden lagbyte: $D3CC -> $D3C0"),
    (962, 0x8C, 0x00, "rullning, kallans lagbyte"),
    (965, 0x8C, 0x00, "rullning, malets lagbyte"),
]


def main():
    rom = bytearray((SRC / "cegmon.rom").read_bytes())
    if len(rom) != 2048:
        raise SystemExit("cegmon.rom ar inte 2048 byte")

    for offset, vantat, nytt, vad in PATCHAR:
        if rom[offset] != vantat:
            raise SystemExit(
                "offset %d innehaller $%02X, vantade $%02X. Fel ROM-version?"
                % (offset, rom[offset], vantat))
        rom[offset] = nytt
        print("  %4d  $%02X -> $%02X   %s" % (offset, vantat, nytt, vad))

    DST.mkdir(exist_ok=True)
    (DST / "cegmon.rom").write_bytes(bytes(rom))
    for namn in ("basic.rom", "chargen.rom"):
        shutil.copyfile(SRC / namn, DST / namn)
    print("\nskrev %s" % (DST / "cegmon.rom"))
    print("Kor med:  uk101gui.exe --roms ..\\roms64")


if __name__ == "__main__":
    main()
