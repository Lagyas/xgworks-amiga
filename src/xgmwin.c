/* XGWorks Lite: the XGM part editor - the sound of a part chosen by name
 * from the XGworks voice lists (programs on the left, their variations /
 * the drum kits on the right; without the lists: GM names) and the XG part
 * parameters (note shift, variation send, vibrato, filter, envelope, bend
 * range) as sliders. Changes are heard at once and saved like the mixer's. */
#include <exec/types.h>
#include <exec/lists.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xgmwin.h"
#include "voices.h"

extern struct Library *GadToolsBase;

enum { X_PART = 1, X_PROGS, X_VARS, X_DEFAULT, X_SOUND, X_SL0 = 20 };

static struct Window *xw;
static struct Gadget *xgl, *g_progs, *g_vars, *g_sound, *g_part, *g_sl[XP_N];
static void *xvi;
static int xpart, xprog = -1;
static struct List lprogs, lvars;
static struct Node nprogs[130];
static char tprogs[130][44];
static struct Node *nvars;
static char (*tvars)[48];
static int *vidx, nvarn;
static char soundbuf[64];

static const char *const PARTS[] = {"Part 1", "Part 2", "Part 3", "Part 4", "Part 5", "Part 6", "Part 7", "Part 8", "Part 9",
                                    "Part 10", "Part 11", "Part 12", "Part 13", "Part 14", "Part 15", "Part 16", NULL};

int xw_is_open(void) { return xw != NULL; }
unsigned long xw_signal(void) { return xw ? 1UL << xw->UserPort->mp_SigBit : 0; }

/* the program list: 128 programs, then the SFX and the drum kits */
static void fill_progs(void)
{
    int p;
    NewList(&lprogs);
    for (p = 0; p < 130; p++) {
        if (p < 128) {
            const char *n = vx_name(0, 0, p);
            snprintf(tprogs[p], sizeof tprogs[p], "%3d %s", p + 1, n ? n : mx_gm_names[p]);
        } else snprintf(tprogs[p], sizeof tprogs[p], "%s", p == 128 ? "--- SFX ---" : "--- Drum kits ---");
        nprogs[p].ln_Name = tprogs[p];
        AddTail(&lprogs, &nprogs[p]);
    }
}

/* the variations of program `p` (128 = SFX, 129 = kits) */
static void fill_vars(int p)
{
    int i, n = 0, cnt = vx_count();
    GT_SetGadgetAttrs(g_vars, xw, NULL, GTLV_Labels, ~0UL, TAG_DONE);
    free(nvars); free(tvars); free(vidx);
    nvars = NULL; tvars = NULL; vidx = NULL; nvarn = 0;
    NewList(&lvars);
    if (cnt && (nvars = (struct Node *)calloc((size_t)cnt + 1, sizeof(struct Node))) && (tvars = (char (*)[48])calloc((size_t)cnt + 1, 48)) &&
        (vidx = (int *)calloc((size_t)cnt + 1, sizeof(int)))) {
        for (i = 0; i < cnt; i++) {
            const VxVoice *v = vx_get(i);
            int ok = p < 128 ? (v->prog == p && v->msb < 126) : p == 128 ? v->msb == 64 : v->msb >= 126;
            if (!ok) continue;
            snprintf(tvars[n], 48, "%3d/%-3d %s", v->msb, p < 128 ? v->lsb : v->prog + 1, vx_text(v->name));
            nvars[n].ln_Name = tvars[n];
            AddTail(&lvars, &nvars[n]);
            vidx[n++] = i;
        }
    }
    if (!n && p < 128 && (nvars = (struct Node *)calloc(1, sizeof(struct Node))) && (tvars = (char (*)[48])calloc(1, 48))) {
        /* no voice lists: the GM sound */
        snprintf(tvars[0], 48, "  0/0   %s", mx_gm_names[p]);
        nvars[0].ln_Name = tvars[0];
        AddTail(&lvars, &nvars[0]);
    }
    nvarn = n;
    GT_SetGadgetAttrs(g_vars, xw, NULL, GTLV_Labels, (ULONG)&lvars, TAG_DONE);
}

