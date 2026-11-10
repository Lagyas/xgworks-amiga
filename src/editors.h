/* All editor windows (Tracks / Event List, Piano Roll, Drums, Texts) for the main windows. */
#ifndef XG_EDITORS_H
#define XG_EDITORS_H

#include "app.h"

unsigned long eds_signal(void);
/* Messages of the windows whose signals are in `sigs`; 1 = the song changed (all refreshed). */
int  eds_handle(App *a, unsigned long sigs);
void eds_refresh(App *a);
void eds_close(void);

#endif
