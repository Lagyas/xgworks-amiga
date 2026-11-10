/* Reverb and chorus of the song (see fx.h). Type numbers and names after
 * the XG and GS format specifications. */
#include <string.h>
#include "fx.h"

typedef struct { uint8_t msb, lsb; } Type;

static const Type XG_REV[] = {{0, 0}, {1, 0}, {1, 1}, {2, 0}, {2, 1}, {2, 2}, {3, 0}, {3, 1}, {4, 0}, {16, 0}, {17, 0}, {18, 0}, {19, 0}};
static const char *const XG_REV_N[] = {"No effect", "Hall 1", "Hall 2", "Room 1", "Room 2", "Room 3", "Stage 1", "Stage 2", "Plate",
                                       "White Room", "Tunnel", "Canyon", "Basement", NULL};
static const Type XG_CHO[] = {{0, 0}, {65, 0}, {65, 1}, {65, 2}, {65, 8}, {66, 0}, {66, 1}, {66, 2}, {66, 8}, {67, 0}, {67, 1}, {67, 8}, {68, 0}, {72, 0}, {72, 8}};
static const char *const XG_CHO_N[] = {"No effect", "Chorus 1", "Chorus 2", "Chorus 3", "Chorus 4", "Celeste 1", "Celeste 2", "Celeste 3",
                                       "Celeste 4", "Flanger 1", "Flanger 2", "Flanger 3", "Symphonic", "Phaser 1", "Phaser 2", NULL};
static const char *const GS_REV_N[] = {"Room 1", "Room 2", "Room 3", "Hall 1", "Hall 2", "Plate", "Delay", "Panning Delay", NULL};
static const char *const GS_CHO_N[] = {"Chorus 1", "Chorus 2", "Chorus 3", "Chorus 4", "Feedback Chorus", "Flanger", "Short Delay", "Short Delay (FB)", NULL};

#define NXR (int)(sizeof XG_REV / sizeof XG_REV[0])
#define NXC (int)(sizeof XG_CHO / sizeof XG_CHO[0])

int fx_types(int gs, int which, const char *const **names)
{
    if (gs) { *names = which ? GS_CHO_N : GS_REV_N; return 8; }
    *names = which ? XG_CHO_N : XG_REV_N;
    return which ? NXC : NXR;
}

void fx_init(Fx *f, int gs)
{
    memset(f, 0, sizeof *f);
    f->gs = (uint8_t)gs;
    f->sel[0] = gs ? 4 : 1;   /* Hall 2 / Hall 1 */
    f->sel[1] = gs ? 2 : 1;   /* Chorus 3 / Chorus 1 */
    f->ret[0] = f->ret[1] = 64;
}

static int find(const Type *t, int n, int msb, int lsb)
{
    int k;
    for (k = 0; k < n; k++)
        if (t[k].msb == msb && t[k].lsb == lsb) return k;
    for (k = 0; k < n; k++)
        if (t[k].msb == msb) return k;
    return 0;
}

/* XG System Effect 43 1n 4C 02 01 aa …: the address (or -1) */
static int xg_addr(const uint8_t *d, uint32_t len)
{
    if (len < 8 || d[0] != 0x43 || (d[1] & 0xF0) != 0x10 || d[2] != 0x4C || d[3] != 0x02 || d[4] != 0x01) return -1;
    return d[5];
}

/* GS 41 dd 42 12 40 01 aa vv cs F7: the address (or -1) */
static int gs_addr(const uint8_t *d, uint32_t len)
{
    if (len < 10 || d[0] != 0x41 || d[2] != 0x42 || d[3] != 0x12 || d[4] != 0x40 || d[5] != 0x01) return -1;
    return d[6];
}

static int gs_reset(const uint8_t *d, uint32_t len)
{
    return len >= 9 && d[0] == 0x41 && d[2] == 0x42 && d[3] == 0x12 && d[4] == 0x40 && d[5] == 0x00 && d[6] == 0x7F;
}

