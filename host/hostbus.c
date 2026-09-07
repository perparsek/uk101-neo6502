/* hostbus.c — se hostbus.h */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHIPS_IMPL
#include "vendor/m6502.h"

#include "hostbus.h"
#include "uk101_keys.h"

uk101_t host_mach;

static m6502_t  cpu;
static uint64_t pins;

const host_key_t host_keymap[] = {
    { UK101_KEY(0, 0), "SHIFTLOCK" }, { UK101_KEY(0, 1), "RSHIFT" },
    { UK101_KEY(0, 2), "LSHIFT" },    { UK101_KEY(0, 6), "CTRL" },
    { UK101_KEY(1, 7), "Q" }, { UK101_KEY(1, 6), "A" }, { UK101_KEY(1, 5), "Z" },
    { UK101_KEY(1, 4), "SPACE" }, { UK101_KEY(1, 3), "/" },
    { UK101_KEY(1, 2), ";" }, { UK101_KEY(1, 1), "P" },
    { UK101_KEY(2, 7), "X" }, { UK101_KEY(2, 6), "C" }, { UK101_KEY(2, 5), "V" },
    { UK101_KEY(2, 4), "B" }, { UK101_KEY(2, 3), "N" }, { UK101_KEY(2, 2), "M" },
    { UK101_KEY(2, 1), "," },
    { UK101_KEY(3, 7), "S" }, { UK101_KEY(3, 6), "D" }, { UK101_KEY(3, 5), "F" },
    { UK101_KEY(3, 4), "G" }, { UK101_KEY(3, 3), "H" }, { UK101_KEY(3, 2), "J" },
    { UK101_KEY(3, 1), "K" },
    { UK101_KEY(4, 7), "W" }, { UK101_KEY(4, 6), "E" }, { UK101_KEY(4, 5), "R" },
    { UK101_KEY(4, 4), "T" }, { UK101_KEY(4, 3), "Y" }, { UK101_KEY(4, 2), "U" },
    { UK101_KEY(4, 1), "I" },
    { UK101_KEY(5, 7), "." }, { UK101_KEY(5, 6), "L" }, { UK101_KEY(5, 5), "O" },
    { UK101_KEY(5, 4), "[" }, { UK101_KEY(5, 3), "RETURN" },
    { UK101_KEY(6, 7), "8" }, { UK101_KEY(6, 6), "9" }, { UK101_KEY(6, 5), "0" },
    { UK101_KEY(6, 4), "-" }, { UK101_KEY(6, 3), "=" },
    { UK101_KEY(6, 2), "RUBOUT" }, { UK101_KEY(6, 1), "'" },
    { UK101_KEY(7, 7), "1" }, { UK101_KEY(7, 6), "2" }, { UK101_KEY(7, 5), "3" },
    { UK101_KEY(7, 4), "4" }, { UK101_KEY(7, 3), "5" }, { UK101_KEY(7, 2), "6" },
    { UK101_KEY(7, 1), "7" },
};
const int host_keymap_n = (int)(sizeof host_keymap / sizeof host_keymap[0]);

int host_key_by_name(const char *name, uint8_t *out)
{
    int i;
    for (i = 0; i < host_keymap_n; i++)
        if (strcmp(host_keymap[i].name, name) == 0) {
            *out = host_keymap[i].pos;
            return 1;
        }
    return 0;
}

/* ---- ROM-inläsning --------------------------------------------------- */

static uint8_t *load_rom(const char *dir, const char *name, long expect)
{
    char path[600];
    FILE *f;
    uint8_t *buf;
    long n;

    snprintf(path, sizeof path, "%s\\%s", dir, name);
    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "kan inte oppna %s\n", path);
        exit(1);
    }
    buf = malloc((size_t)expect);
    n = (long)fread(buf, 1, (size_t)expect, f);
    fclose(f);
    if (n != expect) {
        fprintf(stderr, "%s: laste %ld byte, vantade %ld\n", path, n, expect);
        exit(1);
    }
    return buf;
}

