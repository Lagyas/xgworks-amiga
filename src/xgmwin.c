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
#include "screen.h"

extern struct Library *GadToolsBase;

enum { X_PART = 1, X_PROGS, X_VARS, X_DEFAULT, X_SOUND, X_STD, X_SL0 = 20 };

static struct Window *xw;
static struct Gadget *xgl, *g_progs, *g_vars, *g_sound, *g_part, *g_std, *g_sl[XP_N];
static void *xvi;
static int xpart, xprog = -1;
static struct List lprogs, lvars;
static struct Node nprogs[130];
static char tprogs[130][44];
static struct Node *nvars;
static char (*tvars)[48];
static struct { uint8_t m, l, p; } *vch;   /* the sounds of the right list: bank MSB / LSB, program */
static int nvarn;
/* the standard of the sounds (as the web version's sound choice): Auto = by the song */
static const char *const STDS[] = {"Auto (song)", "XG", "GM", "GM2", "GS (SC-8850)", NULL};
static char soundbuf[64];

static const char *const PARTS[] = {"Part 1", "Part 2", "Part 3", "Part 4", "Part 5", "Part 6", "Part 7", "Part 8", "Part 9",
                                    "Part 10", "Part 11", "Part 12", "Part 13", "Part 14", "Part 15", "Part 16", NULL};

int xw_is_open(void) { return xw != NULL; }
unsigned long xw_signal(void) { return xw ? 1UL << xw->UserPort->mp_SigBit : 0; }

/* the program list: 128 programs, then the SFX and the drum kits */
static void fill_progs(void)
{
    int p, std = vx_std();
    NewList(&lprogs);
    for (p = 0; p < 130; p++) {
        if (p < 128) {
            const char *n = std == VX_XG ? vx_name(0, 0, p) : vx_sound(0, std == VX_GM2 ? 121 : 0, 0, p);
            snprintf(tprogs[p], sizeof tprogs[p], "%3d %s", p + 1, n ? n : mx_gm_names[p]);
        } else snprintf(tprogs[p], sizeof tprogs[p], "%s", p == 128 ? (std == VX_XG ? "--- SFX ---" : "--- (no SFX bank) ---") : "--- Drum kits ---");
        nprogs[p].ln_Name = tprogs[p];
        AddTail(&lprogs, &nprogs[p]);
    }
}

static void add_var(int m, int l, int pr, const char *text)
{
    snprintf(tvars[nvarn], 48, "%s", text);
    nvars[nvarn].ln_Name = tvars[nvarn];
    AddTail(&lvars, &nvars[nvarn]);
    vch[nvarn].m = (uint8_t)m; vch[nvarn].l = (uint8_t)l; vch[nvarn].p = (uint8_t)pr;
    nvarn++;
}

