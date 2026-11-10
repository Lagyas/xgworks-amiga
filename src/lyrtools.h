/* Lyrics and chord tools of XGWorks Lite (as in XGWorks NextGen; no
 * drawing, tested on the PC):
 *  - text onto notes: syllables (words at spaces, syllables at "-", "/" a
 *    new line, "<" a new page; in a file a line end is a new line and an
 *    empty line a new page) on the next note starts of a track;
 *  - chords onto bars / half bars / beats ("C G Am7 F/A", "-" or "." keeps a
 *    step empty);
 *  - transposing all chords; Midimusic Chord and Lyrics Protocol text out
 *    and in; removing diacritics from lyrics and texts. */
#ifndef XG_LYRTOOLS_H
#define XG_LYRTOOLS_H

#include <stddef.h>
#include "edit.h"

/* The track with the most lyric events (-1 none) / the first track (chords go there). */
int  lt_lyrics_track(const EdSong *s);

/* Text (Amiga letters `letters`) onto the notes of `note_trk` (channel ch,
 * -1 all) from `from`, as lyric events in `lyr_trk`; the lyrics there from
 * `from` up to the last one placed are replaced. Returns the syllables placed
 * (`left` = syllables without a note). */
int  lt_text_to_notes(EdSong *s, uint32_t lyr_trk, uint32_t note_trk, int ch, uint32_t from, const char *text, int letters, int *left);

/* Chords onto steps (0 bar, 1 half bar, 2 beat) from bar `bar`; the chords
 * in the range are replaced. Returns the chords placed; `bad` = names not understood. */
int  lt_chords_to_bars(EdSong *s, uint32_t trk, uint32_t bar, int step, const char *seq, int *bad);

/* Every XF chord moved by `semis` half-tones. Returns how many. */
int  lt_transpose_chords(EdSong *s, int semis);

/* Midimusic text (Windows-1250, CRLF lines; malloc'ed). mode: 0 lyrics + chords, 1 lyrics, 2 chords. */
char *lt_export(const EdSong *s, int lyr_trk, const char *file, int mode, size_t *n);
/* Read a Midimusic text: lyrics into lyr_trk, chords into chord_trk;
 * replace = the old ones go first. Returns lines read; `bad` = lines not understood. */
int  lt_import(EdSong *s, uint32_t lyr_trk, uint32_t chord_trk, const char *text, size_t n, int replace, int *bad);

/* The lyric lines of a track (the marks as in the karaoke window): idx gets
 * the lyric event indices in order, lines their ranges in idx. Returns the lines. */
typedef struct { uint32_t first, n; } LtLine;
int  lt_lines(const EdSong *s, uint32_t trk, uint32_t *idx, uint32_t maxidx, LtLine *lines, int maxlines);
/* A line as text: syllables joined ("-" inside a word), marks left out. */
void lt_line_text(const EdSong *s, uint32_t trk, const uint32_t *idx, const LtLine *ln, char *out, int max, int letters);
/* A changed line: the same number of syllables keeps their times; else the
 * syllables go onto the notes of note_trk from the line's start up to the
 * next line (`next` = its first tick, 0xFFFFFFFF none). Returns the syllables written. */
int  lt_set_line(EdSong *s, uint32_t trk, const uint32_t *idx, const LtLine *ln, uint32_t next, const char *text, int letters, uint32_t note_trk, int *left);

/* Lyrics and text events without diacritics. Returns how many changed. */
int  lt_strip_diacritics(EdSong *s);

#endif
