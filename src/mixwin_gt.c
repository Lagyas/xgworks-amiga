/* XGWorks Lite: the GadTools mixer window - 16 rows: part, sound name,
 * program / bank MSB / LSB (numbers), volume, pan, reverb, chorus
 * (sliders), mute, solo; Save and Undo. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <graphics/text.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <stdio.h>
#include <string.h>
#include "mixwin.h"

extern struct Library *GadToolsBase;

enum { C_PROG, C_MSB, C_LSB, C_VOL, C_PAN, C_REV, C_CHO, C_MUTE, C_SOLO, C_NAME, C_N };
#define G_SAVE (16 * 16)
#define G_UNDO (G_SAVE + 1)
#define G_FX (G_SAVE + 10)   /* + what */
static const int PARAM[C_MUTE] = {MX_PROG, MX_MSB, MX_LSB, MX_VOL, MX_PAN, MX_REV, MX_CHO};

static struct Window *mw;
static struct Gadget *glist, *g_[16][C_N], *gfx[FX_N], *gstd;
static void *vi;
static struct Screen *scr;

int mixgt_is_open(void) { return mw != NULL; }
unsigned long mixgt_signal(void) { return mw ? 1UL << mw->UserPort->mp_SigBit : 0; }

static const char *sound(const App *a, int c)
{
    const uint8_t *v = a->mx.val[c];
    if (v[MX_MSB] == 127 || (c == 9 && v[MX_MSB] == 0)) return "Drums";
    return mx_gm_names[v[MX_PROG] & 127];
}

void mixgt_refresh(App *a)
{
    static char title[120];
    int c;
    if (!mw) return;
    snprintf(title, sizeof title, "Mixer - %s", a->cur >= 0 ? a->names[a->cur] : "no song");
    SetWindowTitles(mw, (CONST_STRPTR)title, (CONST_STRPTR)~0);
    for (c = 0; c < 16; c++) {
        const uint8_t *v = a->mx.val[c];
        GT_SetGadgetAttrs(g_[c][C_NAME], mw, NULL, GTTX_Text, (ULONG)sound(a, c), TAG_DONE);
        GT_SetGadgetAttrs(g_[c][C_PROG], mw, NULL, GTIN_Number, v[MX_PROG] + 1, TAG_DONE);
        GT_SetGadgetAttrs(g_[c][C_MSB], mw, NULL, GTIN_Number, v[MX_MSB], TAG_DONE);
        GT_SetGadgetAttrs(g_[c][C_LSB], mw, NULL, GTIN_Number, v[MX_LSB], TAG_DONE);
        GT_SetGadgetAttrs(g_[c][C_VOL], mw, NULL, GTSL_Level, v[MX_VOL], TAG_DONE);
        GT_SetGadgetAttrs(g_[c][C_PAN], mw, NULL, GTSL_Level, v[MX_PAN] - 64, TAG_DONE);
        GT_SetGadgetAttrs(g_[c][C_REV], mw, NULL, GTSL_Level, v[MX_REV], TAG_DONE);
        GT_SetGadgetAttrs(g_[c][C_CHO], mw, NULL, GTSL_Level, v[MX_CHO], TAG_DONE);
        GT_SetGadgetAttrs(g_[c][C_MUTE], mw, NULL, GTCB_Checked, a->mx.mute[c], TAG_DONE);
        GT_SetGadgetAttrs(g_[c][C_SOLO], mw, NULL, GTCB_Checked, a->mx.solo[c], TAG_DONE);
    }
    {
        const char *const *n;
        int k;
        for (k = 0; k < 2; k++) {
            fx_types(a->mx.fx.gs, k, &n);
            GT_SetGadgetAttrs(gfx[k], mw, NULL, GTCY_Labels, (ULONG)n, GTCY_Active, a->mx.fx.sel[k], TAG_DONE);
            GT_SetGadgetAttrs(gfx[2 + k], mw, NULL, GTSL_Level, a->mx.fx.ret[k], TAG_DONE);
        }
        GT_SetGadgetAttrs(gstd, mw, NULL, GTTX_Text, (ULONG)(a->mx.fx.gs ? "GS" : "XG"), TAG_DONE);
    }
}

