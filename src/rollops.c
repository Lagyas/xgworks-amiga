/* What the Piano Roll does to the notes (see rollops.h). */
#include <stdlib.h>
#include <string.h>
#include "rollops.h"

int ro_is_note(const EdEv *e, int ch)
{
    return (e->st & 0xF0) == 0x90 && e->d2 && (ch < 0 || (e->st & 0x0F) == ch);
}

long ro_hit(const EdSong *s, uint32_t trk, int ch, uint32_t tick, int key)
{
    long i, hit = -1;
    if (trk >= s->ntrk) return -1;
    for (i = 0; i < (long)s->trk[trk].n; i++) {
        const EdEv *e = &s->trk[trk].ev[i];
        if (e->tick > tick) break;
        if (ro_is_note(e, ch) && e->d1 == key && tick < e->tick + (e->dur ? e->dur : 1)) hit = i;
    }
    return hit;
}

void ro_select_none(EdSong *s, uint32_t trk)
{
    uint32_t i;
    if (trk >= s->ntrk) return;
    for (i = 0; i < s->trk[trk].n; i++) s->trk[trk].ev[i].sel = 0;
}

int ro_select_all(EdSong *s, uint32_t trk, int ch)
{
    uint32_t i;
    int n = 0;
    if (trk >= s->ntrk) return 0;
    for (i = 0; i < s->trk[trk].n; i++) {
        EdEv *e = &s->trk[trk].ev[i];
        e->sel = (uint8_t)ro_is_note(e, ch);
        n += e->sel;
    }
    return n;
}

int ro_select_rect(EdSong *s, uint32_t trk, int ch, uint32_t t0, uint32_t t1, int k0, int k1, int add)
{
    uint32_t i;
    int n = 0;
    if (trk >= s->ntrk) return 0;
    if (k0 > k1) { int k = k0; k0 = k1; k1 = k; }
    if (t0 > t1) { uint32_t t = t0; t0 = t1; t1 = t; }
    for (i = 0; i < s->trk[trk].n; i++) {
        EdEv *e = &s->trk[trk].ev[i];
        int in = ro_is_note(e, ch) && e->tick >= t0 && e->tick < t1 && e->d1 >= k0 && e->d1 <= k1;
        if (in) e->sel = 1;
        else if (!add) e->sel = 0;
        n += e->sel;
    }
    return n;
}

int ro_count_sel(const EdSong *s, uint32_t trk)
{
    uint32_t i;
    int n = 0;
    if (trk >= s->ntrk) return 0;
    for (i = 0; i < s->trk[trk].n; i++) n += s->trk[trk].ev[i].sel != 0;
    return n;
}

int ro_move_sel(EdSong *s, uint32_t trk, long dt, int dk)
{
    uint32_t i;
    int n = 0;
    long mint = 0x7FFFFFFFL;
    int mink = 127, maxk = 0;
    if (trk >= s->ntrk) return 0;
    /* the whole selection moves together: limits by its first note and key range */
    for (i = 0; i < s->trk[trk].n; i++) {
        const EdEv *e = &s->trk[trk].ev[i];
        if (!e->sel) continue;
        if ((long)e->tick < mint) mint = (long)e->tick;
        if (e->d1 < mink) mink = e->d1;
        if (e->d1 > maxk) maxk = e->d1;
    }
    if (mint == 0x7FFFFFFFL) return 0;
    if (mint + dt < 0) dt = -mint;
    if (mink + dk < 0) dk = -mink;
    if (maxk + dk > 127) dk = 127 - maxk;
    for (i = 0; i < s->trk[trk].n; i++) {
        EdEv *e = &s->trk[trk].ev[i];
        if (!e->sel) continue;
        e->tick = (uint32_t)((long)e->tick + dt);
        if ((e->st & 0xF0) == 0x90 || (e->st & 0xF0) == 0x80 || (e->st & 0xF0) == 0xA0) e->d1 = (uint8_t)(e->d1 + dk);
        n++;
    }
    ed_sort(s, trk);
    return n;
}

int ro_size_sel(EdSong *s, uint32_t trk, long dd, uint32_t min)
{
    uint32_t i;
    int n = 0;
    if (trk >= s->ntrk) return 0;
    if (!min) min = 1;
    for (i = 0; i < s->trk[trk].n; i++) {
        EdEv *e = &s->trk[trk].ev[i];
        long d;
        if (!e->sel || (e->st & 0xF0) != 0x90 || !e->d2) continue;
        d = (long)e->dur + dd;
        e->dur = d < (long)min ? min : (uint32_t)d;
        n++;
    }
    return n;
}

int ro_vel_sel(EdSong *s, uint32_t trk, int vel)
{
    uint32_t i;
    int n = 0;
    if (trk >= s->ntrk) return 0;
    if (vel < 1) vel = 1;
    if (vel > 127) vel = 127;
    for (i = 0; i < s->trk[trk].n; i++) {
        EdEv *e = &s->trk[trk].ev[i];
        if (e->sel && (e->st & 0xF0) == 0x90 && e->d2) { e->d2 = (uint8_t)vel; n++; }
    }
    return n;
}

int ro_delete_sel(EdSong *s, uint32_t trk)
{
    uint32_t i, o = 0;
    int n = 0;
    EdTrack *t;
    if (trk >= s->ntrk) return 0;
    t = &s->trk[trk];
    for (i = 0; i < t->n; i++) {
        if (t->ev[i].sel) { n++; continue; }
        t->ev[o++] = t->ev[i];
    }
    t->n = o;
    return n;
}

long ro_add_note(EdSong *s, uint32_t trk, int ch, uint32_t tick, int key, int vel, uint32_t dur)
{
    EdEv e;
    int at;
    if (trk >= s->ntrk) return -1;
    ro_select_none(s, trk);
    memset(&e, 0, sizeof e);
    e.tick = tick;
    e.st = (uint8_t)(0x90 | (ch & 15));
    e.d1 = (uint8_t)(key < 0 ? 0 : key > 127 ? 127 : key);
    e.d2 = (uint8_t)(vel < 1 ? 1 : vel > 127 ? 127 : vel);
    e.dur = dur ? dur : 1;
    e.sel = 1;
    at = ed_insert(s, trk, &e);
    return at > 0 ? at - 1L : -1L;
}

int ro_copy_sel(EdSong *s, uint32_t trk, long dt)
{
    uint32_t i, n0;
    int n = 0;
    EdEv *copies;
    if (trk >= s->ntrk) return 0;
    n0 = s->trk[trk].n;
    if (!(copies = (EdEv *)malloc((n0 + 1) * sizeof(EdEv)))) return 0;
    for (i = 0; i < n0; i++) {
        EdEv *e = &s->trk[trk].ev[i];
        if (!e->sel) continue;
        e->sel = 0;
        copies[n] = *e;
        if ((long)copies[n].tick + dt < 0) { e->sel = 1; continue; }
        copies[n].tick = (uint32_t)((long)copies[n].tick + dt);
        copies[n].sel = 1;
        n++;
    }
    for (i = 0; i < (uint32_t)n; i++) ed_insert(s, trk, &copies[i]);
    free(copies);
    return n;
}

uint32_t ro_snap(uint32_t tick, uint32_t g)
{
    return g > 1 ? tick / g * g : tick;
}
