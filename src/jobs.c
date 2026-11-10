/* The Job menu (see jobs.h). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jobs.h"
#include "drumops.h"

const char *const jb_qntz_names[9] = {"1/32", "1/16 triplet", "1/16", "1/8 triplet", "1/8", "1/4 triplet", "1/4", "1/8 + 1/8 triplet", "1/16 + 1/16 triplet"};

static int is_note(const EdEv *e) { return (e->st & 0xF0) == 0x90 && e->d2; }

static int in_trk(const JbScope *sc, uint32_t t) { return sc->trk < 0 || (uint32_t)sc->trk == t; }
static int in_time(const JbScope *sc, uint32_t tick) { return tick >= sc->from && tick < sc->to; }
static int note_in(const JbScope *sc, const EdEv *e) { return is_note(e) && in_time(sc, e->tick) && e->d1 >= sc->lo && e->d1 <= sc->hi; }

static int floordiv(long a, long b) { long q = a / b; if ((a % b) && ((a < 0) != (b < 0))) q--; return (int)q; }

static int apply(JbMod m, int v)
{
    if (m.absolute) return m.value;
    return floordiv((long)v * m.rate + 50, 100) + m.offset;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

int jb_transpose(EdSong *s, const JbScope *sc, int semis)
{
    uint32_t t, i;
    int n = 0;
    for (t = 0; t < s->ntrk; t++) {
        if (!in_trk(sc, t)) continue;
        for (i = 0; i < s->trk[t].n; i++) {
            EdEv *e = &s->trk[t].ev[i];
            int p;
            if (!note_in(sc, e)) continue;
            /* leaving 0..127: the same note name in the highest / lowest octave */
            p = e->d1 + semis;
            while (p > 127) p -= 12;
            while (p < 0) p += 12;
            e->d1 = (uint8_t)p;
            n++;
        }
    }
    return n;
}

int jb_velocity(EdSong *s, const JbScope *sc, JbMod m)
{
    uint32_t t, i;
    int n = 0;
    for (t = 0; t < s->ntrk; t++) {
        if (!in_trk(sc, t)) continue;
        for (i = 0; i < s->trk[t].n; i++) {
            EdEv *e = &s->trk[t].ev[i];
            if (!note_in(sc, e)) continue;
            e->d2 = (uint8_t)clampi(apply(m, e->d2), 1, 127);
            n++;
        }
    }
    return n;
}

int jb_gate(EdSong *s, const JbScope *sc, JbMod m)
{
    uint32_t t, i;
    int n = 0;
    for (t = 0; t < s->ntrk; t++) {
        if (!in_trk(sc, t)) continue;
        for (i = 0; i < s->trk[t].n; i++) {
            EdEv *e = &s->trk[t].ev[i];
            int d;
            if (!note_in(sc, e)) continue;
            d = apply(m, (int)e->dur);
            e->dur = (uint32_t)(d < 1 ? 1 : d);
            n++;
        }
    }
    return n;
}

int jb_shift(EdSong *s, const JbScope *sc, int clocks)
{
    uint32_t t, i;
    int n = 0;
    for (t = 0; t < s->ntrk; t++) {
        if (!in_trk(sc, t)) continue;
        for (i = 0; i < s->trk[t].n; i++) {
            EdEv *e = &s->trk[t].ev[i];
            long v;
            if (!in_time(sc, e->tick)) continue;
            v = (long)e->tick + clocks;
            e->tick = (uint32_t)(v < 0 ? 0 : v);
            n++;
        }
        ed_sort(s, t);
    }
    return n;
}

/* the notes at one tick: their indices in track order */
static int chord_at(const EdTrack *t, uint32_t i, uint32_t *idx, int max, uint32_t *next)
{
    uint32_t j = i;
    int n = 0;
    while (j < t->n && t->ev[j].tick == t->ev[i].tick) {
        if (is_note(&t->ev[j]) && n < max) idx[n++] = j;
        j++;
    }
    *next = j;
    return n;
}

