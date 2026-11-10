/* Lyrics and XF chords of a song for the karaoke window, by the rules of
 * XGWorks NextGen: XF marks ("/" new line, "<" new page at the start) and
 * .kar marks ("/" or "\" at the start, CR / LF, "/" at the end). Lyric
 * events (FF 05) of the track with the most of them; without any, the text
 * events (FF 01) of a .kar file (not the "@" header lines). */
#ifndef XG_LYRICS_H
#define XG_LYRICS_H

#include <stdint.h>
#include "smf.h"

typedef struct {
    uint32_t tick;
    uint32_t off;      /* its text in Lyrics.text */
    uint16_t line;
} LySyl;

typedef struct {
    uint32_t first, n; /* syllables */
} LyLine;

typedef struct {
    uint32_t tick;
    char name[16];
} LyChord;

typedef struct {
    LySyl *syl;
    uint32_t nsyl;
    LyLine *line;
    uint32_t nline;
    char *text;
    LyChord *chord;
    uint32_t nchord;
} Lyrics;

/* letters: TM_ASCII (plain), TM_LATIN2, TM_LATIN1 (textmap.h). 0 = no memory. */
int  ly_build(Lyrics *ly, const XgSong *s, int letters);
void ly_free(Lyrics *ly);
/* The last syllable / chord at or before `tick` (-1 = none yet). */
long ly_syl_at(const Lyrics *ly, uint32_t tick);
long ly_chord_at(const Lyrics *ly, uint32_t tick);
/* XF chord data (7 bytes: 43 7B 01 cr ct bn bt) as a name ("Am7", "C/E", "N.C."). */
void ly_chord_name(const uint8_t *d, char *out);

#endif
