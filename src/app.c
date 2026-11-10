/* XGWorks Lite: the shared part of the windows (see app.h). */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/exall.h>
#include <dos/dostags.h>
#include <libraries/asl.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/asl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "midiout.h"
#include "xws.h"

struct Library *AslBase;

const char *const app_reset_names[RESETS + 1] = {"Song's own", "XG", "GM", "GS", NULL};
const char *const app_output_names[OUTPUTS + 1] = {"serial.device", "CAMD", NULL};

static const uint8_t XG_ON[] = {0xF0, 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0xF7};
static const uint8_t GM_ON[] = {0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7};
static const uint8_t GS_ON[] = {0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7};

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static int ci_cmp(const char *x, const char *y)
{
    while (*x && lower(*x) == lower(*y)) { x++; y++; }
    return lower((unsigned char)*x) - lower((unsigned char)*y);
}

static int name_cmp(const void *x, const void *y)
{
    return ci_cmp(*(char *const *)x, *(char *const *)y);
}

static int is_song(const char *n)
{
    const char *d = strrchr(n, '.');
    return d && (!ci_cmp(d, ".mid") || !ci_cmp(d, ".midi") || !ci_cmp(d, ".kar") || !ci_cmp(d, ".smf") || !ci_cmp(d, ".xws"));
}

static void free_names(App *a)
{
    int i;
    for (i = 0; i < a->count; i++) free(a->names[i]);
    free(a->names);
    a->names = NULL;
    a->count = 0;
}

int app_scan(App *a, const char *folder)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    char **names = NULL;
    int n = 0, cap = 0;
    if (!(lock = Lock((CONST_STRPTR)folder, ACCESS_READ))) {
        snprintf(a->info, sizeof a->info, "The folder %s cannot be read.", folder);
        return 0;
    }
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    if (fib && Examine(lock, fib) && fib->fib_DirEntryType > 0) {
        while (ExNext(lock, fib)) {
            if (fib->fib_DirEntryType >= 0 || !is_song((const char *)fib->fib_FileName)) continue;
            if (n == cap) {
                char **nn = (char **)realloc(names, (cap = cap ? cap * 2 : 64) * sizeof(char *));
                if (!nn) break;
                names = nn;
            }
            if (!(names[n] = strdup((const char *)fib->fib_FileName))) break;
            n++;
        }
    }
    if (fib) FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    if (n) qsort(names, n, sizeof(char *), name_cmp);
    app_stop(a);
    free_names(a);
    a->names = names;
    a->count = n;
    a->cur = -1;
    if (folder != a->folder) {
        strncpy(a->folder, folder, sizeof a->folder - 1);
        a->folder[sizeof a->folder - 1] = 0;
    }
    snprintf(a->info, sizeof a->info, "%d songs in %s", n, folder);
    return 1;
}

int app_init(App *a, const char *folder)
{
    memset(a, 0, sizeof *a);
    a->cur = -1;
    a->autonext = 1;
    a->kfont = 24;
    strcpy(a->serdev, "serial.device");
    strcpy(a->folder, "PROGDIR:Songs");
    if (!pl_init(&a->pl)) return 0;
    AslBase = OpenLibrary((CONST_STRPTR)"asl.library", 37);
    app_load_prefs(a);
    if (folder) {
        strncpy(a->folder, folder, sizeof a->folder - 1);
    }
    if (!app_scan(a, a->folder)) {
        /* no Songs folder: the current one */
        strcpy(a->folder, "");
        app_scan(a, a->folder);
    }
    return 1;
}

/* The window task's own MIDI output (only while nothing plays). */
static void out_close(App *a)
{
    if (a->out_open) mo_close();
    a->out_open = 0;
}

