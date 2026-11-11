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
/* The Event List of a track. */
int  edw_events_open(App *a, int trk);
/* The track selected (in Tracks or the Track View; -1 = none). */
int  edw_selected_track(void);
void edw_select_track(App *a, int t);
/* The song position moved (the Event List marks the event there). */
void edw_position(App *a, uint32_t pos_us);

#endif
