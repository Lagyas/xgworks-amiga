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
/* GS names (SC-8850 map from the user's midi-file-editor.html in XG4) for
 * a GS song, else the XG lists. */
void vx_set_gs(int on);
int  vx_gs(void);
int  vx_gs_count(void);
/* GS voice i (SC-8850 list): its bank / program, the name; 0 = no more. */
int  vx_gs_get(int i, int *msb, int *lsb, int *prog, const char **name);
/* The GS drum kit of program `prog` (NULL = none). */
const char *vx_gs_kit(int prog);
/* The name of a channel's sound for display (never NULL): XG / GM2 / GS by
 * the song, the kit on channel 10, else the GM name. */
const char *vx_sound(int ch, int msb, int lsb, int prog);
/* The name of a sound (NULL = not in the lists). */
const char *vx_name(int msb, int lsb, int prog);
/* The name of a drum key in a kit (msb 126 / 127, prog), NULL = none. */
const char *vx_drum(int msb, int prog, int key);
/* The name of a drum key for a channel's bank / program, as the PC version:
 * channel 10 without a drum bank is the drum bank 127; the kit's name, else
 * the Standard Kit's, else the GM name, else NULL. */
const char *vx_drum_of(int ch, int msb, int prog, int key);
/* The Drum window's rows (XGworks' inst.ini [User], read with the lists):
 * order[row] = key, top row first; all 128 keys. Without inst.ini the
 * highest key first. */
void vx_drum_order(uint8_t order[128]);
/* The kind of a drum key (inst.ini InstColor): 0 snare / brush, 1 percussion,
 * 2 kick, 3 hi-hat, 4 cymbal, 5 tom. */
int  vx_drum_kind(int key);

#endif