static void send_now(App *a, const uint8_t *m)
{
    int k = m[0] & 0xF0, n = 0;
    if (m[0] == 0xF0) while (n < 16 && m[n++] != 0xF7) {}
    else n = (k == 0xC0 || k == 0xD0) ? 2 : 3;
    if (app_playing(a)) {
        pl_queue(&a->pl, m, n);
        return;
    }
    if (!a->out_open) {
        char err[120];
        if (!mo_open(a->output == 1 ? "camd" : a->serdev, a->unit, err, sizeof err)) {
            mo_close();
            snprintf(a->info, sizeof a->info, "%s", err);
            return;
        }
        a->out_open = 1;
    }
    mo_msg(m[0], m + 1, (uint32_t)n - 1);
    mo_flush();
}

void app_note(App *a, int ch, int key, int vel)
{
    uint8_t m[3];
    m[0] = (uint8_t)(0x90 | (ch & 15)); m[1] = (uint8_t)(key & 127); m[2] = (uint8_t)(vel & 127);
    send_now(a, m);
}

void app_fx(App *a, int what, int value)
{
    uint8_t m[16];
    if (what < 2) a->mx.fx.sel[what] = (uint8_t)value;
    else a->mx.fx.ret[what - 2] = (uint8_t)(value < 0 ? 0 : value > 127 ? 127 : value);
    a->mx.fx.touched[what] = 1;
    fx_msg(&a->mx.fx, what, m);
    send_now(a, m);
}

void app_mix(App *a, int ch, int param, int value)
{
    uint8_t out[3][3];
    int k, n = mx_set(&a->mx, ch, param, value, out);
    for (k = 0; k < n; k++) send_now(a, out[k]);
}

void app_mute(App *a, int ch, int mute, int solo)
{
    int c;
    a->mx.mute[ch] = (uint8_t)mute;
    a->mx.solo[ch] = (uint8_t)solo;
    /* what is silent now stops sounding */
    for (c = 0; c < 16; c++)
        if (!mx_audible(&a->mx, c)) {
            uint8_t m[3];
            m[0] = (uint8_t)(0xB0 | c); m[1] = 0x7B; m[2] = 0;
            send_now(a, m);
        }
}

void app_mix_reset(App *a)
{
    uint8_t m[16 * 8][3];
    int k, n;
    Mixer old = a->mx;
    if (a->cur < 0) return;
    mx_from_song(&a->mx, &a->song);
    memcpy(a->mx.mute, old.mute, sizeof old.mute);
    memcpy(a->mx.solo, old.solo, sizeof old.solo);
    /* the song's values out again, for what was changed */
    for (k = 0; k < 16; k++) {
        int p;
        for (p = 0; p < MX_N; p++) a->mx.touched[k][p] = old.touched[k][p];
    }
    n = mx_setup(&a->mx, m, 16 * 8);
    memset(a->mx.touched, 0, sizeof a->mx.touched);
    for (k = 0; k < n; k++) send_now(a, m[k]);
    /* the song's effects, for what was changed */
    {
        int w;
        for (w = 0; w < FX_N; w++)
            if (old.fx.touched[w]) {
                uint8_t sx[16];
                fx_msg(&a->mx.fx, w, sx);
                send_now(a, sx);
            }
    }
}

static void drop_undo(App *a)
{
    int i;
    for (i = 0; i < UNDOS; i++) ed_free(&a->undo[i]);
    a->nundo = a->undo_top = 0;
}

static void unload(App *a)
{
    app_stop(a);
    if (a->data) {
        ly_free(&a->ly);
        xg_smf_free(&a->song);
        free(a->data);
        ed_free(&a->ed);
        drop_undo(a);
    }
    a->dirty = 0;
    a->path_as[0] = 0;
    a->data = NULL;
    a->cur = -1;
    a->len_us = 0;
}

void app_free(App *a)
{
    unload(a);
    pl_free(&a->pl);
    out_close(a);
    free_names(a);
    if (AslBase) CloseLibrary(AslBase);
    AslBase = NULL;
}

