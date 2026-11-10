/* XGWorks Lite: the 16-part mixer. The values come from the song's setup
 * (the messages before each channel's first note), changes go out at once
 * and win over the song's own messages while it plays, and Save writes them
 * into the song: over its own messages, else as new ones after the reset. */
#ifndef XG_MIXER_H
#define XG_MIXER_H

#include <stddef.h>
#include <stdint.h>
#include "smf.h"
#include "fx.h"

enum { MX_VOL, MX_PAN, MX_REV, MX_CHO, MX_MSB, MX_LSB, MX_PROG, MX_N };

/* XG Multi Part parameters of the part editor (08 pp aa). */
enum { XP_SHIFT, XP_VARSEND, XP_VRATE, XP_VDEPTH, XP_VDELAY, XP_CUTOFF, XP_RESO, XP_ATTACK, XP_DECAY, XP_RELEASE, XP_BEND, XP_N };
typedef struct {
    uint8_t addr, def, min, max;
    int8_t  shown;       /* shown as value - shown (64: -64..+63) */
    const char *name;
} MxXp;
extern const MxXp mx_xp[XP_N];

typedef struct {
    uint8_t val[16][MX_N];
    uint8_t touched[16][MX_N];   /* changed by the user: wins over the song */
    uint8_t mute[16], solo[16];
    uint8_t used[16];            /* the channel has notes */
    Fx fx;                       /* reverb / chorus of the song */
    uint8_t xp[16][XP_N];        /* XG part parameters */
    uint8_t xptouched[16][XP_N];
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
/* An XG part parameter as SysEx (F0 … F7); returns its length. */
int  mx_xp_msg(const Mixer *m, int ch, int p, uint8_t out[16]);
/* The user's effect and part settings as SysEx (F0 … F7, length in len[]); returns the count. */
int  mx_setup_sysex(const Mixer *m, uint8_t out[][16], int len[], int max);
/* After the song changed: the user's values (touched), mute / solo and effects of `old` kept. */
void mx_keep_user(Mixer *m, const Mixer *old);
/* The song with the user's values written in (NULL = no memory). */
uint8_t *mx_save(const Mixer *m, const XgSong *s, size_t *out_size);

#endif
