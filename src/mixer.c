/* The 16-part mixer (see mixer.h). */
#include <stdlib.h>
#include <string.h>
#include "mixer.h"

const char *const mx_param_names[MX_N] = {"Volume", "Pan", "Reverb", "Chorus", "Bank MSB", "Bank LSB", "Program"};

/* General MIDI Level 1 sound names (MMA, public). */
const char *const mx_gm_names[128] = {
    "Grand Piano", "Bright Piano", "Electric Grand", "Honky-tonk", "E.Piano 1", "E.Piano 2", "Harpsichord", "Clavi",
    "Celesta", "Glockenspiel", "Music Box", "Vibraphone", "Marimba", "Xylophone", "Tubular Bells", "Dulcimer",
    "Drawbar Organ", "Perc. Organ", "Rock Organ", "Church Organ", "Reed Organ", "Accordion", "Harmonica", "Tango Accord.",
    "Nylon Guitar", "Steel Guitar", "Jazz Guitar", "Clean Guitar", "Muted Guitar", "Overdrive Gt", "Distortion Gt", "Gt Harmonics",
    "Acoustic Bass", "Finger Bass", "Pick Bass", "Fretless Bass", "Slap Bass 1", "Slap Bass 2", "Synth Bass 1", "Synth Bass 2",
    "Violin", "Viola", "Cello", "Contrabass", "Tremolo Str", "Pizzicato Str", "Harp", "Timpani",
    "Strings 1", "Strings 2", "Synth Str 1", "Synth Str 2", "Choir Aahs", "Voice Oohs", "Synth Voice", "Orchestra Hit",
    "Trumpet", "Trombone", "Tuba", "Muted Trumpet", "French Horn", "Brass Section", "Synth Brass 1", "Synth Brass 2",
    "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax", "Oboe", "English Horn", "Bassoon", "Clarinet",
    "Piccolo", "Flute", "Recorder", "Pan Flute", "Blown Bottle", "Shakuhachi", "Whistle", "Ocarina",
    "Square Lead", "Saw Lead", "Calliope", "Chiff Lead", "Charang", "Voice Lead", "Fifths Lead", "Bass & Lead",
    "New Age Pad", "Warm Pad", "Polysynth", "Choir Pad", "Bowed Pad", "Metallic Pad", "Halo Pad", "Sweep Pad",
    "Rain", "Soundtrack", "Crystal", "Atmosphere", "Brightness", "Goblins", "Echoes", "Sci-Fi",
    "Sitar", "Banjo", "Shamisen", "Koto", "Kalimba", "Bagpipe", "Fiddle", "Shanai",
    "Tinkle Bell", "Agogo", "Steel Drums", "Woodblock", "Taiko Drum", "Melodic Tom", "Synth Drum", "Reverse Cymbal",
    "Fret Noise", "Breath Noise", "Seashore", "Bird Tweet", "Telephone", "Helicopter", "Applause", "Gunshot",
};

/* Control change of each parameter (program: none). */
static const int CC[MX_N] = {7, 10, 91, 93, 0, 32, -1};
/* XG Multi Part address of each parameter (08 pp aa). */
static const uint8_t XGA[MX_N] = {0x0B, 0x0E, 0x13, 0x12, 0x01, 0x02, 0x03};
static const uint8_t DEF[MX_N] = {100, 64, 40, 0, 0, 0, 0};

void mx_init(Mixer *m)
{
    int c, p;
    memset(m, 0, sizeof *m);
    fx_init(&m->fx, 0);
    for (c = 0; c < 16; c++)
        for (p = 0; p < MX_N; p++) m->val[c][p] = DEF[p];
}

static int param_of_cc(int cc)
{
    int p;
    for (p = 0; p < MX_N; p++)
        if (CC[p] == cc) return p;
    return -1;
}

/* XG Multi Part change `43 1n 4C 08 pp aa vv F7`: the parameter (or -1), part and value. */
static int xg_part(const uint8_t *d, uint32_t len, int *part, int *val)
{
    int p;
    if (len != 8 || d[0] != 0x43 || (d[1] & 0xF0) != 0x10 || d[2] != 0x4C || d[3] != 0x08 || d[4] > 15) return -1;
    for (p = 0; p < MX_N; p++)
        if (XGA[p] == d[5]) {
            *part = d[4];
            *val = d[6];
            return p;
        }
    return -1;
}

/* The tick of each channel's first note (0xFFFFFFFF = none). */
static void first_notes(const XgSong *s, uint32_t fn[16])
{
    uint32_t i;
    for (i = 0; i < 16; i++) fn[i] = 0xFFFFFFFFUL;
    for (i = 0; i < s->nev; i++) {
        const XgEv *e = &s->ev[i];
        if (e->n == 3 && (e->msg[0] & 0xF0) == 0x90 && e->msg[2] && fn[e->msg[0] & 15] == 0xFFFFFFFFUL) fn[e->msg[0] & 15] = e->tick;
    }
}

