/* XGWorks Lite: the shared part of the windows (see app.h). */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/exall.h>
#include <libraries/asl.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/asl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

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
    return d && (!ci_cmp(d, ".mid") || !ci_cmp(d, ".midi") || !ci_cmp(d, ".kar") || !ci_cmp(d, ".smf"));
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

static void unload(App *a)
{
    app_stop(a);
    if (a->data) {
        xg_smf_free(&a->song);
        free(a->data);
    }
    a->data = NULL;
    a->cur = -1;
    a->len_us = 0;
}

void app_free(App *a)
{
    unload(a);
    pl_free(&a->pl);
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
    if ((r = xg_smf_load(&a->song, data, (size_t)size)) != XG_OK) {
        free(data);
        snprintf(a->info, sizeof a->info, "%s: %s.", a->names[k], xg_error(r));
        return 0;
    }
    a->data = data;
    a->cur = k;
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
    p->song = &a->song;
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

void app_set_tempo(App *a, int pct)
{
    a->pl.tempo = pct < 25 ? 25 : pct > 400 ? 400 : pct;
}

void app_set_transpose(App *a, int semis)
{
    a->pl.transpose = semis < -24 ? -24 : semis > 24 ? 24 : semis;
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
        fprintf(f, "folder=%s\nreset=%d\noutput=%d\ndevice=%s\nunit=%ld\ntempo=%d\n", a->folder, a->reset, a->output, a->serdev, a->unit, a->pl.tempo);
        fclose(f);
    }
}
