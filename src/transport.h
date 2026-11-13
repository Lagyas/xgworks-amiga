/* The Transport window of XGWorks Lite (as the PC / web version's): a big
 * location display (bar : beat : tick, time, tempo, signature, state), big
 * buttons (Top, -1 bar, +1 bar, Stop, Play, Rec, Loop, Click) and the L / R
 * locators (Set, Go); Loop plays L to R again and again. */
#ifndef XG_TRANSPORT_H
#define XG_TRANSPORT_H

#include "app.h"

int  tr_open(App *a);
void tr_close(void);
int  tr_is_open(void);
unsigned long tr_signal(void);
int  tr_handle(App *a);
void tr_refresh(App *a);
/* The song position (and the loop: back to L at R). */
void tr_position(App *a, uint32_t pos_us, int playing);

#endif