void mx_from_song(Mixer *m, const XgSong *s)
{
    uint32_t fn[16], i;
    mx_init(m);
    fx_from_song(&m->fx, s);
    first_notes(s, fn);
    for (i = 0; i < 16; i++) m->used[i] = fn[i] != 0xFFFFFFFFUL;
    for (i = 0; i < s->nev; i++) {
        const XgEv *e = &s->ev[i];
        int ch, p, v;
        if (e->n == 0) {
            if (e->msg[0] == 0xF0 && (p = xg_part(s->buf + e->off, e->len, &ch, &v)) >= 0 && e->tick <= fn[ch]) m->val[ch][p] = (uint8_t)v;
            continue;
        }
        ch = e->msg[0] & 15;
        if (e->tick > fn[ch]) continue;
        if ((e->msg[0] & 0xF0) == 0xB0 && (p = param_of_cc(e->msg[1])) >= 0) m->val[ch][p] = e->msg[2];
        else if ((e->msg[0] & 0xF0) == 0xC0) m->val[ch][MX_PROG] = e->msg[1];
    }
}

int mx_set(Mixer *m, int ch, int param, int value, uint8_t out[3][3])
{
    int n = 0;
    if (ch < 0 || ch > 15 || param < 0 || param >= MX_N) return 0;
    if (value < 0) value = 0;
    if (value > 127) value = 127;
    m->val[ch][param] = (uint8_t)value;
    m->touched[ch][param] = 1;
    if (param >= MX_MSB) {
        /* a bank takes effect with the program change after it */
        out[n][0] = (uint8_t)(0xB0 | ch); out[n][1] = 0; out[n][2] = m->val[ch][MX_MSB]; n++;
        out[n][0] = (uint8_t)(0xB0 | ch); out[n][1] = 32; out[n][2] = m->val[ch][MX_LSB]; n++;
        out[n][0] = (uint8_t)(0xC0 | ch); out[n][1] = m->val[ch][MX_PROG]; out[n][2] = 0; n++;
    } else {
        out[n][0] = (uint8_t)(0xB0 | ch); out[n][1] = (uint8_t)CC[param]; out[n][2] = (uint8_t)value; n++;
    }
    return n;
}

int mx_audible(const Mixer *m, int ch)
{
    int c;
    if (m->mute[ch]) return 0;
    for (c = 0; c < 16; c++)
        if (m->solo[c]) return m->solo[ch];
    return 1;
}

int mx_any_touched(const Mixer *m)
{
    int c, p;
    for (c = 0; c < 16; c++)
        for (p = 0; p < MX_N; p++)
            if (m->touched[c][p]) return 1;
    for (p = 0; p < FX_N; p++)
        if (m->fx.touched[p]) return 1;
    return 0;
}

int mx_setup_sysex(const Mixer *m, uint8_t out[][16], int len[], int max)
{
    int w, n = 0;
    for (w = 0; w < FX_N && n < max; w++)
        if (m->fx.touched[w]) { len[n] = fx_msg(&m->fx, w, out[n]); n++; }
    return n;
}

int mx_filter(const Mixer *m, const uint8_t *msg, int n, uint8_t *out)
{
    int ch = msg[0] & 15, k = msg[0] & 0xF0, p;
    memcpy(out, msg, (size_t)n);
    if (msg[0] >= 0xF0) return 1;
    if (k == 0x90 && n == 3 && msg[2] && !mx_audible(m, ch)) return 0;
    if (k == 0xB0 && n == 3 && (p = param_of_cc(msg[1])) >= 0 && (m->touched[ch][p] || (p >= MX_MSB && m->touched[ch][MX_PROG]))) {
        out[2] = m->val[ch][p];
        return 1;
    }
    if (k == 0xC0 && (m->touched[ch][MX_PROG] || m->touched[ch][MX_MSB] || m->touched[ch][MX_LSB])) out[1] = m->val[ch][MX_PROG];
    return 1;
}

int mx_filter_sysex(const Mixer *m, const uint8_t *data, uint32_t len)
{
    int part, val, p = xg_part(data, len, &part, &val);
    if (!fx_filter_sysex(&m->fx, data, len)) return 0;
    return p < 0 || !m->touched[part][p];
}

int mx_setup(const Mixer *m, uint8_t out[][3], int max)
{
    int c, p, n = 0;
    for (c = 0; c < 16; c++) {
        int bank = m->touched[c][MX_MSB] || m->touched[c][MX_LSB] || m->touched[c][MX_PROG];
        for (p = 0; p < MX_MSB && n < max; p++)
            if (m->touched[c][p]) {
                out[n][0] = (uint8_t)(0xB0 | c); out[n][1] = (uint8_t)CC[p]; out[n][2] = m->val[c][p]; n++;
            }
        if (bank && n + 3 <= max) {
            out[n][0] = (uint8_t)(0xB0 | c); out[n][1] = 0; out[n][2] = m->val[c][MX_MSB]; n++;
            out[n][0] = (uint8_t)(0xB0 | c); out[n][1] = 32; out[n][2] = m->val[c][MX_LSB]; n++;
            out[n][0] = (uint8_t)(0xC0 | c); out[n][1] = m->val[c][MX_PROG]; out[n][2] = 0; n++;
        }
    }
    return n;
}

