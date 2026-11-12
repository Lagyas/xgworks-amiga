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
/* The standard of the sound names and the sound choice (as the web
 * version's): GM, GM2, the GS maps, the PSR-SX900, XG (the XGworks lists)
 * and XG MU1000 / MU2000. GS / SX900 / MU1000 come from the user's
 * midi-file-editor.html in the XG4 folder (else GM names). */
enum { VX_XG, VX_GM, VX_GM2, VX_GS55, VX_GS88, VX_GS88PRO, VX_GS8850, VX_SX900, VX_MU1000, VX_STDS };
#define VX_GS VX_GS8850
extern const char *const vx_std_names[VX_STDS + 1];
void vx_set_std(int std);
int  vx_std(void);
/* GS songs: the SC-8850 (on) or XG (off). */
void vx_set_gs(int on);
int  vx_gs(void);                 /* a GS standard now */
int  vx_gs_count(void);           /* the sounds of the html sets */
/* Sound i of a standard's set (GS, SX900, MU1000): bank / program, name; 0 = no more. */
int  vx_set_get(int std, int i, int *msb, int *lsb, int *prog, const char **name);
/* The drum kits of a standard: kit k (0-) -> bank MSB, program, name; 0 = no more. */
int  vx_kit(int std, int k, int *msb, int *prog, const char **name);
/* The name of a channel's sound for display (never NULL), by the standard;
 * the kit on channel 10; else the GM name. */
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
