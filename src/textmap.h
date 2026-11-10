/* Song texts for the Amiga: lyrics are Windows-1250 (or UTF-8); the Amiga
 * shows ISO-8859-2 (with Czech fonts) or, safest, plain letters. */
#ifndef XG_TEXTMAP_H
#define XG_TEXTMAP_H

#include <stddef.h>
#include <stdint.h>

typedef struct { uint16_t u; char a[4]; } TmAscii;
extern const uint16_t tm_cp1250[128];
extern const uint16_t tm_latin2[96];
extern const TmAscii tm_ascii[];
extern const unsigned tm_ascii_n;

enum { TM_ASCII = 0, TM_LATIN2 = 1, TM_LATIN1 = 2 };

/* `in` (UTF-8 when it is valid UTF-8 with a multi-byte letter, else
 * Windows-1250) written for the Amiga into `out` (zero-ended); CR / LF are
 * kept. Returns the length. */
size_t tm_convert(const uint8_t *in, size_t n, char *out, size_t max, int letters);
/* Text typed on the Amiga (letters: how it is written) as Windows-1250 for
 * the song; returns the length. */
size_t tm_to_cp1250(const char *in, uint8_t *out, size_t max, int letters);

#endif
