/* The Lyrics / Karaoke window of XGWorks Lite (as XGWorks NextGen's Lyrics
 * window; shared by the MUI and GadTools versions). */
#ifndef XG_KARAWIN_H
#define XG_KARAWIN_H

#include "app.h"

int  kw_open(App *a);
void kw_close(void);
int  kw_is_open(void);
unsigned long kw_signal(void);
/* The song position (the preview and the Position line follow it). */
void kw_update(App *a, uint32_t pos_us, int force);
/* Its messages; 1 = the song changed. A request to move the song position
 * comes in a->locate_us. */
int  kw_handle(App *a, uint32_t pos_us);

#endif
