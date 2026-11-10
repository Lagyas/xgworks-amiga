/* The Staff window of XGWorks Lite (notation to read; shared by both versions). */
#ifndef XG_STAFFWIN_H
#define XG_STAFFWIN_H

#include "app.h"

int  sw_open(App *a);
void sw_close(void);
int  sw_is_open(void);
unsigned long sw_signal(void);
int  sw_handle(App *a);
void sw_refresh(App *a);
void sw_position(App *a, uint32_t pos_us, int playing);

#endif