void jb_sort_chord(EdSong *s, const JbScope *sc, int reverse)
{
    uint32_t t, i, next, idx[128];
    for (t = 0; t < s->ntrk; t++) {
        EdTrack *tr = &s->trk[t];
        if (!in_trk(sc, t)) continue;
        for (i = 0; i < tr->n; i = next) {
            int n = chord_at(tr, i, idx, 128, &next), a, b;
            if (n < 2 || !in_time(sc, tr->ev[i].tick)) continue;
            /* the notes' contents ordered by key in their places (stable) */
            for (a = 1; a < n; a++)
                for (b = a; b > 0; b--) {
                    EdEv *x = &tr->ev[idx[b - 1]], *y = &tr->ev[idx[b]], tmp;
                    if (reverse ? x->d1 >= y->d1 : x->d1 <= y->d1) break;
                    tmp = *x; *x = *y; *y = tmp;
                }
        }
    }
}

void jb_separate_chord(EdSong *s, const JbScope *sc, int clocks)
{
    uint32_t t, i, next, idx[128];
    for (t = 0; t < s->ntrk; t++) {
        EdTrack *tr = &s->trk[t];
        if (!in_trk(sc, t)) continue;
        for (i = 0; i < tr->n; i = next) {
            int n = chord_at(tr, i, idx, 128, &next), k;
            if (!n || !in_time(sc, tr->ev[i].tick)) continue;
            for (k = 0; k < n; k++) {
                long v = (long)tr->ev[idx[k]].tick + (long)k * clocks;
                tr->ev[idx[k]].tick = (uint32_t)(v < 0 ? 0 : v);
            }
        }
        ed_sort(s, t);
    }
}

static int ctrl_match(const EdEv *e, int kind, int cc)
{
    int k = e->st & 0xF0;
    switch (kind) {
    case JB_POLYAT: return k == 0xA0;
    case JB_CC: return k == 0xB0 && e->d1 == cc;
    case JB_CHAT: return k == 0xD0;
    default: return k == 0xE0;
    }
}

int jb_modify_control(EdSong *s, const JbScope *sc, int kind, int cc_from, int cc_to, JbMod m)
{
    uint32_t t, i;
    int n = 0;
    for (t = 0; t < s->ntrk; t++) {
        if (!in_trk(sc, t)) continue;
        for (i = 0; i < s->trk[t].n; i++) {
            EdEv *e = &s->trk[t].ev[i];
            if (e->st >= 0xF0 || !in_time(sc, e->tick) || !ctrl_match(e, kind, cc_from)) continue;
            switch (kind) {
            case JB_POLYAT: e->d2 = (uint8_t)clampi(apply(m, e->d2), 0, 127); break;
            case JB_CC: e->d1 = (uint8_t)cc_to; e->d2 = (uint8_t)clampi(apply(m, e->d2), 0, 127); break;
            case JB_CHAT: e->d1 = (uint8_t)clampi(apply(m, e->d1), 0, 127); break;
            default: {
                int v = ((e->d2 << 7) | e->d1) - 8192;
                v = clampi(apply(m, v), -8192, 8191) + 8192;
                e->d1 = (uint8_t)(v & 0x7F);
                e->d2 = (uint8_t)(v >> 7);
            }
            }
            n++;
        }
    }
    return n;
}

int jb_thin_out(EdSong *s, const JbScope *sc, int kind, int cc)
{
    uint32_t t, i;
    int removed = 0;
    for (t = 0; t < s->ntrk; t++) {
        EdTrack *tr = &s->trk[t];
        uint32_t *idx, n = 0, k, o;
        if (!in_trk(sc, t) || !(idx = (uint32_t *)malloc((tr->n + 1) * sizeof(uint32_t)))) continue;
        for (i = 0; i < tr->n; i++)
            if (tr->ev[i].st < 0xF0 && in_time(sc, tr->ev[i].tick) && ctrl_match(&tr->ev[i], kind, cc)) idx[n++] = i;
        /* the run from the second one: keep its first, drop every other one, keep its last */
        if (n >= 4) {
            for (k = 2; k + 1 < n; k += 2) tr->ev[idx[k]].st = 0;   /* marked */
            for (i = o = 0; i < tr->n; i++) {
                if (tr->ev[i].st == 0) { removed++; continue; }
                tr->ev[o++] = tr->ev[i];
            }
            tr->n = o;
        }
        free(idx);
    }
    return removed;
}

