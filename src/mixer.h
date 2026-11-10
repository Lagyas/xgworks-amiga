/* XGWorks Lite: the 16-part mixer. The values come from the song's setup
 * (the messages before each channel's first note), changes go out at once
 * and win over the song's own messages while it plays, and Save writes them
 * into the song: over its own messages, else as new ones after the reset. */
#ifndef XG_MIXER_H
#define XG_MIXER_H

#include <stddef.h>
#include <stdint.h>
#include "smf.h"

enum { MX_VOL, MX_PAN, MX_REV, MX_CHO, MX_MSB, MX_LSB, MX_PROG, MX_N };

typedef struct {
    uint8_t val[16][MX_N];
    uint8_t touched[16][MX_N];   /* changed by the user: wins over the song */
    uint8_t mute[16], solo[16];
    uint8_t used[16];            /* the channel has notes */
} Mixer;

extern const char *const mx_param_names[MX_N];
extern const char *const mx_gm_names[128];

void mx_init(Mixer *m);
void mx_from_song(Mixer *m, const XgSong *s);
/* Set a value (marks it as the user's); the messages to send: returns their count. */
int  mx_set(Mixer *m, int ch, int param, int value, uint8_t out[3][3]);
int  mx_audible(const Mixer *m, int ch);
int  mx_any_touched(const Mixer *m);
/* While playing: 0 = leave the message out, 1 = send `out` (n bytes in *n). */
int  mx_filter(const Mixer *m, const uint8_t *msg, int n, uint8_t *out);
/* SysEx while playing: 0 = leave out (an XG part setting the user has changed). */
int  mx_filter_sysex(const Mixer *m, const uint8_t *data, uint32_t len);
/* The user's values as messages (sent when playing starts); returns the count. */
int  mx_setup(const Mixer *m, uint8_t out[][3], int max);
/* The song with the user's values written in (NULL = no memory). */
uint8_t *mx_save(const Mixer *m, const XgSong *s, size_t *out_size);

#endif
