/* The XGM part editor window of XGWorks Lite (shared by both versions). */
#ifndef XG_XGMWIN_H
#define XG_XGMWIN_H

#include "app.h"

int  xw_open(App *a);
/* The window for part (channel) `part`: its sound (program, bank) and parameters. */
int  xw_open_part(App *a, int part);
/* The same, with the drum kits listed (the Drum editor's Kit...). */
int  xw_open_kits(App *a, int part);
void xw_close(void);
int  xw_is_open(void);
unsigned long xw_signal(void);
/* 1 = a mixer value changed (the mixer window shows it again). */
int  xw_handle(App *a);
void xw_refresh(App *a);

#endif
