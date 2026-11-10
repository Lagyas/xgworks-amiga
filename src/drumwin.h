/* The Drum editor window of XGWorks Lite (shared by both versions). */
#ifndef XG_DRUMWIN_H
#define XG_DRUMWIN_H

#include "app.h"

int  dw_open(App *a);
void dw_close(void);
int  dw_is_open(void);
unsigned long dw_signal(void);
int  dw_handle(App *a);
void dw_refresh(App *a);

#endif
