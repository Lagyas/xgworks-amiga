/* XGWorks Lite: the classic window (GadTools; OS 3.0+, no extra libraries). */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <exec/nodes.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <graphics/text.h>
#include <dos/dos.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui.h"
#include "mixwin.h"
#include "karawin.h"
#include "edwin.h"
#include "pianoroll.h"
#include "drumwin.h"
#include "lyrwin.h"
#include "editors.h"
#include "xgmwin.h"
#include "lyredwin.h"
#include "staffwin.h"

struct Library *GadToolsBase;

enum { G_LIST, G_PREV, G_PLAY, G_STOP, G_NEXT, G_MIX, G_LYR, G_TRK, G_ROLL, G_DRUMS, G_TEXTS, G_XGM, G_LYRED, G_STAFF, G_OPEN, G_FOLDER, G_POS, G_TIME, G_TEMPO, G_TRANS, G_RESET, G_OUT, G_INFO, G_COUNT };

static struct Gadget *gad[G_COUNT];
static struct Window *win;
static struct List names;
static struct Node *nodes;
static char timebuf[40];
static uint32_t shown_pos;

static void set(int g, ULONG tag, ULONG val)
{
    GT_SetGadgetAttrs(gad[g], win, NULL, tag, val, TAG_DONE);
}

static void fill_list(App *a)
{
    int i;
    set(G_LIST, GTLV_Labels, ~0UL);
    free(nodes);
    NewList(&names);
    nodes = (struct Node *)calloc(a->count ? a->count : 1, sizeof(struct Node));
    for (i = 0; nodes && i < a->count; i++) {
        nodes[i].ln_Name = a->names[i];
        AddTail(&names, &nodes[i]);
    }
    set(G_LIST, GTLV_Labels, (ULONG)&names);
}

static void show(App *a)
{
    char x[16], y[16];
    uint32_t pos = app_playing(a) ? a->pl.pos_us : shown_pos;
    app_time(x, pos);
    app_time(y, a->len_us);
    sprintf(timebuf, "%s / %s", x, y);
    set(G_TIME, GTTX_Text, (ULONG)timebuf);
    if (app_playing(a)) {
        shown_pos = pos;
        set(G_POS, GTSL_Level, pos / 1000000);
    }
    kw_update(a, pos, 0);
    eds_position(a, pos, app_playing(a));
    set(G_INFO, GTTX_Text, (ULONG)a->info);
}

static void start(App *a, int k, uint32_t from)
{
    if (k < 0 || k >= a->count) return;
    if (k != a->cur && !app_load(a, k)) { show(a); return; }
    mixgt_refresh(a);
    eds_refresh(a);
    kw_update(a, from, 1);
    shown_pos = from;
    set(G_POS, GTSL_Max, a->len_us / 1000000 + 1);
    set(G_POS, GTSL_Level, from / 1000000);
    set(G_LIST, GTLV_Selected, k);
    set(G_LIST, GTLV_MakeVisible, k);
    app_play(a, from);
    show(a);
}

