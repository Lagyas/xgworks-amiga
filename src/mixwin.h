/* The mixer window of XGWorks Lite (MUI and GadTools versions). */
#ifndef XG_MIXWIN_H
#define XG_MIXWIN_H

#include "app.h"

/* MUI: the window object (a SubWindow of `app`), its return IDs are
 * MIXID_BASE and up; handle them with mixmui_handle. */
#define MIXID_BASE 1000
void *mixmui_create(App *a, void *mui_app);
int   mixmui_handle(App *a, unsigned long id);
void  mixmui_refresh(App *a);
void  mixmui_open(int open);

/* GadTools: a window of its own. */
int   mixgt_open(App *a);
void  mixgt_close(void);
int   mixgt_is_open(void);
unsigned long mixgt_signal(void);
/* Its messages; 0 = the window was closed. */
int   mixgt_handle(App *a);
void  mixgt_refresh(App *a);

#endif
