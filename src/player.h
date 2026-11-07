/* The player process of XGWorks Lite: plays a loaded song from a position
 * while the window stays usable. Tempo and transpose can change while it
 * plays; it signals the window task as the position moves and at the end. */
#ifndef XG_PLAYER_H
#define XG_PLAYER_H

#include <stdint.h>
#include "smf.h"

typedef struct {
    /* set before pl_start */
    const XgSong *song;
    uint32_t from_us;          /* song position to start at */
    const uint8_t *reset;      /* System On to send first (NULL = none) */
    uint32_t rlen;
    char device[40];           /* "serial.device", "camd", … */
    long unit;
    /* may change while playing */
    volatile int tempo;        /* percent, 25..400 */
    volatile int transpose;    /* half-tones, notes of every channel but 10 */
    /* set by the player */
    volatile uint32_t pos_us;  /* song position now */
    volatile int running;      /* the process is alive */
    volatile int ended;        /* reached the end of the song (not stopped) */
    char error[120];           /* why it could not play ("" = fine) */
} Player;

/* The signal the player sends to the calling task (allocated by pl_init). */
int  pl_init(Player *p);
unsigned long pl_signal(void);
void pl_free(Player *p);
/* Start playing (stops a running play first); 0 = the process did not start. */
int  pl_start(Player *p);
/* Stop and wait until the process has ended. */
void pl_stop(Player *p);

#endif
