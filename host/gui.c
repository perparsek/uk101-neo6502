/* gui.c — uk101gui.exe, interaktiv UK101 med riktiga pixlar.
 *
 * Ritar skärm-RAM:et genom teckengeneratorn, samma avkodning som
 * pico-firmwaren gör: uk101_glyph_row(), bit 7 längst till vänster, vitt på
 * svart. Vertikal dubbling ger 512x256, precis som kortet visar i sitt
 * 640x480-läge.
 *
 * Tangentbordet går via SDL:s textinmatning, inte via scankoder. Det betyder
 * att tecknet du faktiskt får ur din layout är det som slås upp i UK101:ans
 * uppmätta tabell. Skriver du ett svenskt Shift+2 får du ett " på UK101:an,
 * oavsett var det låg på originalets tangentbord.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "hostbus.h"
#include "uk101_keys.h"

#define SCR_W        (UK101_SCREEN_COLS * 8)   /* 512 */
#define SCR_H        (UK101_SCREEN_ROWS * 8 * 2)  /* 256, vertikalt dubblat */
#define ZOOM         2
#define CYCLES_FRAME (CPU_HZ / 60)

static const uint8_t *chargen;

/* ---- rendering ------------------------------------------------------- */

/* Skriver SCR_W x SCR_H pixlar i ARGB8888. pitch anges i pixlar. */
static void render(uint32_t *px, int pitch)
{
    int y;
    for (y = 0; y < SCR_H; y++) {
        int sy   = y >> 1;                 /* vertikal dubbling */
        int row  = sy >> 3;
        int line = sy & 7;
        uint32_t *out = px + (size_t)y * pitch;
        int c;
        for (c = 0; c < UK101_SCREEN_COLS; c++) {
            uint8_t ch   = uk101_screen_char(&host_mach, row, c);
            uint8_t bits = uk101_glyph_row(chargen, ch, (uint8_t)line);
            int b;
            for (b = 0; b < 8; b++)
                *out++ = (bits & (0x80u >> b)) ? 0xFFFFFFFFu : 0xFF000000u;
        }
    }
}

/* ---- tangentbord ----------------------------------------------------- */

static int shift_lock = 1;   /* låsande tangent, ligger nere från start */

static void set_shift_lock(int on)
{
    shift_lock = on;
    uk101_key_set(&host_mach, UK101_KEY_SHIFTLOCK, on);
    printf("shift lock %s\n", on ? "nere (versaler)" : "uppe");
    fflush(stdout);
}

/* Trycker en tangent med CTRL nere. BASIC bryter på CTRL+C. */
static void tap_ctrl(uint8_t pos)
{
    uk101_key_set(&host_mach, UK101_KEY_CTRL, 1);
    host_tap(pos, 0);
    uk101_key_set(&host_mach, UK101_KEY_CTRL, 0);
}

/* Ett tecken ur textinmatningen. Returnerar 0 om UK101 inte har tangenten. */
static int type_char(char ch)
{
    uint8_t kc;
    if ((unsigned char)ch > 127) return 0;
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');  /* BASIC vill versaler */
    kc = uk101_ascii_key[(unsigned char)ch];
    if (!UK101_KC_IS_VALID(kc)) return 0;
    host_tap(UK101_KC_POS(kc), UK101_KC_IS_SHIFT(kc));
    return 1;
}

/* ---- interaktiv loop ------------------------------------------------- */

