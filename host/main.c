/* main.c — värdharness för UK101-modellen.
 *
 * Kör uk101.c mot en emulerad 6502 istället för Neo6502:ans riktiga 65C02, så
 * att maskinmodellen kan verifieras på PC utan hårdvara. Bussloopen nedan är
 * medvetet identisk i form med pico-firmwarens: en adress in, en byte ut.
 *
 * Byggs med host/Makefile. Kommandon anges på kommandoraden, se usage().
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHIPS_IMPL
#include "../src/vendor/m6502.h"
#include "../src/uk101.h"
#include "../src/uk101_keys.h"

#define CPU_HZ        1000000u          /* originalets klockfrekvens */
#define MS(n)         ((n) * (CPU_HZ / 1000u))

static uk101_t   mach;
static m6502_t   cpu;
static uint64_t  pins;

/* ---- bussloop -------------------------------------------------------- */

static void run_cycles(unsigned long n)
{
    unsigned long i;
    for (i = 0; i < n; i++) {
        pins = m6502_tick(&cpu, pins);
        uint16_t addr = M6502_GET_ADDR(pins);
        if (pins & M6502_RW) {
            uint8_t d = uk101_read(&mach, addr);
            M6502_SET_DATA(pins, d);
        } else {
            uk101_write(&mach, addr, M6502_GET_DATA(pins));
        }
    }
}

/* ---- tangentbord ----------------------------------------------------- */

/* Tangentmatrisen som den sitter på originalet. Positionerna är hämtade ur
 * Grant Searles UK101keyboard.vhd, som är den uppsättning ROM-bilderna här
 * faktiskt bootar med. */
