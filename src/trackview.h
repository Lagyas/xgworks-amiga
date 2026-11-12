/* The Track View window of XGWorks Lite (tracks and their blocks; shared by both versions). */
#ifndef XG_TRACKVIEW_H
#define XG_TRACKVIEW_H

#include "app.h"

int  tv_open(App *a);
void tv_close(void);
int  tv_is_open(void);
unsigned long tv_signal(void);
int  tv_handle(App *a);
void tv_refresh(App *a);
/* The play cursor (and the page follows it while playing). */
void tv_position(App *a, uint32_t pos_us, int playing);

#endif