int mixgt_open(App *a)
{
    struct NewGadget ng;
    struct Gadget *g;
    WORD fh, top, x0, y, rh, x;
    int c, k;
    static char nums[16][4];
    static const char *const heads[] = {"Part", "Sound", "Prog", "MSB", "LSB", "Volume", "Pan", "Reverb", "Chorus", "M", "S"};
    static const WORD hx[] = {0, 24, 132, 168, 204, 240, 316, 392, 468, 544, 570};
    if (mw) { WindowToFront(mw); ActivateWindow(mw); return 1; }
    if (!(scr = LockPubScreen(NULL))) return 0;
    vi = GetVisualInfo(scr, TAG_DONE);
    fh = scr->Font->ta_YSize;
    top = scr->WBorTop + fh + 1 + 4;
    x0 = scr->WBorLeft + 6;
    rh = fh + 3;
    g = CreateContext(&glist);
    memset(&ng, 0, sizeof ng);
    ng.ng_TextAttr = scr->Font;
    ng.ng_VisualInfo = vi;
    y = top + fh + 4;
    for (c = 0; c < 16; c++, y += rh + 1) {
        sprintf(nums[c], "%2d", c + 1);
        ng.ng_TopEdge = y; ng.ng_Height = rh; ng.ng_Flags = 0; ng.ng_GadgetText = NULL;
        ng.ng_LeftEdge = x0; ng.ng_Width = 22; ng.ng_GadgetID = 0xFFFF;
        g = CreateGadget(TEXT_KIND, g, &ng, GTTX_Text, (ULONG)nums[c], TAG_DONE);
        ng.ng_LeftEdge = x0 + 24; ng.ng_Width = 104; ng.ng_GadgetID = c * 16 + C_NAME;
        g = g_[c][C_NAME] = CreateGadget(TEXT_KIND, g, &ng, GTTX_Text, (ULONG)"", GTTX_Border, TRUE, TAG_DONE);
        for (k = C_PROG; k <= C_LSB; k++) {
            ng.ng_LeftEdge = x0 + 132 + (k - C_PROG) * 36; ng.ng_Width = 34; ng.ng_GadgetID = c * 16 + k;
            g = g_[c][k] = CreateGadget(INTEGER_KIND, g, &ng, GTIN_Number, 0, GTIN_MaxChars, 3, TAG_DONE);
        }
        for (k = C_VOL; k <= C_CHO; k++) {
            ng.ng_LeftEdge = x0 + 240 + (k - C_VOL) * 76; ng.ng_Width = 72; ng.ng_GadgetID = c * 16 + k;
            g = g_[c][k] = CreateGadget(SLIDER_KIND, g, &ng, GTSL_Min, k == C_PAN ? -64 : 0, GTSL_Max, k == C_PAN ? 63 : 127, GTSL_Level, 0, GA_RelVerify, TRUE, TAG_DONE);
        }
        for (k = C_MUTE; k <= C_SOLO; k++) {
            ng.ng_LeftEdge = x0 + 544 + (k - C_MUTE) * 26; ng.ng_Width = 26; ng.ng_GadgetID = c * 16 + k;
            g = g_[c][k] = CreateGadget(CHECKBOX_KIND, g, &ng, GTCB_Checked, FALSE, GTCB_Scaled, TRUE, TAG_DONE);
        }
    }
    y += 3;
    {
        /* effects: reverb type + return, chorus type + return, the standard */
        const char *const *n;
        ng.ng_TopEdge = y; ng.ng_Height = fh + 4; ng.ng_Flags = PLACETEXT_LEFT;
        fx_types(0, 0, &n);
        ng.ng_LeftEdge = x0 + 56; ng.ng_Width = 120; ng.ng_GadgetText = (UBYTE *)"Reverb"; ng.ng_GadgetID = G_FX + FX_REV;
        g = gfx[FX_REV] = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)n, TAG_DONE);
        ng.ng_LeftEdge = x0 + 180; ng.ng_Width = 60; ng.ng_GadgetText = NULL; ng.ng_GadgetID = G_FX + FX_REVRET;
        g = gfx[FX_REVRET] = CreateGadget(SLIDER_KIND, g, &ng, GTSL_Min, 0, GTSL_Max, 127, GTSL_Level, 64, GA_RelVerify, TRUE, TAG_DONE);
        fx_types(0, 1, &n);
        ng.ng_LeftEdge = x0 + 306; ng.ng_Width = 120; ng.ng_GadgetText = (UBYTE *)"Chorus"; ng.ng_GadgetID = G_FX + FX_CHO;
        g = gfx[FX_CHO] = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)n, TAG_DONE);
        ng.ng_LeftEdge = x0 + 430; ng.ng_Width = 60; ng.ng_GadgetText = NULL; ng.ng_GadgetID = G_FX + FX_CHORET;
        g = gfx[FX_CHORET] = CreateGadget(SLIDER_KIND, g, &ng, GTSL_Min, 0, GTSL_Max, 127, GTSL_Level, 64, GA_RelVerify, TRUE, TAG_DONE);
        ng.ng_LeftEdge = x0 + 500; ng.ng_Width = 30; ng.ng_GadgetID = 0xFFFF;
        g = gstd = CreateGadget(TEXT_KIND, g, &ng, GTTX_Text, (ULONG)"XG", TAG_DONE);
        y += fh + 7;
    }
    ng.ng_TopEdge = y; ng.ng_Height = fh + 6; ng.ng_Flags = PLACETEXT_IN;
    ng.ng_LeftEdge = x0; ng.ng_Width = 180; ng.ng_GadgetText = (UBYTE *)"Save into the song"; ng.ng_GadgetID = G_SAVE;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    ng.ng_LeftEdge = x0 + 190; ng.ng_Width = 200; ng.ng_GadgetText = (UBYTE *)"Undo: the song's values"; ng.ng_GadgetID = G_UNDO;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    y += fh + 10;
    if (!g) {
        FreeGadgets(glist); glist = NULL;
        FreeVisualInfo(vi);
        UnlockPubScreen(NULL, scr);
        return 0;
    }
    mw = OpenWindowTags(NULL,
        WA_Title, (ULONG)"Mixer",
        WA_InnerWidth, 600, WA_InnerHeight, y - scr->WBorTop - fh - 1,
        WA_Left, 20, WA_Top, 12,
        WA_Gadgets, (ULONG)glist,
        WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE,
        WA_PubScreen, (ULONG)scr,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | SLIDERIDCMP | INTEGERIDCMP | CHECKBOXIDCMP | BUTTONIDCMP | CYCLEIDCMP,
        TAG_DONE);
    UnlockPubScreen(NULL, scr);
    if (!mw) {
        FreeGadgets(glist); glist = NULL;
        FreeVisualInfo(vi);
        return 0;
    }
    GT_RefreshWindow(mw, NULL);
    /* the column heads */
    SetAPen(mw->RPort, 1);
    for (k = 0; k < 11; k++) {
        Move(mw->RPort, x0 + hx[k], top + mw->RPort->TxBaseline);
        Text(mw->RPort, (CONST_STRPTR)heads[k], strlen(heads[k]));
    }
    x = 0;
    (void)x;
    mixgt_refresh(a);
    return 1;
}