static const struct { uint8_t pos; const char *name; } keymap[] = {
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
#define KEYMAP_N ((int)(sizeof keymap / sizeof keymap[0]))

/* Tryck och släpp med realistiska tider. Monitorn avstuderar, så tangenten
 * måste ligga nere några millisekunder för att räknas. */
static void tap(uint8_t pos, int shift)
{
    if (shift) uk101_key_set(&mach, UK101_KEY_LSHIFT, 1);
    uk101_key_set(&mach, pos, 1);
    run_cycles(MS(40));
    uk101_key_set(&mach, pos, 0);
    if (shift) uk101_key_set(&mach, UK101_KEY_LSHIFT, 0);
    run_cycles(MS(40));
}

/* Skriver en textrad via tangentmatrisen, som en människa hade gjort. */
static int type_text(const char *s)
{
    for (; *s; s++) {
        unsigned char ch = (unsigned char)*s;
        uint8_t kc;
        if (ch == '~') { tap(UK101_KEY_RETURN, 0); continue; }  /* ~ = RETURN */
        if (ch > 127) { fprintf(stderr, "kan inte skrivas: %c\n", ch); return 0; }
        kc = uk101_ascii_key[ch];
        if (!UK101_KC_IS_VALID(kc)) {
            fprintf(stderr, "tecknet '%c' ($%02X) finns inte pa UK101-tangentbordet\n",
                    ch, ch);
            return 0;
        }
        tap(UK101_KC_POS(kc), UK101_KC_IS_SHIFT(kc));
    }
    return 1;
}

static int key_by_name(const char *name, uint8_t *out)
{
    int i;
    for (i = 0; i < KEYMAP_N; i++)
        if (strcmp(keymap[i].name, name) == 0) { *out = keymap[i].pos; return 1; }
    return 0;
}

/* ---- skärm ----------------------------------------------------------- */

static void dump_screen(const char *title)
{
    int r, c;
    printf("\n%s\n    +", title);
    for (c = 0; c < UK101_SCREEN_COLS; c++) putchar('-');
    printf("+\n");
    for (r = 0; r < UK101_SCREEN_ROWS; r++) {
        printf("%2d  |", r);
        for (c = 0; c < UK101_SCREEN_COLS; c++) {
            uint8_t ch = uk101_screen_char(&mach, r, c);
            putchar((ch >= 0x20 && ch < 0x7F) ? (char)ch : '.');
        }
        printf("|\n");
    }
    printf("    +");
    for (c = 0; c < UK101_SCREEN_COLS; c++) putchar('-');
    printf("+\n");
}

/* ---- kalibrering av teckenuppsättningen ------------------------------ */

/* Vilket tecken en fysisk tangent ger, med och utan shift, är monitorns och
 * BASIC:ens sak, inte hårdvarans. Istället för att gissa layouten trycker vi
 * varje position vid BASIC-prompten och läser av vad som hamnar på skärmen. */
/* Markören ritas som understreck. Den cellen är där nästa tecken hamnar. */
static int cursor_cell(void)
{
    int j;
    for (j = 0; j < 1024; j++)
        if (mach.mem[UK101_SCREEN_BASE + j] == 0x5F) return j;
    return -1;
}

static int measure(uint8_t pos, int shift)
{
    int cell = cursor_cell();
    uint8_t got;
    if (cell < 0) return -1;
    tap(pos, shift);
    got = mach.mem[UK101_SCREEN_BASE + cell];
    tap(UK101_KEY_RUBOUT, 0);          /* radera, nästa mätning startar rent */
    return (got == 0x5F) ? -1 : (int)got;  /* markören kvar = tangenten gav inget */
}

static void show(int code)
{
    if (code < 0)            printf("%-9s", "-");
    else if (code == 0x20)   printf("%02X %-6s", code, "SP");
    else if (code < 0x21)    printf("%02X %-6s", code, "ctl");
    else                     printf("%02X %-6c", code, (char)code);
}

static void calibrate(void)
{
    int i;

    printf("\nkalibrering: fysisk tangent -> tecken\n");
    printf("%-10s %-6s %-6s\n", "tangent", "osk.", "shift");
    for (i = 0; i < KEYMAP_N; i++) {
        int plain, shifted;
        if (keymap[i].pos == UK101_KEY_SHIFTLOCK ||
            keymap[i].pos == UK101_KEY_LSHIFT ||
            keymap[i].pos == UK101_KEY_RSHIFT ||
            keymap[i].pos == UK101_KEY_CTRL ||
            keymap[i].pos == UK101_KEY_RETURN ||
            keymap[i].pos == UK101_KEY_RUBOUT) continue;

        plain   = measure(keymap[i].pos, 0);
        shifted = measure(keymap[i].pos, 1);
        printf("%-10s ", keymap[i].name);
        show(plain);
        show(shifted);
        printf("\n");
    }
}

/* ---- main ------------------------------------------------------------ */

static uint8_t *load(const char *path, long expect)
{
    FILE *f = fopen(path, "rb");
    uint8_t *buf;
    long n;
    if (!f) { fprintf(stderr, "kan inte öppna %s\n", path); exit(1); }
    buf = malloc((size_t)expect);
    n = (long)fread(buf, 1, (size_t)expect, f);
    fclose(f);
    if (n != expect) {
        fprintf(stderr, "%s: läste %ld byte, väntade %ld\n", path, n, expect);
        exit(1);
    }
    return buf;
}

static void usage(void)
{
    printf(
"uk101host [flaggor] [kommando ...]\n"
"\n"
"flaggor:\n"
"  --roms KATALOG   katalog med basic.rom, cegmon.rom (standard ..\\roms)\n"
"  --ram BYTE       arbets-RAM, standard 8192\n"
"\n"
"kommandon, körs i ordning:\n"
"  wait:MS          kör MS millisekunder maskintid\n"
"  key:NAMN         tryck tangent, t.ex. key:C key:RETURN key:SPACE\n"
"  shift:NAMN       samma men med shift nere\n"
"  dump             skriv ut skärmen\n"
"  calibrate        mät upp vilket tecken varje tangent ger\n"
"  keys             lista tangentnamn\n");
}

int main(int argc, char **argv)
{
    char rom_dir[512] = "..\\roms";
    char path[600];
    uint32_t ram = 8192;
    uint8_t *basic, *cegmon;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--roms") == 0 && i + 1 < argc) {
            snprintf(rom_dir, sizeof rom_dir, "%s", argv[++i]);
        } else if (strcmp(argv[i], "--ram") == 0 && i + 1 < argc) {
            ram = (uint32_t)strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(); return 0;
        } else {
            break;
        }
    }

    snprintf(path, sizeof path, "%s\\basic.rom", rom_dir);
    basic = load(path, 8192);
    snprintf(path, sizeof path, "%s\\cegmon.rom", rom_dir);
    cegmon = load(path, 2048);

    uk101_init(&mach, basic, cegmon, ram);
    pins = m6502_init(&cpu, &(m6502_desc_t){ 0 });
    printf("UK101: %u byte RAM, resetvektor $%04X\n",
           (unsigned)ram,
           (unsigned)(mach.mem[0xFFFC] | (mach.mem[0xFFFD] << 8)));

    for (; i < argc; i++) {
        char *cmd = argv[i];
        if (strncmp(cmd, "wait:", 5) == 0) {
            run_cycles(MS(strtoul(cmd + 5, NULL, 0)));
        } else if (strncmp(cmd, "key:", 4) == 0 || strncmp(cmd, "shift:", 6) == 0) {
            int shift = (cmd[0] == 's');
            const char *name = shift ? cmd + 6 : cmd + 4;
            uint8_t pos;
            if (!key_by_name(name, &pos)) {
                fprintf(stderr, "okänd tangent: %s\n", name); return 1;
            }
            tap(pos, shift);
        } else if (strncmp(cmd, "type:", 5) == 0) {
            if (!type_text(cmd + 5)) return 1;
        } else if (strcmp(cmd, "boot") == 0) {
            /* Kallstart av BASIC: CEGMON-prompt, C, och blanka svar på
             * MEMORY SIZE och TERMINAL WIDTH. */
            run_cycles(MS(500));
            tap(UK101_KEY(2, 6), 0);          /* C = kallstart av BASIC */
            run_cycles(MS(300));
            tap(UK101_KEY_RETURN, 0);
            run_cycles(MS(300));
            tap(UK101_KEY_RETURN, 0);
            run_cycles(MS(1500));
        } else if (strcmp(cmd, "dump") == 0) {
            dump_screen("skarm:");
        } else if (strcmp(cmd, "calibrate") == 0) {
            calibrate();
        } else if (strcmp(cmd, "keys") == 0) {
            int k;
            for (k = 0; k < KEYMAP_N; k++)
                printf("  %-10s rad %d bit %d\n", keymap[k].name,
                       keymap[k].pos >> 3, keymap[k].pos & 7);
        } else {
            fprintf(stderr, "okänt kommando: %s\n", cmd);
            usage(); return 1;
        }
    }
    return 0;
}
