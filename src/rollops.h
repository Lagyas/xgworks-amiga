/* What the Piano Roll does to the notes of a track (no drawing; tested on the PC).
 * `ch` = the channel shown (-1 = all). The selection is EdEv.sel. */
#ifndef XG_ROLLOPS_H
#define XG_ROLLOPS_H

#include "edit.h"

int  ro_is_note(const EdEv *e, int ch);
/* The note at tick / key (the last one drawn there), -1 = none. */
long ro_hit(const EdSong *s, uint32_t trk, int ch, uint32_t tick, int key);
void ro_select_none(EdSong *s, uint32_t trk);
int  ro_select_all(EdSong *s, uint32_t trk, int ch);
/* Notes starting in [t0, t1) with keys k0..k1; add = keep the others. */
int  ro_select_rect(EdSong *s, uint32_t trk, int ch, uint32_t t0, uint32_t t1, int k0, int k1, int add);
int  ro_count_sel(const EdSong *s, uint32_t trk);
/* Move the selected notes by dt ticks and dk keys (kept in 0..127, not before 0). */
int  ro_move_sel(EdSong *s, uint32_t trk, long dt, int dk);
/* Lengthen / shorten the selected notes by dd ticks (at least `min`). */
int  ro_size_sel(EdSong *s, uint32_t trk, long dd, uint32_t min);
int  ro_vel_sel(EdSong *s, uint32_t trk, int vel);
int  ro_delete_sel(EdSong *s, uint32_t trk);
/* A new note, the only one selected; returns its index (-1 = no memory). */
long ro_add_note(EdSong *s, uint32_t trk, int ch, uint32_t tick, int key, int vel, uint32_t dur);
/* Copy the selected notes `dt` ticks later (the copies selected); returns how many. */
int  ro_copy_sel(EdSong *s, uint32_t trk, long dt);
/* Grid: tick rounded down to the grid g (g <= 1: as it is). */
uint32_t ro_snap(uint32_t tick, uint32_t g);

#endif
