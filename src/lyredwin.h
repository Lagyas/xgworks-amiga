/* The Lyrics editor window of XGWorks Lite (lines of the lyrics; shared by both versions). */
#ifndef XG_LYREDWIN_H
#define XG_LYREDWIN_H

#include "app.h"

int  yw_open(App *a);
void yw_close(void);
int  yw_is_open(void);
unsigned long yw_signal(void);
int  yw_handle(App *a);
void yw_refresh(App *a);

#endif
