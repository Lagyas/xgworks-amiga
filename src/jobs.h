/* The Job menu of XGworks (as XGWorks NextGen's jobs.rs, after JobDll.dll):
 * Transpose, Velocity / Gate Time Modify, Shift Clock, Sort / Separate
 * Chord, Thin Out, Quantize, Create / Delete Measure, Transpose Chord,
 * Modify Control Data, Stretch Time; Demix by channel / by note.
 * No drawing; tested on the PC against the Rust core. */
#ifndef XG_JOBS_H
#define XG_JOBS_H

#include "edit.h"

/* What a job works on: a track (or all, trk < 0), ticks [from, to), keys lo..hi. */
typedef struct {
    int trk;
    uint32_t from, to;
    uint8_t lo, hi;
} JbScope;

/* Absolute value, or relative: value x rate / 100 + offset. */
typedef struct {
    int absolute, value, rate, offset;
} JbMod;

enum { JB_POLYAT, JB_CC, JB_CHAT, JB_BEND };

typedef struct {
    int grid;       /* 0 1/32, 1 1/16 T, 2 1/16, 3 1/8 T, 4 1/8, 5 1/4 T, 6 1/4, 7 1/8 + 1/8 T, 8 1/16 + 1/16 T */
    int strength;   /* 0..100 % */
    int sens;       /* -100..100 % */
    int swing;      /* 50..100 % */
    int gate, vel;  /* 0..200 % for the swung notes */
} JbQntz;

extern const char *const jb_qntz_names[9];

int  jb_transpose(EdSong *s, const JbScope *sc, int semis);
int  jb_velocity(EdSong *s, const JbScope *sc, JbMod m);
int  jb_gate(EdSong *s, const JbScope *sc, JbMod m);
int  jb_shift(EdSong *s, const JbScope *sc, int clocks);
void jb_sort_chord(EdSong *s, const JbScope *sc, int reverse);
void jb_separate_chord(EdSong *s, const JbScope *sc, int clocks);
int  jb_modify_control(EdSong *s, const JbScope *sc, int kind, int cc_from, int cc_to, JbMod m);
int  jb_thin_out(EdSong *s, const JbScope *sc, int kind, int cc);
int  jb_quantize(EdSong *s, const JbScope *sc, JbQntz q);
int  jb_stretch(EdSong *s, const JbScope *sc, uint32_t stretch, int offset);
/* `count` bars of num/den before the bar starting at `at` (old_num/old_den rules there now). */
void jb_create_measure(EdSong *s, uint32_t at, int num, int den, uint32_t count, int old_num, int old_den);
void jb_delete_measure(EdSong *s, uint32_t from, uint32_t to);
int  jb_transpose_chord(EdSong *s, uint32_t from, uint32_t to, int semis);

/* Demix: track `trk` into one track per channel (its meta / SysEx stay in a
 * track of their own), or the notes of `channels` (bit k = channel k+1) into
 * one track per channel and key. label(ch, key) may name a key (NULL = by
 * the rules: GM drum names on channel 10, else "Note C3"). Returns the new tracks. */
int  jb_demix_channels(EdSong *s, uint32_t trk);
int  jb_demix_notes(EdSong *s, uint32_t trk, uint16_t channels, const char *(*label)(int ch, int key));

#endif