int app_load(App *a, int k)
{
    char path[512];
    BPTR fh;
    LONG size;
    uint8_t *data;
    int r;
    if (k < 0 || k >= a->count) return 0;
    unload(a);
    strncpy(path, a->folder, sizeof path - 1);
    path[sizeof path - 1] = 0;
    AddPart((STRPTR)path, (CONST_STRPTR)a->names[k], sizeof path);
    if (!(fh = Open((CONST_STRPTR)path, MODE_OLDFILE))) {
        snprintf(a->info, sizeof a->info, "%s cannot be opened.", a->names[k]);
        return 0;
    }
    Seek(fh, 0, OFFSET_END);
    size = Seek(fh, 0, OFFSET_BEGINNING);
    if (size <= 0 || !(data = (uint8_t *)malloc((size_t)size))) {
        Close(fh);
        snprintf(a->info, sizeof a->info, "%s: not enough memory.", a->names[k]);
        return 0;
    }
    r = Read(fh, data, size) == size;
    Close(fh);
    if (!r) {
        free(data);
        snprintf(a->info, sizeof a->info, "%s cannot be read.", a->names[k]);
        return 0;
    }
    a->is_xws = xws_is(data, (size_t)size);
    if (a->is_xws) {
        /* an XGworks song: read as the MIDI file XGWorks NextGen would export */
        size_t mn;
        uint8_t *mid = xws_to_smf(data, (size_t)size, &mn);
        free(data);
        if (!mid) {
            snprintf(a->info, sizeof a->info, "%s: not enough memory.", a->names[k]);
            return 0;
        }
        data = mid;
        size = (LONG)mn;
    }
    if ((r = xg_smf_load(&a->song, data, (size_t)size)) != XG_OK) {
        free(data);
        snprintf(a->info, sizeof a->info, "%s: %s.", a->names[k], xg_error(r));
        return 0;
    }
    a->data = data;
    a->cur = k;
    if (!ed_load(&a->ed, data, (size_t)size)) memset(&a->ed, 0, sizeof a->ed);
    mx_from_song(&a->mx, &a->song);
    ly_build(&a->ly, &a->song, a->letters);
    a->len_us = a->song.nev ? xg_tick_us(&a->song, a->song.ev[a->song.nev - 1].tick) : 0;
    {
        char t[16];
        app_time(t, a->len_us);
        snprintf(a->info, sizeof a->info, "%s - format %u, %u tracks, %s", a->names[k], a->song.format, a->song.ntracks, t);
    }
    return 1;
}

int app_play(App *a, uint32_t from_us)
{
    Player *p = &a->pl;
    if (a->cur < 0) return 0;
    app_stop(a);
    out_close(a);  /* the player opens the output itself */
    p->song = &a->song;
    p->mixer = &a->mx;
    p->from_us = from_us;
    switch (a->reset) {
    case 1: p->reset = XG_ON; p->rlen = sizeof XG_ON; break;
    case 2: p->reset = GM_ON; p->rlen = sizeof GM_ON; break;
    case 3: p->reset = GS_ON; p->rlen = sizeof GS_ON; break;
    default: p->reset = NULL; p->rlen = 0;
    }
    strcpy(p->device, a->output == 1 ? "camd" : a->serdev);
    p->unit = a->unit;
    if (!pl_start(p)) {
        snprintf(a->info, sizeof a->info, "%s", p->error);
        return 0;
    }
    return 1;
}

void app_stop(App *a)
{
    pl_stop(&a->pl);
}

int app_playing(const App *a)
{
    return a->pl.running;
}

int app_poll(App *a)
{
    if (a->pl.running) return 0;
    if (a->pl.error[0]) {
        snprintf(a->info, sizeof a->info, "%s", a->pl.error);
        a->pl.error[0] = 0;
        return 0;
    }
    if (a->pl.ended) {
        a->pl.ended = 0;
        return 1;
    }
    return 0;
}

static int write_file(const char *path, const uint8_t *b, size_t n)
{
    BPTR fh = Open((CONST_STRPTR)path, MODE_NEWFILE);
    LONG w;
    if (!fh) return 0;
    w = Write(fh, (APTR)b, (LONG)n);
    Close(fh);
    return w == (LONG)n;
}