const uint8_t *host_boot(const char *rom_dir, uint32_t ram_size)
{
    uint8_t *basic   = load_rom(rom_dir, "basic.rom", 8192);
    uint8_t *cegmon  = load_rom(rom_dir, "cegmon.rom", 2048);
    uint8_t *chargen = load_rom(rom_dir, "chargen.rom", 2048);

    uk101_init(&host_mach, basic, cegmon, ram_size);
    pins = m6502_init(&cpu, &(m6502_desc_t){ 0 });

    free(basic);
    free(cegmon);
    return chargen;
}

void host_reset(void)
{
    uk101_keys_clear(&host_mach);
    pins |= M6502_RES;
    host_run_cycles(16);
}

/* ---- bussloop -------------------------------------------------------- */

void host_run_cycles(unsigned long n)
{
    unsigned long i;
    for (i = 0; i < n; i++) {
        pins = m6502_tick(&cpu, pins);
        uint16_t addr = M6502_GET_ADDR(pins);
        if (pins & M6502_RW) {
            uint8_t d = uk101_read(&host_mach, addr);
            M6502_SET_DATA(pins, d);
        } else {
            uk101_write(&host_mach, addr, M6502_GET_DATA(pins));
        }
    }
}

/* ---- tangentbord ----------------------------------------------------- */

void host_tap(uint8_t pos, int shift)
{
    if (shift) uk101_key_set(&host_mach, UK101_KEY_LSHIFT, 1);
    uk101_key_set(&host_mach, pos, 1);
    host_run_cycles(MS(40));
    uk101_key_set(&host_mach, pos, 0);
    if (shift) uk101_key_set(&host_mach, UK101_KEY_LSHIFT, 0);
    host_run_cycles(MS(40));
}

void host_cold_start_basic(void)
{
    host_run_cycles(MS(500));
    host_tap(UK101_KEY(2, 6), 0);          /* C = kallstart */
    host_run_cycles(MS(300));
    host_tap(UK101_KEY_RETURN, 0);         /* MEMORY SIZE?    */
    host_run_cycles(MS(300));
    host_tap(UK101_KEY_RETURN, 0);         /* TERMINAL WIDTH? */
    host_run_cycles(MS(1500));
}

int host_command(const char *cmd)
{
    if (strncmp(cmd, "wait:", 5) == 0) {
        host_run_cycles(MS(strtoul(cmd + 5, NULL, 0)));
    } else if (strncmp(cmd, "type:", 5) == 0) {
        return host_type(cmd + 5) ? 1 : -1;
    } else if (strncmp(cmd, "key:", 4) == 0 || strncmp(cmd, "shift:", 6) == 0) {
        int shift = (cmd[0] == 's');
        const char *name = shift ? cmd + 6 : cmd + 4;
        uint8_t pos;
        if (!host_key_by_name(name, &pos)) {
            fprintf(stderr, "okand tangent: %s\n", name);
            return -1;
        }
        host_tap(pos, shift);
    } else if (strcmp(cmd, "boot") == 0) {
        host_cold_start_basic();
    } else if (strcmp(cmd, "reset") == 0) {
        host_reset();
    } else {
        return 0;
    }
    return 1;
}

int host_type(const char *s)
{
    for (; *s; s++) {
        unsigned char ch = (unsigned char)*s;
        uint8_t kc;
        if (ch == '~') { host_tap(UK101_KEY_RETURN, 0); continue; }
        if (ch > 127) {
            fprintf(stderr, "kan inte skrivas: %c\n", ch);
            return 0;
        }
        kc = uk101_ascii_key[ch];
        if (!UK101_KC_IS_VALID(kc)) {
            fprintf(stderr, "tecknet '%c' ($%02X) finns inte pa "
                            "UK101-tangentbordet\n", ch, ch);
            return 0;
        }
        host_tap(UK101_KC_POS(kc), UK101_KC_IS_SHIFT(kc));
    }
    return 1;
}
