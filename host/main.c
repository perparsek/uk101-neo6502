/* main.c — uk101host.exe, skriptad provrigg.
 *
 * Kör UK101-modellen headless och skriver ut skärm-RAM:et som text.
 * Kommandona körs i ordning, se usage(). För en interaktiv maskin med riktiga
 * pixlar, se uk101gui.exe.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hostbus.h"
#include "uk101_keys.h"

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
            uint8_t ch = uk101_screen_char(&host_mach, r, c);
            putchar((ch >= 0x20 && ch < 0x7F) ? (char)ch : '.');
        }
        printf("|\n");
    }
    printf("    +");
    for (c = 0; c < UK101_SCREEN_COLS; c++) putchar('-');
    printf("+\n");
}

/* ---- minnesdump ------------------------------------------------------ */

/* Läser genom uk101_read, alltså samma väg som processorn ser minnet.
 * Tangentbord och ACIA har sidoeffekter vid läsning, så de hoppas över. */
static void dump_mem(uint16_t addr, int len)
{
    int i;
    printf("\n%04X..%04X\n", addr, (unsigned)(addr + len - 1) & 0xFFFF);
    for (i = 0; i < len; i += 16) {
        int j, n = (len - i < 16) ? len - i : 16;
        printf("  %04X  ", (unsigned)(addr + i) & 0xFFFF);
        for (j = 0; j < 16; j++) {
            if (j < n) printf("%02X ", host_mach.mem[(addr + i + j) & 0xFFFF]);
            else       printf("   ");
            if (j == 7) putchar(' ');
        }
        printf(" |");
        for (j = 0; j < n; j++) {
            uint8_t c = host_mach.mem[(addr + i + j) & 0xFFFF];
            putchar((c >= 0x20 && c < 0x7F) ? (char)c : '.');
        }
        printf("|\n");
    }
}

/* ---- kalibrering av teckenuppsättningen ------------------------------ */

/* Vilket tecken en fysisk tangent ger är monitorns sak, inte hårdvarans.
 * Istället för att gissa layouten trycks varje position vid BASIC-prompten och
 * tecknet läses av där markören stod. */
static int cursor_cell(void)
{
    int j;
    for (j = 0; j < 1024; j++)
        if (host_mach.mem[UK101_SCREEN_BASE + j] == 0x5F) return j;
    return -1;
}

static int measure(uint8_t pos, int shift)
{
    int cell = cursor_cell();
    uint8_t got;
    if (cell < 0) return -1;
    host_tap(pos, shift);
    got = host_mach.mem[UK101_SCREEN_BASE + cell];
    host_tap(UK101_KEY_RUBOUT, 0);         /* radera, nästa mätning startar rent */
    return (got == 0x5F) ? -1 : (int)got;  /* markören kvar = tangenten gav inget */
}

static void show(int code)
{
    if (code < 0)          printf("%-9s", "-");
    else if (code == 0x20) printf("%02X %-6s", code, "SP");
    else if (code < 0x21)  printf("%02X %-6s", code, "ctl");
    else                   printf("%02X %-6c", code, (char)code);
}

static void calibrate(void)
{
    int i;
    printf("\nkalibrering: fysisk tangent -> tecken\n");
    printf("%-10s %-6s %-6s\n", "tangent", "osk.", "shift");
    for (i = 0; i < host_keymap_n; i++) {
        uint8_t pos = host_keymap[i].pos;
        int plain, shifted;
        if (pos == UK101_KEY_SHIFTLOCK || pos == UK101_KEY_LSHIFT ||
            pos == UK101_KEY_RSHIFT || pos == UK101_KEY_CTRL ||
            pos == UK101_KEY_RETURN || pos == UK101_KEY_RUBOUT) continue;

        plain   = measure(pos, 0);
        shifted = measure(pos, 1);
        printf("%-10s ", host_keymap[i].name);
        show(plain);
        show(shifted);
        printf("\n");
    }
}

/* ---- main ------------------------------------------------------------ */

static void usage(void)
{
    printf(
"uk101host [flaggor] [kommando ...]\n"
"\n"
"flaggor:\n"
"  --roms KATALOG   katalog med basic.rom, cegmon.rom, chargen.rom\n"
"                   (standard ..\\roms)\n"
"  --ram BYTE       arbets-RAM, standard 8192\n"
"\n"
"kommandon, korrs i ordning:\n"
"  boot             kallstartar BASIC\n"
"  wait:MS          kor MS millisekunder maskintid\n"
"  type:TEXT        skriver text via tangentmatrisen, ~ betyder RETURN\n"
"  key:NAMN         trycker en tangent, t.ex. key:C key:RETURN key:RUBOUT\n"
"  shift:NAMN       samma med shift nere\n"
"  dump             skriver ut skarmen, 64x16\n"
"  reset            haller 6502:an i reset ett ogonblick\n"
"  calibrate        mater om vilket tecken varje position ger\n"
"  keys             listar tangentnamn och matrisposition\n"
"\n"
"For en interaktiv maskin, kor uk101gui.exe istallet.\n");
}

int main(int argc, char **argv)
{
    char rom_dir[512] = "..\\roms";
    uint32_t ram = 8192;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--roms") == 0 && i + 1 < argc) {
            snprintf(rom_dir, sizeof rom_dir, "%s", argv[++i]);
        } else if (strcmp(argv[i], "--ram") == 0 && i + 1 < argc) {
            ram = (uint32_t)strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--help") == 0) {
            usage();
            return 0;
        } else {
            break;
        }
    }

    host_boot(rom_dir, ram);
    printf("UK101: %u byte RAM, resetvektor $%04X\n", (unsigned)ram,
           (unsigned)(host_mach.mem[0xFFFC] | (host_mach.mem[0xFFFD] << 8)));

    for (; i < argc; i++) {
        char *cmd = argv[i];
        int r = host_command(cmd);
        if (r < 0) return 1;
        if (r > 0) continue;

        if (strncmp(cmd, "mem:", 4) == 0) {
            unsigned a = 0, n = 64;
            sscanf(cmd + 4, "%x,%u", &a, &n);
            dump_mem((uint16_t)a, (int)n);
        } else if (strcmp(cmd, "zp") == 0) {
            dump_mem(0x0000, 256);
        } else if (strcmp(cmd, "dump") == 0) {
            dump_screen("skarm:");
        } else if (strcmp(cmd, "calibrate") == 0) {
            calibrate();
        } else if (strcmp(cmd, "keys") == 0) {
            int k;
            for (k = 0; k < host_keymap_n; k++)
                printf("  %-10s rad %d bit %d\n", host_keymap[k].name,
                       host_keymap[k].pos >> 3, host_keymap[k].pos & 7);
        } else {
            fprintf(stderr, "okant kommando: %s\n", cmd);
            usage();
            return 1;
        }
    }
    return 0;
}
