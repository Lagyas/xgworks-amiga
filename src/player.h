/* The player process of XGWorks Lite: plays a loaded song from a position
 * while the window stays usable. Tempo and transpose can change while it
 * plays; it signals the window task as the position moves and at the end. */
#ifndef XG_PLAYER_H
#define XG_PLAYER_H

#include <stdint.h>
#include "smf.h"
#include "mixer.h"

typedef struct {
    /* set before pl_start */
    const XgSong *song;
    uint32_t from_us;          /* song position to start at */
    const uint8_t *reset;      /* System On to send first (NULL = none) */
    uint32_t rlen;
    char device[40];           /* "serial.device", "camd", … */
    long unit;
    /* may change while playing */
    const Mixer *mixer;        /* NULL = the song as it is */
    volatile uint8_t tmute[64];/* tracks not heard (their notes are left out) */
    volatile int tempo;        /* percent, 25..400 */
    volatile int transpose;    /* half-tones, notes of every channel but 10 */
    /* set by the player */
    volatile uint32_t pos_us;  /* song position now */
    volatile int running;      /* the process is alive */
    volatile int ended;        /* reached the end of the song (not stopped) */
    char error[120];           /* why it could not play ("" = fine) */
    /* messages from the window, sent at once (written by the window only) */
    uint8_t q[64][16];
    uint8_t qn[64];
    volatile uint32_t qw, qr;
} Player;

/* The signal the player sends to the calling task (allocated by pl_init). */
int  pl_init(Player *p);
unsigned long pl_signal(void);
void pl_free(Player *p);
/* Start playing (stops a running play first); 0 = the process did not start. */
int  pl_start(Player *p);
/* Stop and wait until the process has ended. */
void pl_stop(Player *p);
/* While playing: send a message (short, or F0 … F7 up to 16 bytes) as soon as possible; 0 = the queue is full. */
int  pl_queue(Player *p, const uint8_t *msg, int n);

#endif
