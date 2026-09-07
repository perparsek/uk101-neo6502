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
    host_recorder_start();      /* bandet spelar in fran start */

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

/* ---- bandspelaren ---------------------------------------------------- */

/* Bandet måste leva så länge det sitter i, så bufferten ägs här och inte i
 * maskinmodellen. */
static uint8_t *tape_buf;

int host_tape_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    long size;
    size_t got, i, n;
    uint8_t *raw;

    if (!f) {
        fprintf(stderr, "kan inte oppna %s\n", path);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fprintf(stderr, "%s ar tom\n", path);
        fclose(f);
        return 0;
    }
    raw = malloc((size_t)size);
    got = fread(raw, 1, (size_t)size, f);
    fclose(f);

    /* Normalisera radslut till CR: UK101 forvantar sig CR, och ett extra LF
     * hade blivit ett tecken for mycket i BASIC:ens inmatningsbuffert. */
    free(tape_buf);
    tape_buf = malloc(got ? got : 1);
    n = 0;
    for (i = 0; i < got; i++) {
        uint8_t ch = raw[i];
        if (ch == 0x0D) {
            tape_buf[n++] = 0x0D;
            if (i + 1 < got && raw[i + 1] == 0x0A) i++;   /* CRLF -> CR */
        } else if (ch == 0x0A) {
            tape_buf[n++] = 0x0D;                          /* LF -> CR   */
        } else {
            tape_buf[n++] = ch;
        }
    }
    free(raw);

    uk101_tape_insert(&host_mach, tape_buf, (uint32_t)n);
    printf("band i: %s, %lu tecken\n", path, (unsigned long)n);
    fflush(stdout);
    return 1;
}

void host_tape_eject(void)
{
    uk101_tape_eject(&host_mach);
    free(tape_buf);
    tape_buf = 0;
}

int host_load_program(const char *path)
{
    if (!host_tape_load(path)) return 0;

    host_type("LOAD");
    host_tap(UK101_KEY_RETURN, 0);

    /* Mata fram bandet. Det konsumeras i takt med att BASIC frågar efter
     * tecken, så det är klart när bandet är slut. Ett långt program tar lång
     * tid, så tiden får inte vara taket. Istället ges upp först när bandet
     * slutat röra sig, alltså när maskinen inte längre läser. */
    {
        uint32_t forra = host_mach.tape_pos;
        int stillastaende = 0;
        while (!uk101_tape_at_end(&host_mach) && stillastaende < 200) {
            host_run_cycles(MS(10));
            if (host_mach.tape_pos == forra) stillastaende++;
            else { forra = host_mach.tape_pos; stillastaende = 0; }
        }
    }

    if (!uk101_tape_at_end(&host_mach)) {
        fprintf(stderr, "bandet lastes inte klart, %lu av %lu tecken kvar\n",
                (unsigned long)(host_mach.tape_len - host_mach.tape_pos),
                (unsigned long)host_mach.tape_len);
        return 0;
    }

    host_run_cycles(MS(200));       /* sista raden hinner tokeniseras */
    host_tape_eject();

    host_reset();                   /* LOAD slapper inte inmatningen sjalv */
    host_run_cycles(MS(400));
    host_tap(UK101_KEY(4, 7), 0);   /* W = varmstart, programmet ligger kvar */
    host_run_cycles(MS(400));

    printf("program inlast, maskinen star vid OK\n");
    fflush(stdout);
    return 1;
}

/* ---- inspelning ------------------------------------------------------ */

static uint8_t *rec_buf;
static long rec_len, rec_cap;

static void rec_byte(void *user, uint8_t byte)
{
    (void)user;
    if (rec_len == rec_cap) {
        long ny = rec_cap ? rec_cap * 2 : 4096;
        uint8_t *p = realloc(rec_buf, (size_t)ny);
        if (!p) return;                     /* full buffert, tappa tecknet */
        rec_buf = p;
        rec_cap = ny;
    }
    rec_buf[rec_len++] = byte;
}

void host_recorder_start(void)
{
    host_mach.acia_tx = rec_byte;
    host_mach.acia_tx_user = 0;
}

long host_tape_recorded(void)
{
    return rec_len;
}

int host_tape_save(const char *path)
{
    FILE *f;
    long i;

    if (rec_len == 0) {
        fprintf(stderr, "inget inspelat. Skriv SAVE och sedan LIST forst.\n");
        return 0;
    }
    f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "kan inte skriva %s\n", path);
        return 0;
    }
    /* Inspelningen innehaller allt BASIC ekade ut, alltsa ocksa sjalva
     * LIST-kommandot och OK-prompterna. Bara rader som borjar med ett
     * radnummer skrivs, sa att filen gar att lasa in igen utan syntaxfel.
     * Radslut blir CRLF sa filen gar att oppna i en Windows-editor. */
    {
        char rad[256];
        size_t len = 0;
        long rader = 0;
        for (i = 0; i <= rec_len; i++) {
            int slut = (i == rec_len) || (rec_buf[i] == 0x0D);
            if (!slut) {
                uint8_t ch = rec_buf[i];
                if (ch >= 0x20 && ch < 0x7F && len < sizeof rad - 1)
                    rad[len++] = (char)ch;
                continue;
            }
            rad[len] = '\0';
            {
                char *p = rad;
                while (*p == ' ') p++;                  /* LIST inleder med blanksteg */
                if (*p >= '0' && *p <= '9') {
                    fputs(p, f);
                    fputs("\r\n", f);
                    rader++;
                }
            }
            len = 0;
        }
        fclose(f);
        printf("sparade %s, %ld rader ur %ld inspelade tecken\n",
               path, rader, rec_len);
        fflush(stdout);
        rec_len = 0;
        return rader > 0;
    }
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
    } else if (strncmp(cmd, "load:", 5) == 0) {
        return host_load_program(cmd + 5) ? 1 : -1;
    } else if (strncmp(cmd, "tape:", 5) == 0) {
        return host_tape_load(cmd + 5) ? 1 : -1;
    } else if (strncmp(cmd, "save:", 5) == 0) {
        return host_tape_save(cmd + 5) ? 1 : -1;
    } else if (strcmp(cmd, "eject") == 0) {
        host_tape_eject();
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
