/* XGWorks Lite: what both windows (MUI and GadTools) share - the song list
 * of a folder, the loaded song, the player and the settings. */
#ifndef XG_APP_H
#define XG_APP_H

#include <stdint.h>
#include "smf.h"
#include "player.h"

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
void app_set_tempo(App *a, int pct);
void app_set_transpose(App *a, int semis);
/* "m:ss" */
void app_time(char *out, uint32_t us);
/* Settings in ENVARC:XGWorksLite.prefs (folder, reset, output, tempo). */
void app_load_prefs(App *a);
void app_save_prefs(const App *a);

#endif
