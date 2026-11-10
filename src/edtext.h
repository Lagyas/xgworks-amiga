/* Events as text lines for the Event List editor and back:
 *   "0012:1:000  1  Note      C3   100   480"
 *   position (bar:beat:tick), channel 1-16 (or "-"), kind, values.
 * Kinds: Note (key, velocity, length), NoteOff, PolyPress, Ctrl (number,
 * value), Prog (1-128), ChPress, Pitch (-8192..8191), SysEx (hex bytes),
 * Tempo (BPM), TimeSig (4/4), Text / Lyric / Marker / Name / Cue ("..."),
 * Chord (XF, e.g. Am7), Meta (type and hex). Keys as C3 = 60 (Yamaha). */
#ifndef XG_EDTEXT_H
#define XG_EDTEXT_H

#include "edit.h"

/* One event as a line (letters: textmap TM_*); returns `out`. */
char *et_format(const EdSong *s, const EdEv *e, char *out, int max, int letters);
/* A typed line into an event (data of SysEx / meta go into the song's pool);
 * 0 = not understood (`err` says why). `ch` = the channel when none is typed. */
int et_parse(EdSong *s, const char *line, EdEv *e, int ch, int letters, char *err, int errmax);
/* Key name (C3 = 60) and back (-1 = not a key). */
void et_key(int k, char *out);
int  et_key_of(const char *s);

#endif