void mixgt_close(void)
{
    if (!mw) return;
    CloseWindow(mw);
    mw = NULL;
    FreeGadgets(glist);
    glist = NULL;
    FreeVisualInfo(vi);
    vi = NULL;
}

int mixgt_handle(App *a)
{
    struct IntuiMessage *m;
    int open = 1;
    while (mw && (m = GT_GetIMsg(mw->UserPort))) {
        ULONG cls = m->Class;
        UWORD code = m->Code;
        struct Gadget *src = (struct Gadget *)m->IAddress;
        GT_ReplyIMsg(m);
        if (cls == IDCMP_CLOSEWINDOW) { open = 0; break; }
        if (cls == IDCMP_REFRESHWINDOW) { GT_BeginRefresh(mw); GT_EndRefresh(mw, TRUE); continue; }
        if (cls != IDCMP_GADGETUP && cls != IDCMP_MOUSEMOVE) continue;
        if (src->GadgetID == G_SAVE) { app_save(a); mixgt_refresh(a); continue; }
        if (src->GadgetID == G_UNDO) { app_mix_reset(a); mixgt_refresh(a); continue; }
        if (src->GadgetID >= G_FX && src->GadgetID < G_FX + FX_N) {
            int w = src->GadgetID - G_FX;
            if (w < 2 && cls == IDCMP_GADGETUP) app_fx(a, w, code);
            else if (w >= 2) app_fx(a, w, (WORD)code);
            continue;
        }
        {
            int c = src->GadgetID / 16, k = src->GadgetID % 16, v;
            if (c > 15 || k >= C_NAME) continue;
            if (k >= C_MUTE) {
                if (cls != IDCMP_GADGETUP) continue;
                app_mute(a, c, (g_[c][C_MUTE]->Flags & GFLG_SELECTED) != 0, (g_[c][C_SOLO]->Flags & GFLG_SELECTED) != 0);
                continue;
            }
            if (k <= C_LSB) v = (int)((struct StringInfo *)src->SpecialInfo)->LongInt - (k == C_PROG ? 1 : 0);
            else v = (WORD)code + (k == C_PAN ? 64 : 0);
            app_mix(a, c, PARAM[k], v);
            if (k <= C_LSB) GT_SetGadgetAttrs(g_[c][C_NAME], mw, NULL, GTTX_Text, (ULONG)sound(a, c), TAG_DONE);
        }
    }
    if (!open) mixgt_close();
    return open;
}