uint8_t *mx_save(const Mixer *m, const XgSong *s, size_t *out_size)
{
    uint32_t fn[16], last_reset = 0, after_reset, i;
    uint8_t done[16][MX_N], exists[16][MX_N];
    int have_reset = 0, trk_of[16], c, p;
    uint8_t *copy, *res;
    XgInsert ins[16 * MX_N + FX_N];
    uint32_t nins = 0;
    if (!(copy = (uint8_t *)malloc(s->size))) return NULL;
    memcpy(copy, s->buf, s->size);
    memset(done, 0, sizeof done);
    memset(exists, 0, sizeof exists);
    first_notes(s, fn);
    for (c = 0; c < 16; c++) trk_of[c] = -1;
    /* over the song's own setup messages (before each channel's first note) */
    for (i = 0; i < s->nev; i++) {
        const XgEv *e = &s->ev[i];
        int ch, v;
        if (e->n == 0) {
            if (e->msg[0] == 0xF0 && xg_is_reset(s->buf + e->off, e->len)) {
                last_reset = e->tick;
                have_reset = 1;
            }
            if (e->msg[0] == 0xF0 && (p = xg_part(s->buf + e->off, e->len, &ch, &v)) >= 0 && e->tick <= fn[ch]) {
                exists[ch][p] = 1;
                if (m->touched[ch][p]) {
                    copy[e->off + 6] = m->val[ch][p];
                    done[ch][p] = 1;
                }
            }
            continue;
        }
        ch = e->msg[0] & 15;
        if (trk_of[ch] < 0 || (fn[ch] != 0xFFFFFFFFUL && e->tick == fn[ch] && (e->msg[0] & 0xF0) == 0x90)) trk_of[ch] = e->trk;
        if (e->tick > fn[ch]) continue;
        if ((e->msg[0] & 0xF0) == 0xB0 && (p = param_of_cc(e->msg[1])) >= 0) {
            exists[ch][p] = 1;
            if (m->touched[ch][p]) {
                copy[e->off + 1] = m->val[ch][p];
                done[ch][p] = 1;
            }
        } else if ((e->msg[0] & 0xF0) == 0xC0) {
            exists[ch][MX_PROG] = 1;
            if (m->touched[ch][MX_PROG]) {
                copy[e->off] = m->val[ch][MX_PROG];
                done[ch][MX_PROG] = 1;
            }
        }
    }
    /* what the song does not set: new messages 100 ms after the last reset (a module
       needs a moment after a System On), never after the channel's first note */
    after_reset = xg_us_tick(s, xg_tick_us(s, last_reset) + 100000);
    /* what the song does not set: new messages there, before the first note */
    for (c = 0; c < 16; c++) {
        uint32_t t = have_reset ? after_reset : 0;
        int bank = 0;
        if (fn[c] != 0xFFFFFFFFUL && t > fn[c]) t = fn[c];
        for (p = 0; p < MX_N; p++) {
            XgInsert *x;
            if (!m->touched[c][p] || done[c][p]) continue;
            if (p >= MX_MSB) { bank = 1; continue; }
            x = &ins[nins++];
            x->trk = (uint16_t)(trk_of[c] < 0 ? 0 : trk_of[c]);
            x->tick = t;
            x->n = 3;
            x->b[0] = (uint8_t)(0xB0 | c); x->b[1] = (uint8_t)CC[p]; x->b[2] = m->val[c][p];
        }
        if (bank) {
            /* bank select + program, the ones the song lacks */
            static const int order[3] = {MX_MSB, MX_LSB, MX_PROG};
            int k;
            for (k = 0; k < 3; k++) {
                XgInsert *x;
                p = order[k];
                /* the song's own (written over above when changed) stays */
                if (exists[c][p]) continue;
                x = &ins[nins++];
                x->trk = (uint16_t)(trk_of[c] < 0 ? 0 : trk_of[c]);
                x->tick = t;
                if (p == MX_PROG) { x->n = 2; x->b[0] = (uint8_t)(0xC0 | c); x->b[1] = m->val[c][p]; }
                else { x->n = 3; x->b[0] = (uint8_t)(0xB0 | c); x->b[1] = (uint8_t)CC[p]; x->b[2] = m->val[c][p]; }
            }
        }
    }
    /* the effects: after the last reset, before the first note of the song */
    {
        uint32_t t = have_reset ? after_reset : 0, first = 0xFFFFFFFFUL;
        for (c = 0; c < 16; c++)
            if (fn[c] < first) first = fn[c];
        if (t > first) t = first;
        fx_save(&m->fx, s, copy, ins, &nins, t, 0);
    }
    res = xg_smf_rebuild(s, copy, ins, nins, out_size);
    free(copy);
    return res;
}