void fx_from_song(Fx *f, const XgSong *s)
{
    uint32_t i;
    int gs = 0, rm = -1, rl = 0, cm = -1, cl = 0;
    for (i = 0; i < s->nev; i++) {
        const XgEv *e = &s->ev[i];
        const uint8_t *d = s->buf + e->off;
        if (e->n || e->msg[0] != 0xF0) continue;
        if (gs_reset(d, e->len) || gs_addr(d, e->len) >= 0) gs = 1;
    }
    fx_init(f, gs);
    for (i = 0; i < s->nev; i++) {
        const XgEv *e = &s->ev[i];
        const uint8_t *d = s->buf + e->off;
        int a;
        if (e->n || e->msg[0] != 0xF0) continue;
        if (gs) {
            if ((a = gs_addr(d, e->len)) == 0x30) f->sel[0] = d[7] & 7;
            else if (a == 0x38) f->sel[1] = d[7] & 7;
            else if (a == 0x33) f->ret[0] = d[7];
            else if (a == 0x3A) f->ret[1] = d[7];
        } else if ((a = xg_addr(d, e->len)) >= 0) {
            if (a == 0x00) { rm = d[6]; rl = e->len >= 9 && d[7] != 0xF7 ? d[7] : 0; }
            else if (a == 0x01) rl = d[6];
            else if (a == 0x20) { cm = d[6]; cl = e->len >= 9 && d[7] != 0xF7 ? d[7] : 0; }
            else if (a == 0x21) cl = d[6];
            else if (a == 0x0C) f->ret[0] = d[6];
            else if (a == 0x2C) f->ret[1] = d[6];
        }
    }
    if (rm >= 0) f->sel[0] = (uint8_t)find(XG_REV, NXR, rm, rl);
    if (cm >= 0) f->sel[1] = (uint8_t)find(XG_CHO, NXC, cm, cl);
}

int fx_msg(const Fx *f, int what, uint8_t o[16])
{
    if (f->gs) {
        static const uint8_t addr[FX_N] = {0x30, 0x38, 0x33, 0x3A};
        uint8_t v = what < 2 ? f->sel[what] : f->ret[what - 2];
        int sum = 0x40 + 0x01 + addr[what] + v;
        o[0] = 0xF0; o[1] = 0x41; o[2] = 0x10; o[3] = 0x42; o[4] = 0x12;
        o[5] = 0x40; o[6] = 0x01; o[7] = addr[what]; o[8] = v;
        o[9] = (uint8_t)((128 - sum % 128) % 128); o[10] = 0xF7;
        return 11;
    }
    o[0] = 0xF0; o[1] = 0x43; o[2] = 0x10; o[3] = 0x4C; o[4] = 0x02; o[5] = 0x01;
    if (what < 2) {
        const Type *t = what ? &XG_CHO[f->sel[1]] : &XG_REV[f->sel[0]];
        o[6] = what ? 0x20 : 0x00; o[7] = t->msb; o[8] = t->lsb; o[9] = 0xF7;
        return 10;
    }
    o[6] = what == FX_REVRET ? 0x0C : 0x2C; o[7] = f->ret[what - 2]; o[8] = 0xF7;
    return 9;
}

/* Which setting a song message sets (-1 = none of them). */
static int what_of(const Fx *f, const uint8_t *d, uint32_t len)
{
    int a;
    if (f->gs) {
        a = gs_addr(d, len);
        return a == 0x30 ? FX_REV : a == 0x38 ? FX_CHO : a == 0x33 ? FX_REVRET : a == 0x3A ? FX_CHORET : -1;
    }
    a = xg_addr(d, len);
    return a == 0x00 || a == 0x01 ? FX_REV : a == 0x20 || a == 0x21 ? FX_CHO : a == 0x0C ? FX_REVRET : a == 0x2C ? FX_CHORET : -1;
}

int fx_filter_sysex(const Fx *f, const uint8_t *data, uint32_t len)
{
    int w = what_of(f, data, len);
    return w < 0 || !f->touched[w];
}

void fx_save(const Fx *f, const XgSong *s, uint8_t *copy, XgInsert *ins, uint32_t *nins, uint32_t tick, uint16_t trk)
{
    uint8_t done[FX_N] = {0, 0, 0, 0};
    uint32_t i;
    int w;
    for (i = 0; i < s->nev; i++) {
        const XgEv *e = &s->ev[i];
        const uint8_t *d = s->buf + e->off;
        uint8_t m[16];
        if (e->n || e->msg[0] != 0xF0 || (w = what_of(f, d, e->len)) < 0 || !f->touched[w]) continue;
        fx_msg(f, w, m);
        if (f->gs) {
            copy[e->off + 7] = m[8];
            copy[e->off + 8] = m[9];
        } else if (w >= 2) {
            copy[e->off + 6] = m[7];
        } else if (d[5] == 0x00 || d[5] == 0x20) {
            copy[e->off + 6] = m[7];
            if (e->len >= 9 && d[7] != 0xF7) copy[e->off + 7] = m[8];
        } else {
            copy[e->off + 6] = m[8];   /* the LSB on its own */
        }
        done[w] = 1;
    }
    for (w = 0; w < FX_N; w++) {
        XgInsert *x;
        uint8_t m[16];
        int n;
        if (!f->touched[w] || done[w]) continue;
        n = fx_msg(f, w, m);
        x = &ins[(*nins)++];
        x->trk = trk;
        x->tick = tick;
        /* in the file: F0, the length, the rest */
        x->b[0] = 0xF0;
        x->b[1] = (uint8_t)(n - 1);
        memcpy(x->b + 2, m + 1, (size_t)(n - 1));
        x->n = (uint8_t)(n + 1);
    }
}
