/* Sound names from the XGworks V4.0 voice lists, read at run time from the
 * user's XG4 folder (Melody.ini MSB 0 with variations, SFX.ini MSB 64,
 * SFX Kit.ini MSB 126, Drum Kit.ini MSB 127 with the drum names of each
 * kit). Without them: General MIDI names. Nothing of the lists is built in. */
#ifndef XG_VOICES_H
#define XG_VOICES_H

#include <stdint.h>

typedef struct {
    uint8_t msb, lsb, prog;
    uint32_t name;          /* offset in the names */
} VxVoice;

/* Read the lists from `dir` (e.g. "PROGDIR:XG4"); returns the voices read (0 = none: GM names). */
int  vx_load(const char *dir);
void vx_free(void);
int  vx_count(void);
const VxVoice *vx_get(int i);
const char *vx_text(uint32_t off);
/* The name of a sound (NULL = not in the lists). */
const char *vx_name(int msb, int lsb, int prog);
/* The name of a drum key in a kit (msb 126 / 127, prog), NULL = none. */
const char *vx_drum(int msb, int prog, int key);

#endif