static void show(App *a)
{
    const uint8_t *v = a->mx.val[xpart];
    const char *n = vx_name(v[MX_MSB], v[MX_LSB], v[MX_PROG]);
    int p;
    if (!xw) return;
    snprintf(soundbuf, sizeof soundbuf, "%d/%d/%d  %s", v[MX_MSB], v[MX_LSB], v[MX_PROG] + 1,
             n ? n : v[MX_MSB] >= 126 ? "Drums" : mx_gm_names[v[MX_PROG] & 127]);
    GT_SetGadgetAttrs(g_sound, xw, NULL, GTTX_Text, (ULONG)soundbuf, TAG_DONE);
    for (p = 0; p < XP_N; p++) GT_SetGadgetAttrs(g_sl[p], xw, NULL, GTSL_Level, (WORD)(a->mx.xp[xpart][p] - mx_xp[p].shown), TAG_DONE);
    if (xprog != (v[MX_MSB] >= 126 ? 129 : v[MX_MSB] == 64 ? 128 : v[MX_PROG])) {
        xprog = v[MX_MSB] >= 126 ? 129 : v[MX_MSB] == 64 ? 128 : v[MX_PROG];
        GT_SetGadgetAttrs(g_progs, xw, NULL, GTLV_Selected, xprog, GTLV_MakeVisible, xprog, TAG_DONE);
        fill_vars(xprog);
    }
}

int xw_open(App *a)
{
    struct Screen *scr;
    struct NewGadget ng;
    struct Gadget *g;
    WORD fh, x0, y, lw = 230, lh;
    int p;
    if (xw) { WindowToFront(xw); ActivateWindow(xw); show(a); return 1; }
    if (!(scr = LockPubScreen(NULL))) return 0;
    xvi = GetVisualInfo(scr, TAG_DONE);
    fh = scr->Font->ta_YSize;
    x0 = (WORD)(scr->WBorLeft + 6);
    y = (WORD)(scr->WBorTop + fh + 1 + 4);
    lh = (WORD)(fh * 8 + 4);
    memset(&ng, 0, sizeof ng);
    ng.ng_TextAttr = scr->Font;
    ng.ng_VisualInfo = xvi;
    g = CreateContext(&xgl);
    fill_progs();
    NewList(&lvars);
    ng.ng_TopEdge = y; ng.ng_Height = fh + 4; ng.ng_LeftEdge = x0; ng.ng_Width = 90; ng.ng_GadgetID = X_PART;
    g = g_part = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)PARTS, GTCY_Active, xpart, TAG_DONE);
    ng.ng_LeftEdge = x0 + 96; ng.ng_Width = lw * 2 - 96 + 4; ng.ng_GadgetID = X_SOUND;
    g = g_sound = CreateGadget(TEXT_KIND, g, &ng, GTTX_Text, (ULONG)"", GTTX_Border, TRUE, TAG_DONE);
    y += fh + 8;
    ng.ng_TopEdge = y; ng.ng_LeftEdge = x0; ng.ng_Width = lw; ng.ng_Height = lh; ng.ng_GadgetID = X_PROGS;
    g = g_progs = CreateGadget(LISTVIEW_KIND, g, &ng, GTLV_Labels, (ULONG)&lprogs, GTLV_ShowSelected, 0, TAG_DONE);
    ng.ng_LeftEdge = x0 + lw + 4; ng.ng_GadgetID = X_VARS;
    g = g_vars = CreateGadget(LISTVIEW_KIND, g, &ng, GTLV_Labels, (ULONG)&lvars, GTLV_ShowSelected, 0, TAG_DONE);
    y += lh + 6;
    ng.ng_Height = fh + 4; ng.ng_Flags = PLACETEXT_LEFT;
    for (p = 0; p < XP_N; p++) {
        int col = p % 2;
        ng.ng_LeftEdge = (WORD)(x0 + 104 + col * (lw + 4)); ng.ng_TopEdge = (WORD)(y + (p / 2) * (fh + 6)); ng.ng_Width = lw - 104 - 34;
        ng.ng_GadgetText = (UBYTE *)mx_xp[p].name; ng.ng_GadgetID = X_SL0 + p;
        g = g_sl[p] = CreateGadget(SLIDER_KIND, g, &ng, GTSL_Min, mx_xp[p].min - mx_xp[p].shown, GTSL_Max, mx_xp[p].max - mx_xp[p].shown,
                                   GTSL_Level, mx_xp[p].def - mx_xp[p].shown, GTSL_LevelFormat, (ULONG)"%+4ld", GTSL_MaxLevelLen, 4,
                                   GTSL_LevelPlace, PLACETEXT_RIGHT, GA_RelVerify, TRUE, TAG_DONE);
    }
    y += (WORD)(((XP_N + 1) / 2) * (fh + 6) + 4);
    ng.ng_TopEdge = y; ng.ng_LeftEdge = x0; ng.ng_Width = 200; ng.ng_Height = fh + 6; ng.ng_Flags = PLACETEXT_IN;
    ng.ng_GadgetText = (UBYTE *)"Part parameters: default"; ng.ng_GadgetID = X_DEFAULT;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    y += fh + 10;
    if (!g) { FreeGadgets(xgl); xgl = NULL; FreeVisualInfo(xvi); UnlockPubScreen(NULL, scr); return 0; }
    xw = OpenWindowTags(NULL, WA_Title, (ULONG)"XGM - part", WA_Left, 30, WA_Top, 16,
        WA_InnerWidth, lw * 2 + 16, WA_InnerHeight, y - scr->WBorTop - fh - 1,
        WA_Gadgets, (ULONG)xgl, WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE,
        WA_PubScreen, (ULONG)scr,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | LISTVIEWIDCMP | SLIDERIDCMP | CYCLEIDCMP | BUTTONIDCMP, TAG_DONE);
    UnlockPubScreen(NULL, scr);
    if (!xw) { FreeGadgets(xgl); xgl = NULL; FreeVisualInfo(xvi); return 0; }
    GT_RefreshWindow(xw, NULL);
    xprog = -1;
    show(a);
    return 1;
}

