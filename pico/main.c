/* main.c — UK101 på Olimex Neo6502.
 *
 * RP2040:n gör tre saker: matar den riktiga W65C02S:an med UK101:ans minneskarta
 * över PIO, ritar skärm-RAM:et som 64x16 tecken på HDMI, och översätter ett
 * USB-tangentbord till UK101:ans tangentmatris.
 *
 *   core0   bussloopen. En PIO-post per 6502-cykel, tabelluppslag, svara.
 *           Var 1024:e cykel lämnas tid till USB-stacken.
 *   core1   DVI. Avbrottsdriven, rader byggs i _scanline_callback.
 *
 * Maskinmodellen (src/uk101.c) är exakt samma kod som värdharnesset kör mot en
 * emulerad 6502, så minneskarta och tangentmatris är verifierade på PC.
 *
 * Bussens PIO-program och pinout kommer från Neo6502:ans egen firmware
 * (paulscottrobson/neo6502-firmware), som är det som körs på hårdvaran i
 * original. Se pico/uk101_bus.pio.
 */

#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/sem.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/structs/bus_ctrl.h"

#include "dvi.h"
#include "dvi_serialiser.h"
#include "common_dvi_pin_configs.h"
#include "tmds_encode.h"

#include "tusb.h"
#include "class/hid/hid.h"

#include "uk101.h"
#include "uk101_keys.h"
#include "uk101_roms.h"
#include "uk101_bus.pio.h"

/* ---- videoläge ------------------------------------------------------- */

/* 640x480@60Hz, monokromt. DVI_VERTICAL_REPEAT=2 gör att varje rad i bufferten
 * visas två gånger, så bildminnet är 640x240 och tecknen blir 8x16 fysiska
 * pixlar. UK101-fönstret blir 512x128 i den bufferten, centrerat. */
#define FRAME_WIDTH   640
#define FRAME_HEIGHT  240
#define VREG_VSEL     VREG_VOLTAGE_1_20
#define DVI_TIMING    dvi_timing_640x480p_60hz

#define WIN_W   (UK101_SCREEN_COLS * 8)          /* 512 */
#define WIN_H   (UK101_SCREEN_ROWS * 8)          /* 128 */
#define WIN_X   ((FRAME_WIDTH  - WIN_W) / 2)     /* 64, jämnt delbart med 8 */
#define WIN_Y   ((FRAME_HEIGHT - WIN_H) / 2)     /* 56 */

/* Pinout för Neo6502, ur kortets egen firmware. */
static const struct dvi_serialiser_cfg neo6502_dvi_cfg = {
    .pio = DVI_DEFAULT_PIO_INST,
    .sm_tmds = { 0, 1, 2 },
    .pins_tmds = { 14, 18, 16 },
    .pins_clk = 12,
    .invert_diffpairs = true
};

/* ---- 65C02-buss ------------------------------------------------------ */

/* Bussens PIO ligger på pio1 sm0. DVI:n äger pio0 sm0-2. */
#define BUS_PIO   pio1
#define BUS_SM    0

/* PIO-programmet kostar omkring 25 PIO-cykler per 6502-cykel. Med 252 MHz
 * systemklocka ger delaren nedan ungefär 1 MHz, alltså originalets takt.
 * Höj till 1.0 om du vill köra kortet så fort det går. */
#define BUS_CLKDIV  10.08f

/* GPIO-nummer, ur Neo6502:ans wdc65C02cpu.h */
#define PIN_OE1     8
#define PIN_OE2     9
#define PIN_OE3     10
#define PIN_RW      11
#define PIN_AUDIO   20
#define PIN_CLOCK   21
#define PIN_IRQ     25
#define PIN_RESET   26
#define PIN_NMI     27

static uk101_t mach;

static struct dvi_inst dvi0;
static struct semaphore dvi_start_sem;

/* ---- video ----------------------------------------------------------- */