/* the sounds of program `p` in the banks of the standard (128 = SFX, 129 = kits) */
static void fill_vars(int p)
{
    int i, cnt = vx_count() + vx_gs_count() + 200, std = vx_std(), first;
    char t[48];
    GT_SetGadgetAttrs(g_vars, xw, NULL, GTLV_Labels, ~0UL, TAG_DONE);
    free(nvars); free(tvars); free(vch);
    nvars = NULL; tvars = NULL; vch = NULL; nvarn = 0;
    NewList(&lvars);
    if (!(nvars = (struct Node *)calloc((size_t)cnt, sizeof(struct Node))) || !(tvars = (char (*)[48])calloc((size_t)cnt, 48)) ||
        !(vch = calloc((size_t)cnt, sizeof *vch))) {
        GT_SetGadgetAttrs(g_vars, xw, NULL, GTLV_Labels, (ULONG)&lvars, TAG_DONE);
        return;
    }
    if (p == 129) {
        /* the drum kits of the standard */
        int m, pr;
        const char *n;
        for (i = 0; nvarn < cnt && vx_kit(std, i, &m, &pr, &n); i++) {
            snprintf(t, sizeof t, "%3d/%-3d %s", m, pr + 1, n);
            add_var(m, 0, pr, t);
        }
    } else if (p == 128) {
        for (i = 0; std == VX_XG && i < vx_count() && nvarn < cnt; i++) {
            const VxVoice *v = vx_get(i);
            if (v->msb != 64) continue;
            snprintf(t, sizeof t, "%3d/%-3d %s", v->msb, v->prog + 1, vx_text(v->name));
            add_var(64, 0, v->prog, t);
        }
    } else {
        first = 1;
        if (std == VX_GS) {
            /* the SC-8850 variations (bank MSB) of the program */
            int gm, gl, gp;
            const char *gn;
            for (i = 0; nvarn < cnt && vx_gs_get(i, &gm, &gl, &gp, &gn); i++)
                if (gp == p) {
                    snprintf(t, sizeof t, "%3d/%-3d %s", gm, gl, gn);
                    add_var(gm, gl, gp, t);
                }
        } else if (std == VX_XG || std == VX_GM2) {
            /* XG: every bank of the lists (MU voices, MU Sampling, VL, PLG, GM2 ...); GM2: MSB 121 */
            for (i = 0; i < vx_count() && nvarn < cnt; i++) {
                const VxVoice *v = vx_get(i);
                if (v->prog != p || v->msb >= 126 || v->msb == 120 || v->msb == 64) continue;
                if (std == VX_GM2 && v->msb != 121) continue;
                snprintf(t, sizeof t, "%3d/%-3d %s", v->msb, v->lsb, vx_text(v->name));
                add_var(v->msb, v->lsb, v->prog, t);
            }
        }
        (void)first;
        if (!nvarn) {
            /* GM, or no lists: the GM sound */
            int m = std == VX_GM2 ? 121 : 0;
            snprintf(t, sizeof t, "%3d/0   %s", m, mx_gm_names[p]);
            add_var(m, 0, p, t);
        }
    }
    GT_SetGadgetAttrs(g_vars, xw, NULL, GTLV_Labels, (ULONG)&lvars, TAG_DONE);
}

/* the row of the program list for the part's sound */
static int prog_row(const uint8_t *v)
{
    int std = vx_std();
    if (xpart == 9 && std != VX_XG) return 129;
    if (std == VX_XG && (v[MX_MSB] >= 126 || v[MX_MSB] == 120 || (xpart == 9 && v[MX_MSB] == 0))) return 129;
    if (std == VX_GM2 && v[MX_MSB] == 120) return 129;
    if (std == VX_XG && v[MX_MSB] == 64) return 128;
    return v[MX_PROG] & 127;
}

static void show(App *a)
{
    const uint8_t *v = a->mx.val[xpart];
    const char *n = vx_sound(xpart, v[MX_MSB], v[MX_LSB], v[MX_PROG]);
    int p;
    if (!xw) return;
    snprintf(soundbuf, sizeof soundbuf, "%d/%d/%d  %s", v[MX_MSB], v[MX_LSB], v[MX_PROG] + 1, n);
    GT_SetGadgetAttrs(g_sound, xw, NULL, GTTX_Text, (ULONG)soundbuf, TAG_DONE);
    for (p = 0; p < XP_N; p++) GT_SetGadgetAttrs(g_sl[p], xw, NULL, GTSL_Level, (WORD)(a->mx.xp[xpart][p] - mx_xp[p].shown), TAG_DONE);
    if (xprog != prog_row(v)) {
        xprog = prog_row(v);
        GT_SetGadgetAttrs(g_progs, xw, NULL, GTLV_Selected, xprog, GTLV_MakeVisible, xprog, TAG_DONE);
        fill_vars(xprog);
    }
}

int xw_open_part(App *a, int part)
{
    if (part < 0 || part > 15) return xw_open(a);
    xpart = part;
    if (xw) {
        GT_SetGadgetAttrs(g_part, xw, NULL, GTCY_Active, xpart, TAG_DONE);
        xprog = -1;
    }
    return xw_open(a);
}

