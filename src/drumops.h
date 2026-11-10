/* What the Drum editor does (no drawing; tested on the PC): hits are notes
 * on a step grid, one row per drum key. */
#ifndef XG_DRUMOPS_H
#define XG_DRUMOPS_H

#include "edit.h"

extern const char *const dr_gm_names[128];   /* GM drum names (35-81), the rest NULL */

/* Keys with hits in the track (channel ch, -1 all): used[k] = number of hits; returns how many keys. */
int  dr_keys_used(const EdSong *s, uint32_t trk, int ch, uint32_t used[128]);
/* The hit of `key` in [t0, t1), -1 = none. */
long dr_hit(const EdSong *s, uint32_t trk, int ch, int key, uint32_t t0, uint32_t t1);
/* Toggle the hit in the step [t, t + step): removes it, else adds a note
 * (length step / 2, at least 1). Returns +1 added, -1 removed, 0 no memory. */
int  dr_toggle(EdSong *s, uint32_t trk, int ch, int key, uint32_t t, uint32_t step, int vel);

#endif