static void __not_in_flash_func(prepare_scanline)(uint y)
{
    static uint8_t scanbuf[FRAME_WIDTH / 8];
    uint32_t *tmdsbuf;

    memset(scanbuf, 0, sizeof scanbuf);

    if (y >= WIN_Y && y < WIN_Y + WIN_H) {
        uint sy   = y - WIN_Y;
        uint line = sy & 7;
        const uint8_t *src = &mach.mem[UK101_SCREEN_BASE +
                                       (sy >> 3) * UK101_SCREEN_COLS];
        uint8_t *dst = &scanbuf[WIN_X / 8];
        uint c;
        /* Teckengeneratorn adresseras teckenkod*8 + rad, bit 7 längst till
         * vänster. Kompileringsflaggan DVI_1BPP_BIT_REVERSE=1 gör att
         * kodaren läser bitarna i samma ordning, så inget behöver vändas. */
        for (c = 0; c < UK101_SCREEN_COLS; c++)
            dst[c] = uk101_rom_chargen[((uint16_t)src[c] << 3) | line];
    }

    queue_remove_blocking(&dvi0.q_tmds_free, &tmdsbuf);
    tmds_encode_1bpp((const uint32_t *)scanbuf, tmdsbuf, FRAME_WIDTH);
    queue_add_blocking(&dvi0.q_tmds_valid, &tmdsbuf);
}

static void __not_in_flash_func(scanline_callback)(void)
{
    static uint y = 1;
    prepare_scanline(y);
    y = (y + 1) % FRAME_HEIGHT;
}

static void __not_in_flash_func(core1_main)(void)
{
    dvi_register_irqs_this_core(&dvi0, DMA_IRQ_1);
    sem_acquire_blocking(&dvi_start_sem);
    dvi_start(&dvi0);
    while (1) __wfi();
}

static void video_start(void)
{
    dvi0.timing = &DVI_TIMING;
    dvi0.ser_cfg = neo6502_dvi_cfg;
    dvi0.scanline_callback = scanline_callback;
    dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());

    prepare_scanline(0);

    sem_init(&dvi_start_sem, 0, 1);
    hw_set_bits(&bus_ctrl_hw->priority, BUSCTRL_BUS_PRIORITY_PROC1_BITS);
    multicore_launch_core1(core1_main);
    sem_release(&dvi_start_sem);
}

/* ---- tangentbord ----------------------------------------------------- */

/* Shift Lock är en låsande tangent på originalet och styrs med Caps Lock. */
static bool shift_lock = true;

/* HID-koden översätts först till det tecken användaren menade, och tecknet
 * slås sedan upp i den uppmätta UK101-tabellen. Det ger en tangentbordskänsla
 * som stämmer med tryckta symboler istället för att kräva originalets layout.
 * Gemener versaliseras: UK101:ans BASIC förstår bara versaler. */
static const uint8_t hid_ascii[128][2] = { HID_KEYCODE_TO_ASCII };

static char hid_to_ascii(uint8_t kc, bool shift)
{
    char ch;
    if (kc >= 128) return 0;
    ch = (char)hid_ascii[kc][shift ? 1 : 0];
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    return ch;
}

static void kbd_apply(uint8_t const *report)
{
    bool shift = (report[0] & (KEYBOARD_MODIFIER_LEFTSHIFT |
                               KEYBOARD_MODIFIER_RIGHTSHIFT)) != 0;
    bool ctrl  = (report[0] & (KEYBOARD_MODIFIER_LEFTCTRL |
                               KEYBOARD_MODIFIER_RIGHTCTRL)) != 0;
    int i;

    /* Bygg om hela matrisen ur rapporten. En USB-rapport är fullständig, så
     * det är enklare och säkrare än att spåra upp- och nedhändelser. */
    for (i = 0; i < 8; i++) mach.kbd[i] = 0xFF;
    if (shift_lock) uk101_key_set(&mach, UK101_KEY_SHIFTLOCK, 1);
    if (ctrl)       uk101_key_set(&mach, UK101_KEY_CTRL, 1);

    for (i = 2; i < 8; i++) {
        uint8_t kc = report[i];
        char ch;
        uint8_t pos;
        if (kc == 0) continue;

        if (kc == HID_KEY_CAPS_LOCK) continue;   /* hanteras vid nedslag */
        if (kc == HID_KEY_ENTER || kc == HID_KEY_KEYPAD_ENTER) {
            uk101_key_set(&mach, UK101_KEY_RETURN, 1);
            continue;
        }
        if (kc == HID_KEY_BACKSPACE || kc == HID_KEY_DELETE) {
            uk101_key_set(&mach, UK101_KEY_RUBOUT, 1);
            continue;
        }
        if (kc == HID_KEY_SPACE) {
            uk101_key_set(&mach, UK101_KEY_SPACE, 1);
            continue;
        }

        ch = hid_to_ascii(kc, shift);
        if (ch <= 0) continue;
        pos = uk101_ascii_key[(unsigned char)ch];
        if (!UK101_KC_IS_VALID(pos)) continue;
        uk101_key_set(&mach, UK101_KC_POS(pos), 1);
        if (UK101_KC_IS_SHIFT(pos))
            uk101_key_set(&mach, UK101_KEY_LSHIFT, 1);
    }
}

