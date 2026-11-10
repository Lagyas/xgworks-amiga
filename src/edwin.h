/* The editor windows of XGWorks Lite (GadTools; shared by both versions):
 * Tracks (the song's tracks: name, channel, mute / solo, new / delete,
 * Undo, Save, Save as) and the Event List of a track. */
#ifndef XG_EDWIN_H
#define XG_EDWIN_H

#include "app.h"

int  edw_tracks_open(App *a);
void edw_close_all(void);
unsigned long edw_signal(void);
/* Messages of the editor windows; returns 1 when the song changed. */
int  edw_handle(App *a);
/* After a song was loaded / changed elsewhere. */
void edw_refresh(App *a);
/* The track selected in the Tracks window (-1 = none / closed). */
int  edw_selected_track(void);
/* The song position moved (the Event List marks the event there). */
void edw_position(App *a, uint32_t pos_us);

#endif