static int interactive(void)
{
    SDL_Window   *win;
    SDL_Renderer *ren;
    SDL_Texture  *tex;
    int running = 1;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    win = SDL_CreateWindow("Compukit UK101", SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED, SCR_W * ZOOM, SCR_H * ZOOM,
                           SDL_WINDOW_RESIZABLE);
    if (!win) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return 1; }

    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); return 1; }

    SDL_RenderSetLogicalSize(ren, SCR_W, SCR_H);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                            SDL_TEXTUREACCESS_STREAMING, SCR_W, SCR_H);
    if (!tex) { fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError()); return 1; }

    SDL_StartTextInput();

    printf("\nUK101 kor. Tryck C vid CEGMON-prompten for att kallstarta BASIC,\n"
           "eller W for varmstart, M for monitorn.\n"
           "  Backsteg   RUBOUT\n"
           "  Ctrl+C     bryter ett BASIC-program\n"
           "  Caps Lock  vaxlar Shift Lock\n"
           "  F12        reset\n"
           "  F11        kallstartar BASIC direkt\n\n");
    fflush(stdout);

    while (running) {
        SDL_Event e;
        void *pixels;
        int pitch;

        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                running = 0;
                break;

            case SDL_TEXTINPUT: {
                const char *p;
                for (p = e.text.text; *p; p++) type_char(*p);
                break;
            }

            case SDL_KEYDOWN:
                switch (e.key.keysym.sym) {
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    host_tap(UK101_KEY_RETURN, 0);
                    break;
                case SDLK_BACKSPACE:
                case SDLK_DELETE:
                    host_tap(UK101_KEY_RUBOUT, 0);
                    break;
                case SDLK_CAPSLOCK:
                    set_shift_lock(!shift_lock);
                    break;
                case SDLK_F12:
                    host_reset();
                    printf("reset\n");
                    fflush(stdout);
                    break;
                case SDLK_F11:
                    host_cold_start_basic();
                    break;
                default:
                    /* CTRL+bokstav ger ingen textinmatning, sa den fangas har. */
                    if ((e.key.keysym.mod & KMOD_CTRL) &&
                        e.key.keysym.sym >= SDLK_a && e.key.keysym.sym <= SDLK_z) {
                        char letter = (char)('A' + (e.key.keysym.sym - SDLK_a));
                        uint8_t kc = uk101_ascii_key[(unsigned char)letter];
                        if (UK101_KC_IS_VALID(kc)) tap_ctrl(UK101_KC_POS(kc));
                    }
                    break;
                }
                break;
            }
        }

        host_run_cycles(CYCLES_FRAME);

        if (SDL_LockTexture(tex, NULL, &pixels, &pitch) == 0) {
            render((uint32_t *)pixels, pitch / 4);
            SDL_UnlockTexture(tex);
        }
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }

    SDL_StopTextInput();
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

/* ---- skärmdump till fil ---------------------------------------------- */

/* Ritar en bild utan att öppna fönster. Används för att kunna granska
 * pixelrenderingen utan skärm. */
static int shot(const char *path)
{
    uint32_t *px = malloc((size_t)SCR_W * SCR_H * 4);
    SDL_Surface *s;
    int rc;

    render(px, SCR_W);
    s = SDL_CreateRGBSurfaceFrom(px, SCR_W, SCR_H, 32, SCR_W * 4,
                                 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!s) {
        fprintf(stderr, "SDL_CreateRGBSurfaceFrom: %s\n", SDL_GetError());
        free(px);
        return 1;
    }
    rc = SDL_SaveBMP(s, path);
    if (rc != 0) fprintf(stderr, "SDL_SaveBMP: %s\n", SDL_GetError());
    else         printf("skrev %s (%dx%d)\n", path, SCR_W, SCR_H);
    SDL_FreeSurface(s);
    free(px);
    return rc != 0;
}

/* ---- main ------------------------------------------------------------ */

static void usage(void)
{
    printf(
"uk101gui [flaggor] [kommando ...]\n"
"\n"
"Utan --shot oppnas ett fonster och maskinen blir interaktiv.\n"
"\n"
"flaggor:\n"
"  --roms KATALOG   katalog med basic.rom, cegmon.rom, chargen.rom\n"
"                   (standard ..\\roms)\n"
"  --ram BYTE       arbets-RAM, standard 8192\n"
"  --shot FIL.bmp   oppna inget fonster, kor kommandona och spara bilden\n"
"\n"
"kommandon, korrs innan fonstret oppnas:\n"
"  boot             kallstartar BASIC\n"
"  wait:MS          kor MS millisekunder maskintid\n"
"  type:TEXT        skriver text, ~ betyder RETURN\n"
"  key:NAMN         trycker en tangent\n"
"  shift:NAMN       samma med shift nere\n"
"  reset            haller 6502:an i reset ett ogonblick\n");
}

int main(int argc, char **argv)
{
    char rom_dir[512] = "..\\roms";
    const char *shot_path = NULL;
    uint32_t ram = 8192;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--roms") == 0 && i + 1 < argc) {
            snprintf(rom_dir, sizeof rom_dir, "%s", argv[++i]);
        } else if (strcmp(argv[i], "--ram") == 0 && i + 1 < argc) {
            ram = (uint32_t)strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--shot") == 0 && i + 1 < argc) {
            shot_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            usage();
            return 0;
        } else {
            break;
        }
    }

    chargen = host_boot(rom_dir, ram);
    printf("UK101: %u byte RAM, resetvektor $%04X\n", (unsigned)ram,
           (unsigned)(host_mach.mem[0xFFFC] | (host_mach.mem[0xFFFD] << 8)));

    for (; i < argc; i++) {
        int r = host_command(argv[i]);
        if (r < 0) return 1;
        if (r == 0) {
            fprintf(stderr, "okant kommando: %s\n", argv[i]);
            usage();
            return 1;
        }
    }

    if (shot_path) return shot(shot_path);
    return interactive();
}
