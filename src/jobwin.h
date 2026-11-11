/* The Job window of XGWorks Lite (shared by both versions). */
#ifndef XG_JOBWIN_H
#define XG_JOBWIN_H

#include "app.h"

int  jw_open(App *a);
/* With a job chosen (0 Transpose, 13 Demix by Channel, 14 Demix by Note ...). */
int  jw_open_job(App *a, int job);
void jw_close(void);
int  jw_is_open(void);
unsigned long jw_signal(void);
int  jw_handle(App *a);

#endif
