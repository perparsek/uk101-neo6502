/* uk101.c — se uk101.h */

#include "uk101.h"

static void set_pages(uk101_t *m, unsigned first, unsigned last, uint8_t cls)
{
    unsigned p;
    for (p = first; p <= last; p++) m->page[p] = cls;
}

void uk101_init(uk101_t *m, const uint8_t *basic8k, const uint8_t *cegmon2k,
                uint32_t ram_size)
{
    unsigned i;
    uint32_t ram_pages;

    for (i = 0; i < 65536; i++) m->mem[i] = 0;

    /* Allt är öppen buss tills något deklarerar sig */
    set_pages(m, 0x00, 0xFF, UK101_PAGE_OPEN);

    if (ram_size > 0xA000) ram_size = 0xA000;
    ram_pages = ram_size >> 8;
    if (ram_pages) set_pages(m, 0x00, (unsigned)(ram_pages - 1), UK101_PAGE_RAM);

    set_pages(m, 0xA0, 0xBF, UK101_PAGE_ROM);   /* BASIC              */
    set_pages(m, 0xD0, 0xD3, UK101_PAGE_RAM);   /* skärm-RAM          */
    set_pages(m, 0xDF, 0xDF, UK101_PAGE_KBD);   /* tangentmatris      */
    set_pages(m, 0xF0, 0xF7, UK101_PAGE_ACIA);  /* MC6850, speglad    */
    set_pages(m, 0xF8, 0xFF, UK101_PAGE_ROM);   /* CEGMON + vektorer  */

    for (i = 0; i < 8192; i++) m->mem[UK101_BASIC_BASE + i] = basic8k[i];
    for (i = 0; i < 2048; i++) m->mem[UK101_MONITOR_BASE + i] = cegmon2k[i];

    /* Skärmen fylls med blanksteg, som när videokretsen startar */
    for (i = 0; i < 1024; i++) m->mem[UK101_SCREEN_BASE + i] = 0x20;

    uk101_keys_clear(m);
    m->kbd_row = 0xFF;
    m->acia_ctrl = 0;
    m->acia_rx = 0;
    m->acia_rx_full = 0;
}

void uk101_keys_clear(uk101_t *m)
{
    int r;
    for (r = 0; r < 8; r++) m->kbd[r] = 0xFF;
    /* Shift Lock ligger nere vid start, så maskinen börjar i versaler.
     * Samma utgångsläge som originalets mekaniskt låsande tangent. */
    m->kbd[0] &= (uint8_t)~0x01;
}

void uk101_key_set(uk101_t *m, uint8_t pos, int down)
{
    uint8_t row = (uint8_t)((pos >> 3) & 7);
    uint8_t bit = (uint8_t)(1u << (pos & 7));
    if (down) m->kbd[row] &= (uint8_t)~bit;
    else      m->kbd[row] |= bit;
}

static uint8_t kbd_read(const uk101_t *m)
{
    /* Monitorn väljer flera rader samtidigt, så resultatet är OCH över alla
     * valda rader. Radvalet i kbd_row är aktivt lågt, och en nedtryckt tangent
     * drar sin kolumnbit låg. */
    uint8_t result = 0xFF;
    int r;
    for (r = 0; r < 8; r++)
        if (!(m->kbd_row & (1u << r))) result &= m->kbd[r];
    return result;
}

static uint8_t acia_read(uk101_t *m, uint16_t addr)
{
    if (addr & 1) {                 /* datamottagning */
        m->acia_rx_full = 0;
        return m->acia_rx;
    }
    /* Statusregister: TDRE alltid satt, annars ligger utmatningen och snurrar.
     * RDRF sätts bara när något faktiskt matats in. */
    return (uint8_t)(0x02 | (m->acia_rx_full ? 0x01 : 0x00));
}

uint8_t uk101_read(uk101_t *m, uint16_t addr)
{
    switch (m->page[addr >> 8]) {
        case UK101_PAGE_RAM:
        case UK101_PAGE_ROM:  return m->mem[addr];
        case UK101_PAGE_KBD:  return kbd_read(m);
        case UK101_PAGE_ACIA: return acia_read(m, addr);
        default:              return 0xFF;
    }
}

void uk101_write(uk101_t *m, uint16_t addr, uint8_t data)
{
    switch (m->page[addr >> 8]) {
        case UK101_PAGE_RAM:  m->mem[addr] = data; break;
        case UK101_PAGE_KBD:  m->kbd_row = data;   break;
        case UK101_PAGE_ACIA:
            if (!(addr & 1)) m->acia_ctrl = data;
            /* Sänd data kastas: v1 har ingen kassett eller serieport. */
            break;
        default: break;  /* ROM och öppen buss sväljer skrivningen */
    }
}

uint8_t uk101_screen_char(const uk101_t *m, int row, int col)
{
    return m->mem[UK101_SCREEN_BASE + row * UK101_SCREEN_COLS + col];
}