/* ---- quantize ---- */

static const long QGRID[9] = {60, 80, 120, 160, 240, 320, 480, 160, 120};
static const long QMIX_L[2][4] = {{80, 80, 40, 40}, {40, 40, 20, 20}};
static const long QMIX_R[2][4] = {{80, 40, 40, 80}, {40, 20, 20, 40}};

static void qwidth(int q, long i, long scale, long *l, long *r)
{
    if (q == 7 || q == 8) { *l = QMIX_L[q - 7][i & 3]; *r = QMIX_R[q - 7][i & 3]; }
    else { *l = QGRID[q] / 2; *r = QGRID[q] / 2; }
    *l = *l * scale / 480;
    *r = *r * scale / 480;
}

static int qswing(int q, long i)
{
    if (q == 1 || q == 3 || q == 5) return i % 3 == 2;
    if (q == 7 || q == 8) return (i & 3) == 2;
    return (i & 1) == 1;
}

static int qtime(long t, long g, long wl, long wr, long strength, long sens, long *out)
{
    if (!(g - wl <= t && t < g + wr)) return 0;
    if (t < g) {
        if (sens < 0) { if ((sens * wl) / -100 - wl + g < t) return 0; }
        else if (t < (sens * wl) / -100 + g) return 0;
        *out = t + strength * (g - t) / 100;
    } else {
        if (sens < 0) { if (t < (sens * wr) / 100 + wr + g) return 0; }
        else if ((sens * wr) / 100 + g < t) return 0;
        *out = t - strength * (t - g) / 100;
    }
    return 1;
}

/* C division truncates like Rust's for these signs; the bar of a tick from the time signatures */
static void bar_at(const EdSong *s, uint32_t tick, uint32_t *start, uint32_t *len)
{
    uint32_t tk[64], num[64], bl[64], b;
    int n = ed_sigs(s, tk, num, bl, 64), k = n - 1;
    while (k > 0 && tk[k] > tick) k--;
    b = num[k] * bl[k];
    *start = tk[k] + (tick - tk[k]) / b * b;
    *len = b;
}

int jb_quantize(EdSong *s, const JbScope *sc, JbQntz o)
{
    uint32_t t, i;
    int n = 0, q = o.grid < 0 ? 0 : o.grid > 8 ? 8 : o.grid;
    long scale = s->ppqn;
    for (t = 0; t < s->ntrk; t++) {
        if (!in_trk(sc, t)) continue;
        for (i = 0; i < s->trk[t].n; i++) {
            EdEv *e = &s->trk[t].ev[i];
            uint32_t bs, bl;
            long g, end, tk = (long)e->tick, k = 0;
            if (!note_in(sc, e)) continue;
            bar_at(s, e->tick, &bs, &bl);
            g = (long)bs;
            end = (long)bs + (long)bl;
            while (g <= end) {
                long wl, wr, r;
                qwidth(q, k, scale, &wl, &wr);
                if (qtime(tk, g, wl, wr, o.strength, o.sens, &r)) {
                    if (qswing(q, k)) {
                        r += QGRID[q] * scale / 480 * (o.swing - 50) / 50;
                        e->dur = (uint32_t)o.gate * e->dur / 100;
                        if (!e->dur) e->dur = 1;
                        e->d2 = (uint8_t)clampi((int)((uint32_t)o.vel * e->d2 / 100), 1, 127);
                    }
                    e->tick = (uint32_t)(r < 0 ? 0 : r);
                    n++;
                    break;
                }
                if (q == 7 || q == 8) g += 2 * wr;
                else g += QGRID[q] * scale / 480;
                k++;
            }
        }
        ed_sort(s, t);
    }
    return n;
}

int jb_stretch(EdSong *s, const JbScope *sc, uint32_t stretch, int offset)
{
    uint32_t t, i;
    int n = 0;
    for (t = 0; t < s->ntrk; t++) {
        if (!in_trk(sc, t)) continue;
        for (i = 0; i < s->trk[t].n; i++) {
            EdEv *e = &s->trk[t].ev[i];
            long v;
            if (!in_time(sc, e->tick)) continue;
            v = (long)sc->from + offset + ((long)e->tick - (long)sc->from) * (long)stretch / 100;
            e->tick = (uint32_t)(v < 0 ? 0 : v);
            if (is_note(e)) { e->dur = e->dur * stretch / 100; if (!e->dur) e->dur = 1; }
            n++;
        }
        ed_sort(s, t);
    }
    return n;
}