int gui_gadtools(App *a)
{
    struct Screen *scr;
    void *vi;
    struct Gadget *glist = NULL, *g;
    struct NewGadget ng;
    WORD fh, top, x0, w, y, bh, lh;
    int running = 1, sel = -1;
    ULONG secs0 = 0, mics0 = 0;
    if (!(GadToolsBase = OpenLibrary((CONST_STRPTR)"gadtools.library", 39))) return -1;
    if (!(scr = LockPubScreen(NULL))) { CloseLibrary(GadToolsBase); return -1; }
    vi = GetVisualInfo(scr, TAG_DONE);
    fh = scr->Font->ta_YSize;
    top = scr->WBorTop + fh + 1 + 4;
    x0 = scr->WBorLeft + 8;
    w = 600;
    bh = fh + 6;
    lh = fh * 8 + 4;
    NewList(&names);

    g = CreateContext(&glist);
    memset(&ng, 0, sizeof ng);
    ng.ng_TextAttr = scr->Font;
    ng.ng_VisualInfo = vi;

    y = top;
    ng.ng_LeftEdge = x0; ng.ng_TopEdge = y; ng.ng_Width = w; ng.ng_Height = lh;
    ng.ng_GadgetID = G_LIST;
    g = gad[G_LIST] = CreateGadget(LISTVIEW_KIND, g, &ng, GTLV_ShowSelected, 0, GTLV_Labels, (ULONG)&names, TAG_DONE);
    y += lh + 4;
    {
        /* two rows: playing and files; the windows */
        static const char *const txt[15] = {"|<", "Play", "Stop", ">|", "Open...", "Folder...", "Mixer...", "XGM...", "Lyrics...", "Lyr.Edit...",
                                            "Texts...", "Tracks...", "Roll...", "Drums...", "Staff..."};
        static const WORD bw[15] = {40, 72, 72, 40, 90, 90, 114, 114, 114, 114, 114, 114, 114, 114, 114};
        static const int ids[15] = {G_PREV, G_PLAY, G_STOP, G_NEXT, G_OPEN, G_FOLDER, G_MIX, G_XGM, G_LYR, G_LYRED, G_TEXTS, G_TRK, G_ROLL, G_DRUMS, G_STAFF};
        WORD x = x0;
        int k;
        for (k = 0; k < 15; k++) {
            if (k == 4) x = x0 + w - 184;
            if (k == 6 || k == 11) { x = x0; y += bh + 4; }
            ng.ng_LeftEdge = x; ng.ng_TopEdge = y; ng.ng_Width = bw[k]; ng.ng_Height = bh;
            ng.ng_GadgetText = (UBYTE *)txt[k]; ng.ng_GadgetID = ids[k]; ng.ng_Flags = PLACETEXT_IN;
            g = gad[ids[k]] = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
            x += bw[k] + 4;
        }
    }
    y += bh + 6;
    ng.ng_Flags = PLACETEXT_LEFT;
    ng.ng_LeftEdge = x0 + 90; ng.ng_TopEdge = y; ng.ng_Width = w - 90 - 100; ng.ng_Height = fh + 4;
    ng.ng_GadgetText = (UBYTE *)"Position"; ng.ng_GadgetID = G_POS;
    g = gad[G_POS] = CreateGadget(SLIDER_KIND, g, &ng, GTSL_Min, 0, GTSL_Max, 1, GTSL_Level, 0, GA_RelVerify, TRUE, TAG_DONE);
    ng.ng_LeftEdge = x0 + w - 96; ng.ng_Width = 96; ng.ng_GadgetText = NULL; ng.ng_GadgetID = G_TIME;
    g = gad[G_TIME] = CreateGadget(TEXT_KIND, g, &ng, GTTX_Text, (ULONG)"0:00 / 0:00", TAG_DONE);
    y += fh + 8;
    ng.ng_LeftEdge = x0 + 90; ng.ng_TopEdge = y; ng.ng_Width = w - 90 - 60;
    ng.ng_GadgetText = (UBYTE *)"Tempo"; ng.ng_GadgetID = G_TEMPO;
    g = gad[G_TEMPO] = CreateGadget(SLIDER_KIND, g, &ng, GTSL_Min, 25, GTSL_Max, 400, GTSL_Level, a->pl.tempo, GTSL_LevelFormat, (ULONG)"%3ld%%", GTSL_MaxLevelLen, 5, GTSL_LevelPlace, PLACETEXT_RIGHT, GA_RelVerify, TRUE, TAG_DONE);
    y += fh + 8;
    ng.ng_TopEdge = y; ng.ng_GadgetText = (UBYTE *)"Transpose"; ng.ng_GadgetID = G_TRANS;
    g = gad[G_TRANS] = CreateGadget(SLIDER_KIND, g, &ng, GTSL_Min, -12, GTSL_Max, 12, GTSL_Level, a->pl.transpose, GTSL_LevelFormat, (ULONG)"%+3ld", GTSL_MaxLevelLen, 4, GTSL_LevelPlace, PLACETEXT_RIGHT, GA_RelVerify, TRUE, TAG_DONE);
    y += fh + 8;
    ng.ng_TopEdge = y; ng.ng_Width = 130; ng.ng_Height = bh; ng.ng_GadgetText = (UBYTE *)"Reset"; ng.ng_GadgetID = G_RESET;
    g = gad[G_RESET] = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)app_reset_names, GTCY_Active, a->reset, TAG_DONE);
    ng.ng_LeftEdge = x0 + w - 150; ng.ng_Width = 150; ng.ng_GadgetText = (UBYTE *)"Output"; ng.ng_GadgetID = G_OUT;
    g = gad[G_OUT] = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)app_output_names, GTCY_Active, a->output, TAG_DONE);
    y += bh + 6;
    ng.ng_LeftEdge = x0; ng.ng_TopEdge = y; ng.ng_Width = w; ng.ng_Height = fh + 4; ng.ng_GadgetText = NULL; ng.ng_GadgetID = G_INFO;
    g = gad[G_INFO] = CreateGadget(TEXT_KIND, g, &ng, GTTX_Text, (ULONG)a->info, GTTX_Border, TRUE, TAG_DONE);
    y += fh + 8;

    if (!g) {
        FreeGadgets(glist);
        FreeVisualInfo(vi);
        UnlockPubScreen(NULL, scr);
        return -1;
    }
    win = OpenWindowTags(NULL,
        WA_Title, (ULONG)"XGWorks Lite",
        WA_InnerWidth, w + 16, WA_InnerHeight, y - scr->WBorTop - fh - 1 + 2,
        WA_Left, 40, WA_Top, 30,
        WA_Gadgets, (ULONG)glist,
        WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE,
        WA_PubScreen, (ULONG)scr,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_VANILLAKEY | IDCMP_RAWKEY | LISTVIEWIDCMP | SLIDERIDCMP | BUTTONIDCMP | CYCLEIDCMP,
        TAG_DONE);
    UnlockPubScreen(NULL, scr);
    if (!win) {
        FreeGadgets(glist);
        FreeVisualInfo(vi);
        return -1;
    }
    GT_RefreshWindow(win, NULL);
    fill_list(a);
    show(a);

    while (running) {
        struct IntuiMessage *m;
        ULONG sigs = Wait((1UL << win->UserPort->mp_SigBit) | pl_signal() | mixgt_signal() | kw_signal() | eds_signal() | SIGBREAKF_CTRL_C);
        if (sigs & eds_signal()) {
            if (eds_handle(a, sigs)) {
                mixgt_refresh(a);
                if (a->cur >= 0) set(G_POS, GTSL_Max, a->len_us / 1000000 + 1);
                kw_update(a, shown_pos, 1);
            }
            show(a);
        }
        if (sigs & kw_signal()) kw_handle(a, app_playing(a) ? a->pl.pos_us : shown_pos);
        if (sigs & mixgt_signal()) {
            mixgt_handle(a);
            if (a->cur >= 0) set(G_POS, GTSL_Max, a->len_us / 1000000 + 1);
            show(a);
        }
        if (sigs & SIGBREAKF_CTRL_C) running = 0;
        if (sigs & pl_signal()) {
            if (app_poll(a) && a->autonext && a->cur + 1 < a->count) start(a, a->cur + 1, 0);
            else show(a);
        }
        while (running && (m = GT_GetIMsg(win->UserPort))) {
            ULONG cls = m->Class, secs = m->Seconds, mics = m->Micros;
            UWORD code = m->Code;
            struct Gadget *src = (struct Gadget *)m->IAddress;
            GT_ReplyIMsg(m);
            switch (cls) {
            case IDCMP_CLOSEWINDOW:
                running = 0;
                break;
            case IDCMP_REFRESHWINDOW:
                GT_BeginRefresh(win);
                GT_EndRefresh(win, TRUE);
                break;
            case IDCMP_RAWKEY:
                if (code == 0x5F) app_help();   /* the Help key */
                break;
            case IDCMP_VANILLAKEY:
                if (code == 'p' || code == 'P' || code == ' ') start(a, sel >= 0 ? sel : (a->cur >= 0 ? a->cur : 0), sel == a->cur || sel < 0 ? shown_pos : 0);
                else if (code == 's' || code == 'S') { app_stop(a); show(a); }
                else if (code == 27) running = 0;
                break;
            case IDCMP_MOUSEMOVE:
            case IDCMP_GADGETUP:
                switch (src->GadgetID) {
                case G_LIST:
                    if (cls != IDCMP_GADGETUP) break;
                    /* a double click plays */
                    if ((int)code == sel && DoubleClick(secs0, mics0, secs, mics)) start(a, sel, 0);
                    sel = code;
                    secs0 = secs; mics0 = mics;
                    break;
                case G_PLAY: {
                    int k = sel >= 0 ? sel : (a->cur >= 0 ? a->cur : 0);
                    start(a, k, k == a->cur ? shown_pos : 0);
                    break;
                }
                case G_STOP:
                    app_stop(a);
                    show(a);
                    break;
                case G_MIX:
                    mixgt_open(a);
                    break;
                case G_LYR:
                    kw_open(a);
                    break;
                case G_TRK:
                    edw_tracks_open(a);
                    show(a);
                    break;
                case G_ROLL:
                    pr_open(a);
                    show(a);
                    break;
                case G_DRUMS:
                    dw_open(a);
                    show(a);
                    break;
                case G_TEXTS:
                    lw_open(a);
                    show(a);
                    break;
                case G_XGM:
                    xw_open(a);
                    break;
                case G_LYRED:
                    yw_open(a);
                    show(a);
                    break;
                case G_STAFF:
                    sw_open(a);
                    show(a);
                    break;
                case G_PREV:
                case G_NEXT: {
                    int k = a->cur + (src->GadgetID == G_NEXT ? 1 : -1);
                    if (k >= 0 && k < a->count) { sel = k; start(a, k, 0); }
                    break;
                }
                case G_OPEN:
                case G_FOLDER: {
                    int r = src->GadgetID == G_OPEN ? app_ask_file(a, win) : app_ask_folder(a, win);
                    if (r) { fill_list(a); sel = -1; }
                    if (r >= 2) { sel = r - 2; start(a, sel, 0); }
                    show(a);
                    break;
                }
                case G_POS:
                    shown_pos = (uint32_t)code * 1000000UL;
                    if (cls == IDCMP_GADGETUP && app_playing(a)) app_play(a, shown_pos);
                    show(a);
                    break;
                case G_TEMPO:
                    app_set_tempo(a, (WORD)code);
                    break;
                case G_TRANS:
                    app_set_transpose(a, (WORD)code);
                    break;
                case G_RESET:
                    a->reset = code;
                    break;
                case G_OUT:
                    a->output = code;
                    break;
                }
                break;
            }
        }
    }
    app_stop(a);
    mixgt_close();
    kw_close();
    eds_close();
    CloseWindow(win);
    win = NULL;
    FreeGadgets(glist);
    FreeVisualInfo(vi);
    free(nodes);
    nodes = NULL;
    CloseLibrary(GadToolsBase);
    return 0;
}