/* Caps Lock ska växla, inte hållas. Kolla mot föregående rapport. */
static void kbd_report(uint8_t const *report)
{
    static bool caps_was_down = false;
    bool caps_down = false;
    int i;
    for (i = 2; i < 8; i++)
        if (report[i] == HID_KEY_CAPS_LOCK) caps_down = true;
    if (caps_down && !caps_was_down) shift_lock = !shift_lock;
    caps_was_down = caps_down;

    kbd_apply(report);
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance,
                      uint8_t const *desc_report, uint16_t desc_len)
{
    (void)desc_report; (void)desc_len;
    tuh_hid_receive_report(dev_addr, instance);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    (void)dev_addr; (void)instance;
    uk101_keys_clear(&mach);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                uint8_t const *report, uint16_t len)
{
    if (tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_KEYBOARD
        && len >= 8)
        kbd_report(report);
    tuh_hid_receive_report(dev_addr, instance);
}

/* ---- bussloop -------------------------------------------------------- */

static void bus_init(void)
{
    uint offset;

    gpio_init(PIN_OE1); gpio_set_dir(PIN_OE1, GPIO_OUT); gpio_put(PIN_OE1, 1);
    gpio_init(PIN_OE2); gpio_set_dir(PIN_OE2, GPIO_OUT); gpio_put(PIN_OE2, 1);
    gpio_init(PIN_OE3); gpio_set_dir(PIN_OE3, GPIO_OUT); gpio_put(PIN_OE3, 1);
    gpio_init(PIN_RW);  gpio_set_dir(PIN_RW, GPIO_IN);
    gpio_init(PIN_CLOCK); gpio_set_dir(PIN_CLOCK, GPIO_OUT);
    gpio_init(PIN_IRQ); gpio_set_dir(PIN_IRQ, GPIO_OUT); gpio_put(PIN_IRQ, 1);
    gpio_init(PIN_NMI); gpio_set_dir(PIN_NMI, GPIO_OUT); gpio_put(PIN_NMI, 1);
    gpio_init(PIN_RESET); gpio_set_dir(PIN_RESET, GPIO_OUT);

    gpio_put(PIN_RESET, 0);

    offset = pio_add_program(BUS_PIO, &memory_emulation_with_clock_program);
    memory_emulation_with_clock_program_init(BUS_PIO, BUS_SM, offset);
    pio_sm_set_clkdiv(BUS_PIO, BUS_SM, BUS_CLKDIV);
    pio_sm_set_enabled(BUS_PIO, BUS_SM, true);

    /* Släpp reset först när minnet svarar, annars hämtar CPU:n skräpvektor. */
    sleep_ms(10);
    gpio_put(PIN_RESET, 1);
}

static void __not_in_flash_func(bus_run)(void)
{
    union {
        uint32_t value;
        struct { uint16_t address; uint8_t flags; } data;
    } v;
    uint16_t housekeep = 0;

    while (1) {
        v.value = pio_sm_get_blocking(BUS_PIO, BUS_SM);

        if (v.data.flags & 0x8) {                 /* 65C02 läser  */
            pio_sm_put(BUS_PIO, BUS_SM, uk101_read(&mach, v.data.address));
        } else {                                  /* 65C02 skriver */
            uk101_write(&mach, v.data.address,
                        (uint8_t)pio_sm_get_blocking(BUS_PIO, BUS_SM));
        }

        /* USB-stacken behöver köras ungefär varje millisekund. Vid 1 MHz
         * 6502-takt är 1024 bussåtkomster just det. */
        if (++housekeep >= 1024) {
            housekeep = 0;
            tuh_task();
        }
    }
}

/* ---- main ------------------------------------------------------------ */

int main(void)
{
    vreg_set_voltage(VREG_VSEL);
    sleep_ms(10);
    set_sys_clock_khz(DVI_TIMING.bit_clk_khz, true);

    /* 8 KB arbets-RAM, originalets maximala påbyggnad. Höj hit upp till
     * $A000 om du vill ha mer utrymme åt BASIC-program. */
    uk101_init(&mach, uk101_rom_basic, uk101_rom_cegmon, 8192);

    video_start();

    tuh_init(0);                      /* USB-varden sitter pa RH-port 0 */

    bus_init();
    bus_run();
    return 0;
}
