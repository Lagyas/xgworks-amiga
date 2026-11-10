/* The karaoke window of XGWorks Lite (shared by the MUI and GadTools versions). */
#ifndef XG_KARAWIN_H
#define XG_KARAWIN_H

#include "app.h"

int  kw_open(App *a);
void kw_close(void);
int  kw_is_open(void);
unsigned long kw_signal(void);
/* Draw for the song position (only what changed, unless `force`). */
void kw_update(App *a, uint32_t pos_us, int force);
/* Its messages; 0 = the window was closed. */
int  kw_handle(App *a, uint32_t pos_us);

#endif
