/* uk101_keys.h — ASCII till tangentposition.
 *
 * Tabellen är inte gissad ur en tangentbordsbild, utan uppmätt: varje position
 * i matrisen trycktes vid BASIC-prompten, med och utan shift, och tecknet som
 * hamnade på skärmen lästes av. Kör host\uk101host.exe ... calibrate för att
 * göra om mätningen.
 *
 * Två egenheter i CEGMON:s avkodning som mätningen visade:
 *   - Shift på en bokstav ger bokstavens ASCII-kod + $10, inte gemener. Shift
 *     Lock ligger nere från start, så versaler får man utan shift. Det är
 *     därför bara gemenerna a-j går att skriva alls.
 *   - Tangenten som Searles PS/2-mappning kallar "-" ger i själva verket ":"
 *     och med shift "*". Tangenten han kallar "=" ger "-" och med shift "=".
 *     Det är originalets ": *" och "- =" -tangenter.
 *   - Positionerna rad 5 bit 4 och rad 6 bit 1 ger ingenting: CEGMON har ingen
 *     post för dem.
 */
#ifndef UK101_KEYS_H
#define UK101_KEYS_H

#include "uk101.h"

#define UK101_KC_NONE  0x00
#define UK101_KC_VALID 0x40
#define UK101_KC_SHIFT 0x80

#define UK101_KC(row, bit)  (uint8_t)(UK101_KC_VALID | UK101_KEY(row, bit))
#define UK101_KCS(row, bit) (uint8_t)(UK101_KC_VALID | UK101_KC_SHIFT | UK101_KEY(row, bit))

#define UK101_KC_POS(v)     (uint8_t)((v) & 0x3F)
#define UK101_KC_IS_SHIFT(v) (((v) & UK101_KC_SHIFT) != 0)
#define UK101_KC_IS_VALID(v) (((v) & UK101_KC_VALID) != 0)

/* Indexerad på ASCII-kod. 0 = tecknet går inte att skriva på en UK101. */
static const uint8_t uk101_ascii_key[128] = {
    /* 00-0F */
    0, 0, 0, 0, 0, 0, 0, 0,
    UK101_KC(6, 2),              /* 08 BS  -> RUBOUT */
    0,
    UK101_KC(5, 3),              /* 0A LF  -> RETURN */
    0, 0,
    UK101_KC(5, 3),              /* 0D CR  -> RETURN */
    0, 0,
    /* 10-1F */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 20-2F  ' ' ! " # $ % & ' ( ) * + , - . / */
    UK101_KC (1, 4),             /* space  */
    UK101_KCS(7, 7),             /* !      */
    UK101_KCS(7, 6),             /* "      */
    UK101_KCS(7, 5),             /* #      */
    UK101_KCS(7, 4),             /* $      */
    UK101_KCS(7, 3),             /* %      */
    UK101_KCS(7, 2),             /* &      */
    UK101_KCS(7, 1),             /* '      */
    UK101_KCS(6, 7),             /* (      */
    UK101_KCS(6, 6),             /* )      */
    UK101_KCS(6, 4),             /* *      */
    UK101_KCS(1, 2),             /* +      */
    UK101_KC (2, 1),             /* ,      */
    UK101_KC (6, 3),             /* -      */
    UK101_KC (5, 7),             /* .      */
    UK101_KC (1, 3),             /* /      */
    /* 30-3F  0-9 : ; < = > ? */
    UK101_KC (6, 5),             /* 0 */
    UK101_KC (7, 7),             /* 1 */
    UK101_KC (7, 6),             /* 2 */
    UK101_KC (7, 5),             /* 3 */
    UK101_KC (7, 4),             /* 4 */
    UK101_KC (7, 3),             /* 5 */
    UK101_KC (7, 2),             /* 6 */
    UK101_KC (7, 1),             /* 7 */
    UK101_KC (6, 7),             /* 8 */
    UK101_KC (6, 6),             /* 9 */
    UK101_KC (6, 4),             /* :      */
    UK101_KC (1, 2),             /* ;      */
    UK101_KCS(2, 1),             /* <      */
    UK101_KCS(6, 3),             /* =      */
    UK101_KCS(5, 7),             /* >      */
    UK101_KCS(1, 3),             /* ?      */
    /* 40-4F  @ A-O */
    UK101_KCS(6, 5),             /* @      */
    UK101_KC (1, 6),             /* A */
    UK101_KC (2, 4),             /* B */
    UK101_KC (2, 6),             /* C */
    UK101_KC (3, 6),             /* D */
    UK101_KC (4, 6),             /* E */
    UK101_KC (3, 5),             /* F */
    UK101_KC (3, 4),             /* G */
    UK101_KC (3, 3),             /* H */
    UK101_KC (4, 1),             /* I */
    UK101_KC (3, 2),             /* J */
    UK101_KC (3, 1),             /* K */
    UK101_KC (5, 6),             /* L */
    UK101_KC (2, 2),             /* M */
    UK101_KC (2, 3),             /* N */
    UK101_KC (5, 5),             /* O */
    /* 50-5F  P-Z [ \ ] ^ _ */
    UK101_KC (1, 1),             /* P */
    UK101_KC (1, 7),             /* Q */
    UK101_KC (4, 5),             /* R */
    UK101_KC (3, 7),             /* S */
    UK101_KC (4, 4),             /* T */
    UK101_KC (4, 2),             /* U */
    UK101_KC (2, 5),             /* V */
    UK101_KC (4, 7),             /* W */
    UK101_KC (2, 7),             /* X */
    UK101_KC (4, 3),             /* Y */
    UK101_KC (1, 5),             /* Z */
    UK101_KCS(3, 1),             /* [  = shift K */
    UK101_KCS(5, 6),             /* \  = shift L */
    UK101_KCS(2, 2),             /* ]  = shift M */
    UK101_KCS(2, 3),             /* ^  = shift N */
    UK101_KCS(5, 5),             /* _  = shift O */
    /* 60-6F  ` a-j, resten oåtkomligt */
    UK101_KCS(1, 1),             /* `  = shift P */
    UK101_KCS(1, 7),             /* a  = shift Q */
    UK101_KCS(4, 5),             /* b  = shift R */
    UK101_KCS(3, 7),             /* c  = shift S */
    UK101_KCS(4, 4),             /* d  = shift T */
    UK101_KCS(4, 2),             /* e  = shift U */
    UK101_KCS(2, 5),             /* f  = shift V */
    UK101_KCS(4, 7),             /* g  = shift W */
    UK101_KCS(2, 7),             /* h  = shift X */
    UK101_KCS(4, 3),             /* i  = shift Y */
    UK101_KCS(1, 5),             /* j  = shift Z */
    0, 0, 0, 0, 0,
    /* 70-7F */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

#endif /* UK101_KEYS_H */
