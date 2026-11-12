/* XGWorks Lite: the colours of the PC / web version for the drawn editors
 * (Track View, Piano Roll, Drums). Obtained from the screen (shared pens);
 * on a screen with few colours the nearest ones, on 4 / 8 colours none
 * (pal() gives the screen's pen then). */
#ifndef XG_PAL_H
#define XG_PAL_H

#include <exec/types.h>
#include <intuition/screens.h>

enum {
    /* the most needed first (on 16 colours the later ones share) */
    PAL_WHITE, PAL_BLACK, PAL_LANE, PAL_HEAD, PAL_ROW, PAL_BLOCK, PAL_RED, PAL_NOTE,
    PAL_BARLINE, PAL_GRID, PAL_RULER, PAL_KEYROW, PAL_LIGHT, PAL_EDGE, PAL_LINE, PAL_BEAT,
    PAL_SOLO, PAL_MUTE, PAL_CYAN,
    /* the drum kinds (XGworks' inst.ini InstColor): snare, percussion, kick, hi-hat, cymbal, tom */
    PAL_K_SNARE, PAL_K_PERC, PAL_K_KICK, PAL_K_HAT, PAL_K_CYM, PAL_K_TOM,
    PAL_N
};

/* A window opens / closes on `scr` (counted: the pens stay while any is open). */
void pal_open(struct Screen *scr);
void pal_close(void);
/* The pen of colour c, else `def`. */
UWORD pal(int c, UWORD def);
/* Are the colours there (else the windows draw with the screen's pens)? */
int pal_ok(void);

#endif