/* ---- measures ---- */

static void shift_from(EdSong *s, uint32_t at, long by)
{
    uint32_t t, i;
    for (t = 0; t < s->ntrk; t++) {
        for (i = 0; i < s->trk[t].n; i++) {
            EdEv *e = &s->trk[t].ev[i];
            if (e->tick >= at) { long v = (long)e->tick + by; e->tick = (uint32_t)(v < 0 ? 0 : v); }
        }
        if (s->trk[t].end >= at) { long v = (long)s->trk[t].end + by; s->trk[t].end = (uint32_t)(v < 0 ? 0 : v); }
        ed_sort(s, t);
    }
}

static int is_sig(const EdEv *e) { return e->st == 0xFF && e->d1 == 0x58; }

static void put_sig(EdSong *s, uint32_t trk, uint32_t tick, int num, int den)
{
    EdEv e;
    uint8_t d[4];
    int l = 0;
    while ((1 << l) < den && l < 6) l++;
    d[0] = (uint8_t)num; d[1] = (uint8_t)l; d[2] = 24; d[3] = 8;
    memset(&e, 0, sizeof e);
    e.tick = tick; e.st = 0xFF; e.d1 = 0x58; e.len = 4;
    if ((e.data = ed_pool_add(s, d, 4)) != 0xFFFFFFFFUL) ed_insert(s, trk, &e);
}

void jb_create_measure(EdSong *s, uint32_t at, int num, int den, uint32_t count, int old_num, int old_den)
{
    uint32_t bar = (uint32_t)s->ppqn * 4 * (uint32_t)num / (uint32_t)(den ? den : 4), len = bar * count, i, o;
    EdTrack *c;
    int after = 0;
    if (!s->ntrk) return;
    shift_from(s, at, (long)len);
    c = &s->trk[0];
    for (i = o = 0; i < c->n; i++) {
        if (is_sig(&c->ev[i]) && c->ev[i].tick == at) continue;
        if (is_sig(&c->ev[i]) && c->ev[i].tick == at + len) after = 1;
        c->ev[o++] = c->ev[i];
    }
    c->n = o;
    put_sig(s, 0, at, num, den);
    if ((num != old_num || den != old_den) && !after) put_sig(s, 0, at + len, old_num, old_den);
}

void jb_delete_measure(EdSong *s, uint32_t from, uint32_t to)
{
    uint32_t t, i, o, sig_trk = 0;
    EdEv sig_after;
    int have = 0, sig_found = 0;
    if (to <= from) return;
    /* a time signature inside the cut that still rules afterwards moves to the cut */
    for (t = 0; t < s->ntrk; t++)
        for (i = 0; i < s->trk[t].n; i++) {
            const EdEv *e = &s->trk[t].ev[i];
            if (is_sig(e) && !sig_found) { sig_trk = t; sig_found = 1; }
            if (is_sig(e) && e->tick >= from && e->tick < to) { sig_after = *e; sig_after.tick = from; have = 1; }
        }
    for (t = 0; t < s->ntrk; t++) {
        EdTrack *tr = &s->trk[t];
        for (i = o = 0; i < tr->n; i++) {
            if (tr->ev[i].tick >= from && tr->ev[i].tick < to) continue;
            tr->ev[o++] = tr->ev[i];
        }
        tr->n = o;
    }
    shift_from(s, to, -(long)(to - from));
    if (have) {
        /* into the first track that still has a time signature (none left: none) */
        sig_found = 0;
        for (t = 0; t < s->ntrk && !sig_found; t++)
            for (i = 0; i < s->trk[t].n; i++)
                if (is_sig(&s->trk[t].ev[i])) { sig_trk = t; sig_found = 1; break; }
        if (!sig_found) return;
        for (i = 0; i < s->trk[sig_trk].n; i++)
            if (is_sig(&s->trk[sig_trk].ev[i]) && s->trk[sig_trk].ev[i].tick == from) return;
        ed_insert(s, sig_trk, &sig_after);
    }
}

