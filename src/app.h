/* XGWorks Lite: what both windows (MUI and GadTools) share - the song list
 * of a folder, the loaded song, the player and the settings. */
#ifndef XG_APP_H
#define XG_APP_H

#include <stdint.h>
#include "smf.h"
#include "player.h"
#include "mixer.h"
#include "lyrics.h"
#include "edit.h"

#define UNDOS 12

#define RESETS 4   /* Off, XG, GM, GS */
#define OUTPUTS 2  /* serial.device, CAMD */

typedef struct {
    char folder[256];
    char **names;          /* song files of the folder, sorted */
    int count;
    int cur;               /* the loaded one, -1 = none */
    uint8_t *data;
    XgSong song;
    uint32_t len_us;
    Player pl;
    int reset;             /* 0 Off, 1 XG, 2 GM, 3 GS */
    int output;            /* 0 serial.device, 1 CAMD */
    char serdev[40];
    long unit;
    char info[160];        /* the status line */
    int autonext;          /* the next song after the end */
    Mixer mx;              /* the 16 parts of the loaded song */
    int out_open;          /* the window task holds the MIDI output (not playing) */
    Lyrics ly;             /* lyrics and chords of the loaded song */
    int letters;           /* TM_ASCII, TM_LATIN2, TM_LATIN1 */
    int kfont;             /* karaoke text size */
    int is_xws;            /* the loaded song is an .XWS (saved as .mid) */
    EdSong ed;             /* the song as it is edited */
    EdSong undo[UNDOS];    /* before the last edits (ring) */
    int nundo, undo_top;
    int dirty;             /* edited since loaded / saved */
    char path_as[256];     /* Save as: the file the song now belongs to ("" = its own) */
} App;

extern const char *const app_reset_names[RESETS + 1];
extern const char *const app_output_names[OUTPUTS + 1];

int  app_init(App *a, const char *folder);
void app_free(App *a);
/* Read a folder's songs (keeps the old list when it cannot be read). */
int  app_scan(App *a, const char *folder);
/* Load song k of the list; 0 = failed (info says why). */
int  app_load(App *a, int k);
/* Play the loaded song from `from_us`; Stop. */
int  app_play(App *a, uint32_t from_us);
void app_stop(App *a);
int  app_playing(const App *a);
/* After the player's signal: 1 = it ended at the song's end (go on). */
int  app_poll(App *a);
/* A file / a folder through the ASL requester (window: NULL or the window). */
int  app_ask_file(App *a, void *window);
int  app_ask_folder(App *a, void *window);
/* A mixer change: sets it and sends it (also while playing). */
void app_mix(App *a, int ch, int param, int value);
void app_mute(App *a, int ch, int mute, int solo);
/* Back to the song's own values. */
void app_mix_reset(App *a);
/* An effect setting (FX_REV / FX_CHO: type index, FX_REVRET / FX_CHORET: level), sent at once. */
void app_fx(App *a, int what, int value);
/* Write the mixer's values into the song file (song.mid.bak keeps the old one). */
int  app_save(App *a);
/* Save as another file (ASL requester); the song belongs to it from then on. */
int  app_save_as(App *a, void *window);
/* Editing: app_ed_begin before a change of a->ed (for Undo), app_ed_commit
 * after it (the player, mixer and lyrics take the new song; playing stops). */
void app_ed_begin(App *a);
int  app_ed_commit(App *a);
int  app_undo(App *a);
/* Track mute / solo into the player. */
void app_track_mute(App *a);
/* How lyrics are written: plain letters, ISO-8859-2 or Latin-1 (rebuilds them). */
void app_set_letters(App *a, int letters);
void app_set_tempo(App *a, int pct);
void app_set_transpose(App *a, int semis);
/* The guide (PROGDIR:XGWorksLite.guide) in MultiView, without waiting. */
void app_help(void);
/* "m:ss" */
void app_time(char *out, uint32_t us);
/* Settings in ENVARC:XGWorksLite.prefs (folder, reset, output, tempo). */
void app_load_prefs(App *a);
void app_save_prefs(const App *a);

#endif
