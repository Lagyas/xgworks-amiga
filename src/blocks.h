/* Blocks of the Track View (as XGWorks NextGen's: a block owns the events
 * from its start to the next block's start, the first one also those before
 * it). No drawing; tested on the PC. */
#ifndef XG_BLOCKS_H
#define XG_BLOCKS_H

#include "edit.h"

/* A copied block: its events (times relative to the copy) with their data. */
typedef struct {
    int trk;
    char name[40];
    uint32_t start, end;
    EdEv *ev;
    uint32_t n;
    uint8_t *data;      /* the meta / SysEx bytes of ev (ev[i].data = offset here) */
    uint32_t ndata;
} BkClip;

/* A track without blocks gets one over all its events (whole bars of `bar` ticks). */
void bk_materialize(EdSong *s, uint32_t trk, uint32_t bar);
uint32_t bk_owner(const EdTrack *t, uint32_t tick);
/* The blocks a track shows (its own, or the one it would get): count into *out (max). */
int  bk_view(const EdSong *s, uint32_t trk, uint32_t bar, EdBlock *out, int max);
int  bk_create(EdSong *s, uint32_t trk, uint32_t start, uint32_t end, const char *name, uint32_t bar);
void bk_remove(EdSong *s, uint32_t trk, uint32_t bi);
int  bk_take(const EdSong *s, uint32_t trk, uint32_t bi, uint32_t t0, BkClip *c);
/* Paste at `at` (+ its relative start); returns the new block's index (-1 = no memory). */
long bk_paste(EdSong *s, uint32_t trk, const BkClip *c, uint32_t at, uint32_t bar);
void bk_clip_free(BkClip *c);
void bk_rename(EdSong *s, uint32_t trk, uint32_t bi, const char *name);
/* Block Repeat: the blocks wholly in [from, to) of every track copied to `at`, `count` times. */
int  bk_repeat(EdSong *s, uint32_t from, uint32_t to, uint32_t at, uint32_t count, uint32_t bar);
/* Move block bi of trk by delta ticks (not before 0); returns its new index. */
long bk_move(EdSong *s, uint32_t trk, uint32_t bi, long delta, uint32_t bar);

#endif