/* ---- editing ---- */

void app_ed_begin(App *a)
{
    EdSong *u = &a->undo[a->undo_top];
    ed_free(u);
    if (!ed_copy(u, &a->ed)) return;
    a->undo_top = (a->undo_top + 1) % UNDOS;
    if (a->nundo < UNDOS) a->nundo++;
}

/* The song made again from a->ed; the user's mixer values stay. */
static int rebuild(App *a)
{
    size_t n;
    uint8_t *nb = ed_save(&a->ed, &n);
    XgSong ns;
    Mixer keep = a->mx;
    if (!nb) { snprintf(a->info, sizeof a->info, "Not enough memory."); return 0; }
    if (xg_smf_load(&ns, nb, n) != XG_OK) { free(nb); snprintf(a->info, sizeof a->info, "The edited song cannot be read."); return 0; }
    app_stop(a);
    ly_free(&a->ly);
    xg_smf_free(&a->song);
    free(a->data);
    a->data = nb;
    a->song = ns;
    a->len_us = ns.nev ? xg_tick_us(&ns, ns.ev[ns.nev - 1].tick) : 0;
    mx_from_song(&a->mx, &a->song);
    mx_keep_user(&a->mx, &keep);
    ly_build(&a->ly, &a->song, a->letters);
    app_track_mute(a);
    return 1;
}

int app_ed_commit(App *a)
{
    if (a->cur < 0) return 0;
    a->dirty = 1;
    return rebuild(a);
}

int app_undo(App *a)
{
    int i;
    if (!a->nundo) { snprintf(a->info, sizeof a->info, "Nothing to undo."); return 0; }
    i = (a->undo_top + UNDOS - 1) % UNDOS;
    ed_free(&a->ed);
    a->ed = a->undo[i];
    memset(&a->undo[i], 0, sizeof a->undo[i]);
    a->undo_top = i;
    a->nundo--;
    snprintf(a->info, sizeof a->info, "Undone (%d more).", a->nundo);
    return rebuild(a);
}

void app_track_mute(App *a)
{
    uint32_t t;
    int solo = 0;
    for (t = 0; t < a->ed.ntrk; t++) solo |= a->ed.trk[t].solo;
    for (t = 0; t < 64; t++)
        a->pl.tmute[t] = (uint8_t)(t < a->ed.ntrk && (a->ed.trk[t].mute || (solo && !a->ed.trk[t].solo)));
}

