/* The Texts window of XGWorks Lite: lyrics and chord tools (shared by both versions). */
#ifndef XG_LYRWIN_H
#define XG_LYRWIN_H

#include "app.h"

int  lw_open(App *a);
void lw_close(void);
int  lw_is_open(void);
unsigned long lw_signal(void);
int  lw_handle(App *a);
void lw_refresh(App *a);

#endif
