/* The Job window of XGWorks Lite (shared by both versions). */
#ifndef XG_JOBWIN_H
#define XG_JOBWIN_H

#include "app.h"

int  jw_open(App *a);
void jw_close(void);
int  jw_is_open(void);
unsigned long jw_signal(void);
int  jw_handle(App *a);

#endif