int xw_open_kits(App *a, int part)
{
    int r = xw_open_part(a, part);
    if (r && xw) {
        xprog = 129;
        GT_SetGadgetAttrs(g_progs, xw, NULL, GTLV_Selected, 129, GTLV_MakeVisible, 129, TAG_DONE);
        fill_vars(129);
    }
    return r;
}

int xw_open(App *a)
{
    struct Screen *scr;
    struct NewGadget ng;
    struct Gadget *g;
    WORD fh, x0, y, lw = 230, lh;
    int p;
    if (xw) { WindowToFront(xw); ActivateWindow(xw); show(a); return 1; }
    if (!(scr = LockPubScreen((CONST_STRPTR)scr_pubname()))) return 0;
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
    ng.ng_LeftEdge = x0 + 94; ng.ng_Width = 136; ng.ng_GadgetID = X_STD;
    g = g_std = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)STDS, GTCY_Active, a->std_user + 1, TAG_DONE);
    ng.ng_LeftEdge = x0 + 234; ng.ng_Width = lw * 2 - 234 + 4; ng.ng_GadgetID = X_SOUND;
    g = g_sound = CreateGadget(TEXT_KIND, g, &ng, GTTX_Text, (ULONG)"", GTTX_Border, TRUE, TAG_DONE);
    y += fh + 8;
    y += fh + 2;
    ng.ng_TopEdge = y; ng.ng_LeftEdge = x0; ng.ng_Width = lw; ng.ng_Height = lh; ng.ng_GadgetID = X_PROGS;
    ng.ng_GadgetText = (UBYTE *)"1. Program"; ng.ng_Flags = PLACETEXT_ABOVE;
    g = g_progs = CreateGadget(LISTVIEW_KIND, g, &ng, GTLV_Labels, (ULONG)&lprogs, GTLV_ShowSelected, 0, TAG_DONE);
    ng.ng_LeftEdge = x0 + lw + 4; ng.ng_GadgetID = X_VARS;
    ng.ng_GadgetText = (UBYTE *)"2. Bank MSB/LSB - sound";
    g = g_vars = CreateGadget(LISTVIEW_KIND, g, &ng, GTLV_Labels, (ULONG)&lvars, GTLV_ShowSelected, 0, TAG_DONE);
    ng.ng_GadgetText = NULL; ng.ng_Flags = 0;
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
    xw = OpenWindowTags(NULL, WA_Title, (ULONG)"Sound / bank - XGM part", WA_Left, 30, WA_Top, 16,
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
    free(nvars); free(tvars); free(vch);
    nvars = NULL; tvars = NULL; vch = NULL;
}

void xw_refresh(App *a)
{
    if (!xw) return;
    /* the program names again (GS / XG may have changed with the song) */
    GT_SetGadgetAttrs(g_progs, xw, NULL, GTLV_Labels, ~0UL, TAG_DONE);
    fill_progs();
    GT_SetGadgetAttrs(g_progs, xw, NULL, GTLV_Labels, (ULONG)&lprogs, TAG_DONE);
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
        case X_STD:
            /* GM / GM2 / GS / XG: the names and the sounds offered */
            a->std_user = (int)code - 1;
            app_names_std(a);
            GT_SetGadgetAttrs(g_progs, xw, NULL, GTLV_Labels, ~0UL, TAG_DONE);
            fill_progs();
            GT_SetGadgetAttrs(g_progs, xw, NULL, GTLV_Labels, (ULONG)&lprogs, TAG_DONE);
            xprog = -1;
            show(a);
            changed = 1;
            break;
        case X_PROGS:
            if (cls != IDCMP_GADGETUP) break;
            xprog = code;
            fill_vars(xprog);
            /* a program alone: its main sound (the first of its banks) */
            if (xprog < 128 && nvarn) { choose(a, vch[0].m, vch[0].l, vch[0].p); changed = 1; show(a); }
            break;
        case X_VARS:
            if (cls != IDCMP_GADGETUP) break;
            if (code < nvarn) {
                choose(a, vch[code].m, vch[code].l, vch[code].p);
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