void xw_close(void)
{
    if (!xw) return;
    CloseWindow(xw);
    xw = NULL;
    FreeGadgets(xgl); xgl = NULL;
    FreeVisualInfo(xvi); xvi = NULL;
    free(nvars); free(tvars); free(vidx);
    nvars = NULL; tvars = NULL; vidx = NULL;
}

void xw_refresh(App *a)
{
    if (!xw) return;
    xprog = -1;
    show(a);
}

/* choose a sound: bank and program set (and heard) at once */
static void choose(App *a, int msb, int lsb, int prog)
{
    a->mx.val[xpart][MX_MSB] = (uint8_t)msb; a->mx.touched[xpart][MX_MSB] = 1;
    a->mx.val[xpart][MX_LSB] = (uint8_t)lsb; a->mx.touched[xpart][MX_LSB] = 1;
    app_mix(a, xpart, MX_PROG, prog);
}

int xw_handle(App *a)
{
    struct IntuiMessage *m;
    int changed = 0, close = 0;
    while (xw && (m = GT_GetIMsg(xw->UserPort))) {
        ULONG cls = m->Class;
        UWORD code = m->Code;
        struct Gadget *src = (struct Gadget *)m->IAddress;
        GT_ReplyIMsg(m);
        if (cls == IDCMP_CLOSEWINDOW) { close = 1; break; }
        if (cls == IDCMP_REFRESHWINDOW) { GT_BeginRefresh(xw); GT_EndRefresh(xw, TRUE); continue; }
        if (cls != IDCMP_GADGETUP && cls != IDCMP_MOUSEMOVE) continue;
        switch (src->GadgetID) {
        case X_PART:
            xpart = code;
            xprog = -1;
            show(a);
            break;
        case X_PROGS:
            if (cls != IDCMP_GADGETUP) break;
            xprog = code;
            fill_vars(xprog);
            /* a program alone: its main sound */
            if (xprog < 128) { choose(a, 0, 0, xprog); changed = 1; show(a); }
            break;
        case X_VARS:
            if (cls != IDCMP_GADGETUP) break;
            if (code < nvarn) {
                const VxVoice *v = vx_get(vidx[code]);
                choose(a, v->msb, v->msb >= 126 || v->msb == 64 ? 0 : v->lsb, v->prog);
                changed = 1;
                show(a);
            }
            break;
        case X_DEFAULT: {
            int p;
            for (p = 0; p < XP_N; p++) app_xp(a, xpart, p, mx_xp[p].def);
            changed = 1;
            show(a);
            break;
        }
        default:
            if (src->GadgetID >= X_SL0 && src->GadgetID < X_SL0 + XP_N) {
                int p = src->GadgetID - X_SL0;
                app_xp(a, xpart, p, (WORD)code + mx_xp[p].shown);
                changed = cls == IDCMP_GADGETUP;
            }
            break;
        }
    }
    if (close) xw_close();
    return changed;
}