int app_save(App *a)
{
    char path[512], bak[520];
    uint8_t *nb;
    size_t n;
    int k = a->cur;
    BPTR l;
    if (k < 0) return 0;
    if (!mx_any_touched(&a->mx) && !a->dirty) {
        snprintf(a->info, sizeof a->info, "Nothing changed.");
        return 0;
    }
    app_stop(a);
    if (!(nb = mx_save(&a->mx, &a->song, &n))) {
        snprintf(a->info, sizeof a->info, "Not enough memory to save.");
        return 0;
    }
    strncpy(path, a->folder, sizeof path - 1);
    path[sizeof path - 1] = 0;
    AddPart((STRPTR)path, (CONST_STRPTR)a->names[k], sizeof path);
    if (a->path_as[0]) strcpy(path, a->path_as);
    else if (a->is_xws) {
        /* an .XWS is kept as it is: the song goes into <name>.mid next to it */
        char *dot = strrchr(path, '.');
        if (dot && strlen(dot) == 4) strcpy(dot, ".mid");
    }
    /* the first save keeps the original as .bak */
    snprintf(bak, sizeof bak, "%s.bak", path);
    if ((l = Lock((CONST_STRPTR)bak, ACCESS_READ))) UnLock(l);
    else if (!a->is_xws && !a->path_as[0] && (l = Lock((CONST_STRPTR)path, ACCESS_READ))) {
        /* the file as it is on the disk (not the edited song) */
        BPTR fh;
        UnLock(l);
        if ((fh = Open((CONST_STRPTR)path, MODE_OLDFILE))) {
            LONG sz;
            uint8_t *old;
            Seek(fh, 0, OFFSET_END);
            sz = Seek(fh, 0, OFFSET_BEGINNING);
            if (sz > 0 && (old = (uint8_t *)malloc((size_t)sz))) {
                if (Read(fh, old, sz) == sz) write_file(bak, old, (size_t)sz);
                free(old);
            }
            Close(fh);
        }
    }
    if (!write_file(path, nb, n)) {
        free(nb);
        snprintf(a->info, sizeof a->info, "%s cannot be written.", a->names[k]);
        return 0;
    }
    free(nb);
    a->dirty = 0;
    if (a->is_xws || a->path_as[0]) {
        /* a new file: its folder is listed and the song taken from there */
        char saved[120], folder[256];
        int was_xws = a->is_xws && !a->path_as[0];
        strncpy(saved, (const char *)FilePart((STRPTR)path), sizeof saved - 1);
        saved[sizeof saved - 1] = 0;
        strncpy(folder, path, sizeof folder - 1);
        folder[sizeof folder - 1] = 0;
        *PathPart((STRPTR)folder) = 0;
        app_scan(a, folder);
        for (k = 0; k < a->count; k++)
            if (!ci_cmp(a->names[k], saved)) break;
        if (k >= a->count || !app_load(a, k)) return 0;
        snprintf(a->info, sizeof a->info, was_xws ? "Saved as %s (the .XWS is unchanged)." : "Saved as %s.", saved);
        return 1;
    }
    {
        Mixer keep = a->mx;
        a->cur = -1;   /* load it again */
        if (!app_load(a, k)) return 0;
        memcpy(a->mx.mute, keep.mute, sizeof keep.mute);
        memcpy(a->mx.solo, keep.solo, sizeof keep.solo);
    }
    snprintf(a->info, sizeof a->info, "%s saved (the old one: .bak).", a->names[k]);
    return 1;
}

int app_save_as(App *a, void *window)
{
    struct FileRequester *fr;
    char path[256];
    int ok = 0;
    if (a->cur < 0 || !AslBase) return 0;
    fr = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
        ASLFR_TitleText, (ULONG)"Save the song as", ASLFR_DoSaveMode, TRUE,
        ASLFR_InitialDrawer, (ULONG)a->folder, ASLFR_InitialFile, (ULONG)a->names[a->cur],
        window ? ASLFR_Window : TAG_IGNORE, (ULONG)window, TAG_DONE);
    if (!fr) return 0;
    if (AslRequest(fr, NULL) && fr->fr_File && fr->fr_File[0]) {
        char *dot;
        strncpy(path, (const char *)fr->fr_Drawer, sizeof path - 1);
        path[sizeof path - 1] = 0;
        AddPart((STRPTR)path, (CONST_STRPTR)fr->fr_File, sizeof path);
        /* an .XWS name gets .mid: the song is written as MIDI */
        if ((dot = strrchr(path, '.')) && !ci_cmp(dot, ".xws")) strcpy(dot, ".mid");
        strcpy(a->path_as, path);
        a->dirty = 1;
        ok = app_save(a);
    }
    FreeAslRequest(fr);
    return ok;
}

void app_set_letters(App *a, int letters)
{
    a->letters = letters;
    if (a->cur >= 0) {
        ly_free(&a->ly);
        ly_build(&a->ly, &a->song, letters);
    }
}

void app_set_tempo(App *a, int pct)
{
    a->pl.tempo = pct < 25 ? 25 : pct > 400 ? 400 : pct;
}

void app_set_transpose(App *a, int semis)
{
    a->pl.transpose = semis < -24 ? -24 : semis > 24 ? 24 : semis;
}

