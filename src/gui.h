/* The two windows of XGWorks Lite; each returns -1 when it cannot open
 * (no muimaster.library, no screen), else 0 after the window was closed. */
#ifndef XG_GUI_H
#define XG_GUI_H

#include "app.h"

int gui_mui(App *a);
int gui_gadtools(App *a);

#endif
