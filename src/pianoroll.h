/* The Piano Roll window of XGWorks Lite (shared by the MUI and GadTools versions). */
#ifndef XG_PIANOROLL_H
#define XG_PIANOROLL_H

#include "app.h"

/* Open for the track selected in the Tracks window (else the first with notes). */
int  pr_open(App *a);
void pr_close(void);
int  pr_is_open(void);
unsigned long pr_signal(void);
/* Its messages; 1 = the song changed. */
int  pr_handle(App *a);
/* After the song was loaded / changed elsewhere. */
void pr_refresh(App *a);
/* The song position (the cursor line; while playing the view follows). */
void pr_position(App *a, uint32_t pos_us, int playing);

#endif