void app_help(void)
{
    BPTR nil = Open((CONST_STRPTR)"NIL:", MODE_NEWFILE);
    if (SystemTags((CONST_STRPTR)"SYS:Utilities/MultiView PROGDIR:XGWorksLite.guide",
                   SYS_Asynch, TRUE, SYS_Input, (ULONG)nil, SYS_Output, 0, TAG_DONE) == -1 && nil)
        Close(nil);
}

void app_time(char *out, uint32_t us)
{
    uint32_t s = us / 1000000;
    sprintf(out, "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

static int ask(App *a, void *window, int folders)
{
    struct FileRequester *fr;
    int ok = 0;
    if (!AslBase) {
        snprintf(a->info, sizeof a->info, "asl.library is missing.");
        return 0;
    }
    fr = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
        ASLFR_TitleText, (ULONG)(folders ? "Choose the folder of the songs" : "Open a song"),
        ASLFR_InitialDrawer, (ULONG)a->folder,
        ASLFR_DrawersOnly, folders,
        ASLFR_DoPatterns, !folders,
        ASLFR_InitialPattern, (ULONG)"#?.(mid|midi|kar|smf)",
        window ? ASLFR_Window : TAG_IGNORE, (ULONG)window,
        TAG_DONE);
    if (!fr) return 0;
    if (AslRequest(fr, NULL)) {
        char folder[256];
        strncpy(folder, (const char *)fr->fr_Drawer, sizeof folder - 1);
        folder[sizeof folder - 1] = 0;
        if (app_scan(a, folder)) {
            ok = 1;
            if (!folders && fr->fr_File && fr->fr_File[0]) {
                int i;
                for (i = 0; i < a->count; i++)
                    if (!ci_cmp(a->names[i], (const char *)fr->fr_File)) {
                        ok = 2 + i;
                        break;
                    }
            }
        }
    }
    FreeAslRequest(fr);
    return ok;
}

int app_ask_file(App *a, void *window)
{
    /* 2 + index of the chosen song, 1 = only the folder changed, 0 = cancelled */
    return ask(a, window, 0);
}

int app_ask_folder(App *a, void *window)
{
    return ask(a, window, 1);
}

#define PREFS "ENVARC:XGWorksLite.prefs"
#define PREFS_ENV "ENV:XGWorksLite.prefs"

void app_load_prefs(App *a)
{
    char line[300];
    FILE *f = fopen(PREFS_ENV, "r");
    if (!f) f = fopen(PREFS, "r");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char *v = strchr(line, '=');
        size_t n;
        if (!v) continue;
        *v++ = 0;
        n = strlen(v);
        while (n && (v[n - 1] == '\n' || v[n - 1] == '\r')) v[--n] = 0;
        if (!strcmp(line, "folder")) { strncpy(a->folder, v, sizeof a->folder - 1); }
        else if (!strcmp(line, "reset")) a->reset = atoi(v) % RESETS;
        else if (!strcmp(line, "output")) a->output = atoi(v) % OUTPUTS;
        else if (!strcmp(line, "device")) { strncpy(a->serdev, v, sizeof a->serdev - 1); }
        else if (!strcmp(line, "unit")) a->unit = atol(v);
        else if (!strcmp(line, "tempo")) app_set_tempo(a, atoi(v));
        else if (!strcmp(line, "letters")) a->letters = atoi(v) % 3;
        else if (!strcmp(line, "lyricsize")) a->kfont = atoi(v) < 8 ? 24 : atoi(v);
    }
    fclose(f);
}

void app_save_prefs(const App *a)
{
    const char *files[2] = {PREFS_ENV, PREFS};
    int k;
    for (k = 0; k < 2; k++) {
        FILE *f = fopen(files[k], "w");
        if (!f) continue;
        fprintf(f, "folder=%s\nreset=%d\noutput=%d\ndevice=%s\nunit=%ld\ntempo=%d\nletters=%d\nlyricsize=%d\n", a->folder, a->reset, a->output, a->serdev, a->unit, a->pl.tempo, a->letters, a->kfont);
        fclose(f);
    }
}
