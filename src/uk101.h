/* uk101.h — Compukit UK101 maskinmodell, plattformsoberoende.
 *
 * Modellen kan inget om skärmar, tangentbord eller filer. Den svarar bara på
 * bussåtkomster, precis som Neo6502:ans RP2040 gör mot den riktiga 65C02:an.
 * Därför delas exakt samma kod mellan värdharnesset (emulerad 6502) och
 * pico-firmwaren (riktig 65C02 över PIO).
 */
#ifndef UK101_H
#define UK101_H

#include <stdint.h>

/* Minneskarta, enligt originalets adressrum */
#define UK101_RAM_BASE     0x0000u
#define UK101_BASIC_BASE   0xA000u   /* 8 KB Microsoft BASIC          */
#define UK101_SCREEN_BASE  0xD000u   /* 1 KB skärm-RAM, 64x16 tecken  */
#define UK101_KBD_BASE     0xDF00u   /* tangentmatris, radval + kolumn */
#define UK101_ACIA_BASE    0xF000u   /* MC6850, $F000 status, $F001 data */
#define UK101_MONITOR_BASE 0xF800u   /* 2 KB CEGMON                   */

#define UK101_SCREEN_COLS  64
#define UK101_SCREEN_ROWS  16

/* Sidklass per 256-bytessida. En tabelluppslagning per bussåtkomst, vilket är
 * allt som hinns med i pico-loopen. */
enum {
    UK101_PAGE_RAM = 0,   /* skriv och läs                       */
    UK101_PAGE_ROM,       /* läs, skrivning ignoreras            */
    UK101_PAGE_OPEN,      /* ingen krets: läser $FF, skrivning faller bort */
    UK101_PAGE_KBD,       /* tangentmatris                       */
    UK101_PAGE_ACIA       /* serieport                           */
};

typedef struct {
    uint8_t mem[65536];   /* hela adressrummet, ROM inlagt vid init */
    uint8_t page[256];    /* sidklass, indexerad på högadressbyten  */
    uint8_t kbd_row;      /* senast skrivna radval, aktiv låg       */
    uint8_t kbd[8];       /* matrisrader, aktiv låg: 0 = nedtryckt  */
    uint8_t acia_ctrl;
    uint8_t acia_rx;
    uint8_t acia_rx_full;
} uk101_t;

/* ram_size = arbets-RAM i byte från $0000. Originalet hade 4K eller 8K.
 * basic8k och cegmon2k är råa ROM-bilder. */
void uk101_init(uk101_t *m, const uint8_t *basic8k, const uint8_t *cegmon2k,
                uint32_t ram_size);

uint8_t uk101_read(uk101_t *m, uint16_t addr);
void    uk101_write(uk101_t *m, uint16_t addr, uint8_t data);

/* Tangentposition i matrisen: rad 0-7, bit 0-7 */
#define UK101_KEY(row, bit) ((uint8_t)(((row) << 3) | (bit)))

#define UK101_KEY_SHIFTLOCK UK101_KEY(0, 0)
#define UK101_KEY_RSHIFT    UK101_KEY(0, 1)
#define UK101_KEY_LSHIFT    UK101_KEY(0, 2)
#define UK101_KEY_CTRL      UK101_KEY(0, 6)
#define UK101_KEY_RETURN    UK101_KEY(5, 3)
#define UK101_KEY_SPACE     UK101_KEY(1, 4)
#define UK101_KEY_RUBOUT    UK101_KEY(6, 2)

void uk101_key_set(uk101_t *m, uint8_t pos, int down);
void uk101_keys_clear(uk101_t *m);

/* Läser ett tecken ur skärm-RAM. Teckenkoderna är ASCII i intervallet
 * $20-$5F, vilket är varför monitorn kan lägga in text direkt. */
uint8_t uk101_screen_char(const uk101_t *m, int row, int col);

#endif /* UK101_H */
