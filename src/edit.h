/* XGWorks Lite: the song as it is edited - tracks of events, notes with a
 * length (note-on and note-off paired), SysEx / meta data in a pool. Read
 * from and written to a Standard MIDI File (format 1); the player and the
 * mixer work on that file, made again after an edit. */
#ifndef XG_EDIT_H
#define XG_EDIT_H

#include <stddef.h>
#include <stdint.h>

/* st: 0x80..0xEF channel message (0x9n = a note with dur), 0xF0 / 0xF7 SysEx, 0xFF meta (d1 = type). */
typedef struct {
    uint32_t tick;
    uint32_t dur;      /* note: length in ticks */
    uint32_t data;     /* SysEx / meta: offset of the data in the pool */
    uint32_t len;      /* SysEx / meta: data length */
    uint8_t  st, d1, d2;
    uint8_t  offv;     /* note: 0x80 | velocity of a note-off (0x8n), 0 = note-on velocity 0 */
} EdEv;

typedef struct {
    EdEv *ev;
    uint32_t n, cap;
    uint32_t end;      /* tick of its end (End of Track) */
    uint8_t mute, solo;
} EdTrack;

typedef struct {
    EdTrack *trk;
    uint32_t ntrk;
    uint16_t ppqn;
    uint8_t *pool;
    uint32_t npool, cappool;
} EdSong;

int  ed_load(EdSong *s, const uint8_t *smf, size_t n);
/* The song as a format-1 MIDI file (malloc'ed). */
uint8_t *ed_save(const EdSong *s, size_t *n);
void ed_free(EdSong *s);
int  ed_copy(EdSong *dst, const EdSong *src);

/* Events stay in time order; a new one goes after those at the same tick. */
int  ed_insert(EdSong *s, uint32_t trk, const EdEv *e);
void ed_delete(EdSong *s, uint32_t trk, uint32_t i);
/* Replace event i (moved to its place by time); returns its new index. */
long ed_replace(EdSong *s, uint32_t trk, uint32_t i, const EdEv *e);
/* Data for a new SysEx / meta event; returns its pool offset (or 0xFFFFFFFF). */
uint32_t ed_pool_add(EdSong *s, const uint8_t *d, uint32_t n);
int  ed_add_track(EdSong *s, uint32_t at);
void ed_del_track(EdSong *s, uint32_t t);

/* The track's name (FF 03) and channel (of its first channel message, -1 none). */
const uint8_t *ed_name(const EdSong *s, uint32_t trk, uint32_t *len);
int  ed_channel(const EdSong *s, uint32_t trk);
/* Set the name (FF 03 at tick 0); move all channel messages to channel `ch`. */
int  ed_set_name(EdSong *s, uint32_t trk, const uint8_t *name, uint32_t len);
void ed_set_channel(EdSong *s, uint32_t trk, int ch);

/* Bars: tick <-> bar (1-), beat (1-), tick in the beat, by the time signatures (FF 58). */
void ed_mbt(const EdSong *s, uint32_t tick, uint32_t *bar, uint32_t *beat, uint32_t *t);
uint32_t ed_tick_of(const EdSong *s, uint32_t bar, uint32_t beat, uint32_t t);

#endif
