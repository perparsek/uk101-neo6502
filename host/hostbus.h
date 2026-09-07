/* hostbus.h — UK101-modellen driven av en emulerad 6502.
 *
 * Delas av de två värdprogrammen: uk101host.exe (skriptad provrigg) och
 * uk101gui.exe (interaktivt fönster). Bussloopen har samma form som
 * pico-firmwarens: en adress in, en byte ut.
 */
#ifndef HOSTBUS_H
#define HOSTBUS_H

#include <stdint.h>
#include "uk101.h"

#define CPU_HZ  1000000u                    /* originalets klockfrekvens */
#define MS(n)   ((unsigned long)(n) * (CPU_HZ / 1000u))

extern uk101_t host_mach;

/* Läser roms\basic.rom, cegmon.rom och chargen.rom ur rom_dir och startar
 * maskinen. Teckengeneratorn returneras för den som ska rita. Avbryter
 * programmet med felmeddelande om något ROM saknas eller har fel storlek. */
const uint8_t *host_boot(const char *rom_dir, uint32_t ram_size);

/* Håller 6502:an i reset ett ögonblick och släpper den igen. */
void host_reset(void);

void host_run_cycles(unsigned long n);

/* Trycker och släpper en tangent med tider som monitorns avsökning hinner se. */
void host_tap(uint8_t pos, int shift);

/* Skriver text via tangentmatrisen. Tecknet ~ betyder RETURN. Returnerar 0 och
 * skriver till stderr om något tecken inte finns på UK101-tangentbordet. */
int host_type(const char *s);

/* Lägger en fil i bandspelaren. Radslut normaliseras till CR, så både
 * CRLF-filer från Windows och LF-filer fungerar. Returnerar 0 vid fel. */
int  host_tape_load(const char *path);
void host_tape_eject(void);

/* Läser in ett program och lämnar tillbaka tangentbordet.
 *
 * Gör hela originalets manöver: lägger i bandet, skriver LOAD, matar fram
 * bandet, och tar sedan RESET följt av W. Reset behövs eftersom BASIC:ens LOAD
 * växlar inmatningen till ACIA:n och aldrig växlar tillbaka av sig själv, och W
 * är varmstart, som lämnar det inlästa programmet i minnet.
 *
 * Efteråt står maskinen vid OK-prompten med programmet laddat. Returnerar 0
 * vid fel. */
int host_load_program(const char *path);

/* Allt BASIC skickar till bandet spelas in i en buffert. host_tape_save skriver
 * bufferten till fil och nollar den. Arbetsgången på maskinen är SAVE, sedan
 * LIST, och sedan den här. Returnerar 0 vid fel eller om inget spelats in. */
void host_recorder_start(void);
int  host_tape_save(const char *path);
long host_tape_recorded(void);

/* Kallstartar BASIC: väntar in CEGMON-prompten, trycker C och svarar blankt på
 * MEMORY SIZE och TERMINAL WIDTH. */
void host_cold_start_basic(void);

/* Utför ett kommando av formen boot, wait:MS, type:TEXT, key:NAMN, shift:NAMN
 * eller reset. Returnerar 1 om utfört, 0 om kommandot är okänt för den här
 * nivån, -1 om kommandot kändes igen men misslyckades. */
int host_command(const char *cmd);

/* Tangentnamn, för kommandoraden. Positionerna är originalets matris. */
typedef struct { uint8_t pos; const char *name; } host_key_t;
extern const host_key_t host_keymap[];
extern const int host_keymap_n;
int host_key_by_name(const char *name, uint8_t *out);

#endif /* HOSTBUS_H */
