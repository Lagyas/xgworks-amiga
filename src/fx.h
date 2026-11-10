/* XGWorks Lite: the song's reverb and chorus - type and return level, as
 * XG (System Effect 02 01 xx) or GS (40 01 3x) messages, by the song's
 * standard (GS when it has a GS reset or GS effect messages). */
#ifndef XG_FX_H
#define XG_FX_H

#include <stdint.h>
#include "smf.h"

enum { FX_REV, FX_CHO, FX_REVRET, FX_CHORET, FX_N };

typedef struct {
    uint8_t gs;             /* 1 = GS messages */
    uint8_t sel[2];         /* reverb / chorus type: index into fx_types() */
    uint8_t ret[2];         /* reverb / chorus return (level) */
    uint8_t touched[FX_N];
} Fx;

/* The type names of reverb (which 0) or chorus (1); returns their count. */
int  fx_types(int gs, int which, const char *const **names);
void fx_init(Fx *f, int gs);
void fx_from_song(Fx *f, const XgSong *s);
/* The message for one setting, F0 … F7; returns its length. */
int  fx_msg(const Fx *f, int what, uint8_t out[16]);
/* While playing: 0 = leave out (the song's message for a setting the user changed). */
int  fx_filter_sysex(const Fx *f, const uint8_t *data, uint32_t len);
/* Saving: the song's own messages written over in `copy`, the missing ones
 * added to `ins` at `tick` in track `trk`. */
void fx_save(const Fx *f, const XgSong *s, uint8_t *copy, XgInsert *ins, uint32_t *nins, uint32_t tick, uint16_t trk);

#endif