static int xf_pc(uint8_t b)
{
    static const int NAT[8] = {0, 0, 2, 4, 5, 7, 9, 11};
    int n = b & 0x0F;
    if (n < 1 || n > 7) return -1;
    return ((NAT[n] + (b >> 4) - 3) % 12 + 12) % 12;
}

static uint8_t xf_root(int pc, int flats)
{
    static const uint8_t SH[12][2] = {{1, 3}, {1, 4}, {2, 3}, {2, 4}, {3, 3}, {4, 3}, {4, 4}, {5, 3}, {5, 4}, {6, 3}, {6, 4}, {7, 3}};
    static const uint8_t FL[12][2] = {{1, 3}, {2, 2}, {2, 3}, {3, 2}, {3, 3}, {4, 3}, {5, 2}, {5, 3}, {6, 2}, {6, 3}, {7, 2}, {7, 3}};
    const uint8_t *x = flats ? FL[pc] : SH[pc];
    return (uint8_t)((x[1] << 4) | x[0]);
}

int jb_transpose_chord(EdSong *s, uint32_t from, uint32_t to, int semis)
{
    uint32_t t, i;
    int n = 0, k;
    for (t = 0; t < s->ntrk; t++)
        for (i = 0; i < s->trk[t].n; i++) {
            EdEv *e = &s->trk[t].ev[i];
            uint8_t d[7];
            uint32_t at;
            if (e->st != 0xFF || e->d1 != 0x7F || e->len < 7 || e->tick < from || e->tick >= to) continue;
            memcpy(d, s->pool + e->data, 7);
            if (d[0] != 0x43 || d[1] != 0x7B || d[2] != 0x01) continue;
            for (k = 3; k <= 5; k += 2) {
                int pc = xf_pc(d[k]);
                if (pc >= 0) d[k] = xf_root(((pc + semis) % 12 + 12) % 12, semis < 0);
            }
            if ((at = ed_pool_add(s, d, 7)) == 0xFFFFFFFFUL) return n;
            e->data = at;
            n++;
        }
    return n;
}

/* ---- demix ---- */

static int ev_channel(const EdEv *e) { return e->st >= 0x80 && e->st < 0xF0 ? e->st & 0x0F : -1; }

static int set_name(EdSong *s, uint32_t trk, const char *name)
{
    return ed_set_name(s, trk, (const uint8_t *)name, (uint32_t)strlen(name));
}

static void name_of(const EdSong *s, uint32_t trk, char *out, int max)
{
    uint32_t len;
    const uint8_t *n = ed_name(s, trk, &len);
    if (n && len) { if ((int)len >= max) len = (uint32_t)max - 1; memcpy(out, n, len); out[len] = 0; }
    else snprintf(out, (size_t)max, "Track%lu", (unsigned long)trk);
}

int jb_demix_channels(EdSong *s, uint32_t trk)
{
    uint16_t used = 0;
    uint32_t i;
    int c, chans = 0, made = 0, at;
    char base[60], nm[80];
    EdTrack src;
    if (trk >= s->ntrk) return 0;
    for (i = 0; i < s->trk[trk].n; i++)
        if (ev_channel(&s->trk[trk].ev[i]) >= 0) used |= (uint16_t)(1u << ev_channel(&s->trk[trk].ev[i]));
    for (c = 0; c < 16; c++) chans += (used >> c) & 1;
    if (chans <= 1) return 0;
    name_of(s, trk, base, sizeof base);
    src = s->trk[trk];
    /* the track keeps its meta / SysEx (and goes when it has none) */
    {
        EdTrack *t = &s->trk[trk];
        uint32_t o = 0;
        EdEv *copy = (EdEv *)malloc((src.n + 1) * sizeof(EdEv));
        if (!copy) return 0;
        memcpy(copy, src.ev, src.n * sizeof(EdEv));
        for (i = 0; i < src.n; i++)
            if (ev_channel(&copy[i]) < 0) t->ev[o++] = copy[i];
        t->n = o;
        at = (int)trk + 1;
        if (!o) { ed_del_track(s, trk); at = (int)trk; }
        for (c = 0; c < 16; c++) {
            EdTrack *nt;
            if (!((used >> c) & 1)) continue;
            if (!ed_add_track(s, (uint32_t)at)) break;
            nt = &s->trk[at];
            for (i = 0; i < src.n; i++) {
                if (ev_channel(&copy[i]) != c) continue;
                if (nt->n == nt->cap) {
                    uint32_t cap = nt->cap ? nt->cap * 2 : 256;
                    EdEv *nv = (EdEv *)realloc(nt->ev, cap * sizeof(EdEv));
                    if (!nv) break;
                    nt->ev = nv; nt->cap = cap;
                }
                nt->ev[nt->n++] = copy[i];
            }
            nt->end = src.end;
            snprintf(nm, sizeof nm, "%s [ch %d]", base, c + 1);
            set_name(s, (uint32_t)at, nm);
            at++;
            made++;
        }
        free(copy);
    }
    return made;
}

static const char *key_name(int ch, int key, const char *(*label)(int, int), char *buf)
{
    static const char *const N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const char *l = label ? label(ch, key) : NULL;
    if (!l && ch == 9) l = dr_gm_names[key];
    if (l) sprintf(buf, "CH %d - %s (%d)", ch + 1, l, key);
    else sprintf(buf, "CH %d - Note %s%d (%d)", ch + 1, N[key % 12], key / 12 - 2, key);
    return buf;
}

int jb_demix_notes(EdSong *s, uint32_t trk, uint16_t channels, const char *(*label)(int ch, int key))
{
    EdTrack src;
    EdEv *copy;
    uint8_t keys[16][128];
    uint32_t i, o;
    int c, k, made = 0, at;
    char nm[100];
    if (trk >= s->ntrk) return 0;
    memset(keys, 0, sizeof keys);
    src = s->trk[trk];
    for (i = 0; i < src.n; i++) {
        const EdEv *e = &src.ev[i];
        c = ev_channel(e);
        if (c >= 0 && ((channels >> c) & 1) && is_note(e)) keys[c][e->d1] = 1;
    }
    for (c = 0; c < 16; c++)
        for (k = 0; k < 128; k++) made += keys[c][k];
    if (!made) return 0;
    made = 0;
    if (!(copy = (EdEv *)malloc((src.n + 1) * sizeof(EdEv)))) return 0;
    memcpy(copy, src.ev, src.n * sizeof(EdEv));
    /* the track keeps the rest (notes and poly pressure of those channels move) */
    {
        EdTrack *t = &s->trk[trk];
        for (i = o = 0; i < src.n; i++) {
            const EdEv *e = &copy[i];
            c = ev_channel(e);
            if (c >= 0 && ((channels >> c) & 1) && ((is_note(e) && keys[c][e->d1]) || (e->st & 0xF0) == 0xA0 || (e->st & 0xF0) == 0x80 || ((e->st & 0xF0) == 0x90 && !e->d2))) continue;
            t->ev[o++] = copy[i];
        }
        t->n = o;
        at = (int)trk + 1;
        if (!o) { ed_del_track(s, trk); at = (int)trk; }
    }
    for (c = 0; c < 16; c++)
        for (k = 0; k < 128; k++) {
            EdTrack *nt;
            if (!keys[c][k]) continue;
            if (!ed_add_track(s, (uint32_t)at)) { free(copy); return made; }
            nt = &s->trk[at];
            for (i = 0; i < src.n; i++) {
                const EdEv *e = &copy[i];
                if (ev_channel(e) != c || e->d1 != k || !(is_note(e) || (e->st & 0xF0) == 0xA0)) continue;
                if (nt->n == nt->cap) {
                    uint32_t cap = nt->cap ? nt->cap * 2 : 64;
                    EdEv *nv = (EdEv *)realloc(nt->ev, cap * sizeof(EdEv));
                    if (!nv) break;
                    nt->ev = nv; nt->cap = cap;
                }
                nt->ev[nt->n++] = *e;
            }
            nt->end = src.end;
            set_name(s, (uint32_t)at, key_name(c, k, label, nm));
            at++;
            made++;
        }
    free(copy);
    return made;
}
