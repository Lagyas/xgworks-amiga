/* XGWorks Lite: the Track View - the tracks one per row (number, name,
 * channel, M / S) and their blocks along the bars, as in XGworks: click a
 * block to select it (Shift adds), drag it to move it by bars (its events go
 * with it), double click opens its Piano Roll. Create a block from bar to
 * bar on the selected track, Repeat the selected blocks, Rename, Delete,
 * Copy / Cut / Paste (at a bar of the selected track). M / S in the track
 * column mute / solo. Undo-able. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/icclass.h>
#include <libraries/gadtools.h>
#include <graphics/text.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "trackview.h"
#include "blocks.h"
#include "edwin.h"
#include "pianoroll.h"
#include "textmap.h"

extern struct Library *GadToolsBase;

enum { V_FROM = 1, V_TO, V_CREATE, V_COUNT, V_REPEAT, V_NAME, V_COPY, V_CUT, V_PASTE, V_DEL, V_OUT, V_IN, V_UNDO, V_HPROP, V_VPROP, V_INFO };

static struct Window *vw;
static struct Gadget *vgl, *hprop, *vprop, *g_from, *g_to, *g_count, *g_name, *g_info;
static void *vvi;
static WORD tb, rh, cx0, gx0, gx1, gy0, gy1;
static UWORD pen_bg = 0, pen_tx = 1, pen_sh = 2, pen_fill = 3;
static uint32_t htop;          /* tick at the left */
static int bpx = 24;           /* pixels per bar */
static int ttop;               /* first track shown */
static int vtrk = -1;          /* the selected track */
static struct { int trk, bi; } sel[64];
static int nsel;
static BkClip clip[64];
static int nclip;
static int drag, drag_trk, drag_bi;
static WORD dx0, dx1;
static ULONG cs, cm;
static char vinfo[120];

int tv_is_open(void) { return vw != NULL; }
unsigned long tv_signal(void) { return vw ? 1UL << vw->UserPort->mp_SigBit : 0; }

static uint32_t bar_ticks(App *a)
{
    uint32_t tk[64], num[64], bl[64];
    ed_sigs(&a->ed, tk, num, bl, 64);
    return num[0] * bl[0];
}

static WORD xt(App *a, uint32_t t) { return (WORD)(gx0 + ((long)t - (long)htop) * bpx / (long)bar_ticks(a)); }
static uint32_t tx(App *a, WORD x) { long t = (long)htop + (long)(x - gx0) * (long)bar_ticks(a) / bpx; return t < 0 ? 0 : (uint32_t)t; }

static void info(const char *t)
{
    snprintf(vinfo, sizeof vinfo, "%s", t);
    if (vw) GT_SetGadgetAttrs(g_info, vw, NULL, GTTX_Text, (ULONG)vinfo, TAG_DONE);
}

static int is_sel(int t, int b)
{
    int k;
    for (k = 0; k < nsel; k++)
        if (sel[k].trk == t && sel[k].bi == b) return 1;
    return 0;
}

static void layout(void)
{
    rh = (WORD)(vw->RPort->TxHeight + 5);
    cx0 = vw->BorderLeft;
    gx0 = (WORD)(cx0 + 150);
    gx1 = (WORD)(vw->Width - vw->BorderRight - 1);
    gy0 = (WORD)(tb + vw->RPort->TxHeight + 4);
    gy1 = (WORD)(vw->Height - vw->BorderBottom - 1);
}

static void draw(App *a)
{
    struct RastPort *rp;
    uint32_t bar = bar_ticks(a), t, b0, endt;
    int r;
    char txt[80];
    if (!vw) return;
    rp = vw->RPort;
    layout();
    SetDrMd(rp, JAM1);
    SetAPen(rp, pen_bg);
    RectFill(rp, cx0, tb, gx1, gy1);
    endt = tx(a, gx1);
    /* bar numbers */
    for (b0 = htop / bar; b0 * bar <= endt; b0++) {
        WORD x = xt(a, b0 * bar);
        if (x < gx0) continue;
        SetAPen(rp, pen_tx);
        if (bpx >= 16 || b0 % 4 == 0) {
            sprintf(txt, "%lu", (unsigned long)b0 + 1);
            Move(rp, (WORD)(x + 2), (WORD)(tb + rp->TxBaseline));
            Text(rp, (CONST_STRPTR)txt, strlen(txt));
        }
    }
    for (r = 0; r + ttop < (int)a->ed.ntrk; r++) {
        int ti = r + ttop, nb, k, ch;
        WORD y = (WORD)(gy0 + r * rh);
        EdBlock blks[256];
        char nm[44] = "";
        uint32_t len;
        const uint8_t *n;
        if (y + rh > gy1) break;
        /* the track column */
        if (ti == vtrk) { SetAPen(rp, pen_fill); RectFill(rp, cx0, y, (WORD)(gx0 - 2), (WORD)(y + rh - 2)); }
        n = ed_name(&a->ed, (uint32_t)ti, &len);
        if (n) tm_convert(n, len > 40 ? 40 : len, nm, sizeof nm, a->letters);
        ch = ed_channel(&a->ed, (uint32_t)ti);
        snprintf(txt, sizeof txt, "%2d %-11.11s %2s %c%c", ti + 1, nm, ch < 0 ? "-" : "", a->ed.trk[ti].mute ? 'M' : '.', a->ed.trk[ti].solo ? 'S' : '.');
        if (ch >= 0) { char c2[4]; sprintf(c2, "%2d", ch + 1); memcpy(txt + 15, c2, 2); }
        SetAPen(rp, ti == vtrk ? pen_sh : pen_tx);
        Move(rp, (WORD)(cx0 + 2), (WORD)(y + 2 + rp->TxBaseline));
        Text(rp, (CONST_STRPTR)txt, strlen(txt));
        /* bar lines of the row */
        SetAPen(rp, pen_sh);
        for (b0 = htop / bar; b0 * bar <= endt; b0++) {
            WORD x = xt(a, b0 * bar);
            if (x >= gx0) { Move(rp, x, y); Draw(rp, x, (WORD)(y + rh - 2)); }
        }
        /* the blocks */
        nb = bk_view(&a->ed, (uint32_t)ti, bar, blks, 256);
        for (k = 0; k < nb; k++) {
            WORD x0 = xt(a, blks[k].start), x1 = (WORD)(xt(a, blks[k].end > blks[k].start ? blks[k].end : blks[k].start + bar) - 1);
            int s = is_sel(ti, k);
            if (x1 < gx0 || x0 > gx1) continue;
            if (x0 < gx0) x0 = gx0;
            if (x1 > gx1) x1 = gx1;
            SetAPen(rp, s ? pen_tx : pen_fill);
            RectFill(rp, x0, (WORD)(y + 1), x1, (WORD)(y + rh - 3));
            SetAPen(rp, pen_tx);
            Move(rp, x0, (WORD)(y + 1)); Draw(rp, x1, (WORD)(y + 1)); Draw(rp, x1, (WORD)(y + rh - 3)); Draw(rp, x0, (WORD)(y + rh - 3)); Draw(rp, x0, (WORD)(y + 1));
            if (blks[k].name[0] && x1 - x0 > 16) {
                char bn[44];
                int maxc = (x1 - x0 - 4) / (rp->TxWidth ? rp->TxWidth : 8);
                tm_convert((const uint8_t *)blks[k].name, strlen(blks[k].name), bn, sizeof bn, a->letters);
                if ((int)strlen(bn) > maxc) bn[maxc > 0 ? maxc : 0] = 0;
                SetAPen(rp, s ? pen_sh : pen_sh);
                Move(rp, (WORD)(x0 + 2), (WORD)(y + 2 + rp->TxBaseline));
                Text(rp, (CONST_STRPTR)bn, strlen(bn));
            }
        }
    }
    {
        uint32_t songend = 0, tt;
        for (tt = 0; tt < a->ed.ntrk; tt++)
            if (a->ed.trk[tt].n) {
                const EdEv *l = &a->ed.trk[tt].ev[a->ed.trk[tt].n - 1];
                if (l->tick + l->dur > songend) songend = l->tick + l->dur;
            }
        t = songend / bar + 8;
        SetGadgetAttrs(hprop, vw, NULL, PGA_Total, t, PGA_Visible, (gx1 - gx0) / bpx, PGA_Top, htop / bar, TAG_DONE);
        SetGadgetAttrs(vprop, vw, NULL, PGA_Total, a->ed.ntrk ? a->ed.ntrk : 1, PGA_Visible, (gy1 - gy0) / rh, PGA_Top, ttop, TAG_DONE);
    }
}

static void drag_xor(App *a)
{
    struct RastPort *rp = vw->RPort;
    uint32_t bar = bar_ticks(a);
    long d = ((long)tx(a, dx1) - (long)tx(a, dx0)) / (long)bar * (long)bar;
    int k;
    SetDrMd(rp, COMPLEMENT);
    for (k = 0; k < nsel; k++) {
        EdBlock blks[256];
        int nb = bk_view(&a->ed, (uint32_t)sel[k].trk, bar, blks, 256), r = sel[k].trk - ttop;
        WORD y = (WORD)(gy0 + r * rh), x0, x1;
        long st;
        if (sel[k].bi >= nb || r < 0 || y + rh > gy1) continue;
        st = (long)blks[sel[k].bi].start + d;
        if (st < 0) st = 0;
        x0 = xt(a, (uint32_t)st);
        x1 = xt(a, (uint32_t)st + (blks[sel[k].bi].end - blks[sel[k].bi].start));
        if (x0 < gx0) x0 = gx0;
        if (x1 > gx1) x1 = gx1;
        Move(rp, x0, y); Draw(rp, x1, y); Draw(rp, x1, (WORD)(y + rh - 2)); Draw(rp, x0, (WORD)(y + rh - 2)); Draw(rp, x0, y);
    }
    SetDrMd(rp, JAM1);
}

int tv_open(App *a)
{
    struct Screen *scr;
    struct NewGadget ng;
    struct Gadget *g;
    struct DrawInfo *dri;
    WORD fh, x, y;
    if (a->cur < 0) { snprintf(a->info, sizeof a->info, "Load a song first."); return 0; }
    if (vw) { WindowToFront(vw); ActivateWindow(vw); draw(a); return 1; }
    if (!(scr = LockPubScreen(NULL))) return 0;
    if ((dri = GetScreenDrawInfo(scr))) {
        pen_bg = dri->dri_Pens[BACKGROUNDPEN]; pen_tx = dri->dri_Pens[SHADOWPEN];
        pen_sh = dri->dri_Pens[SHINEPEN]; pen_fill = dri->dri_Pens[FILLPEN];
        FreeScreenDrawInfo(scr, dri);
    }
    vvi = GetVisualInfo(scr, TAG_DONE);
    fh = scr->Font->ta_YSize;
    y = (WORD)(scr->WBorTop + fh + 1 + 2);
    memset(&ng, 0, sizeof ng);
    ng.ng_TextAttr = scr->Font;
    ng.ng_VisualInfo = vvi;
    g = CreateContext(&vgl);
    ng.ng_TopEdge = y; ng.ng_Height = fh + 4; ng.ng_Flags = PLACETEXT_LEFT;
    x = (WORD)(scr->WBorLeft + 40);
    ng.ng_LeftEdge = x; ng.ng_Width = 44; ng.ng_GadgetText = (UBYTE *)"Bar"; ng.ng_GadgetID = V_FROM;
    g = g_from = CreateGadget(INTEGER_KIND, g, &ng, GTIN_Number, 1, GTIN_MaxChars, 4, TAG_DONE);
    x += 44 + 26;
    ng.ng_LeftEdge = x; ng.ng_GadgetText = (UBYTE *)"to"; ng.ng_GadgetID = V_TO;
    g = g_to = CreateGadget(INTEGER_KIND, g, &ng, GTIN_Number, 5, GTIN_MaxChars, 4, TAG_DONE);
    x += 48;
    ng.ng_Flags = PLACETEXT_IN; ng.ng_LeftEdge = x; ng.ng_Width = 60; ng.ng_GadgetText = (UBYTE *)"Create"; ng.ng_GadgetID = V_CREATE;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    x += 64 + 18;
    ng.ng_Flags = PLACETEXT_LEFT; ng.ng_LeftEdge = x; ng.ng_Width = 34; ng.ng_GadgetText = (UBYTE *)"x"; ng.ng_GadgetID = V_COUNT;
    g = g_count = CreateGadget(INTEGER_KIND, g, &ng, GTIN_Number, 1, GTIN_MaxChars, 3, TAG_DONE);
    x += 38;
    ng.ng_Flags = PLACETEXT_IN; ng.ng_LeftEdge = x; ng.ng_Width = 60; ng.ng_GadgetText = (UBYTE *)"Repeat"; ng.ng_GadgetID = V_REPEAT;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    x += 64 + 44;
    ng.ng_Flags = PLACETEXT_LEFT; ng.ng_LeftEdge = x; ng.ng_Width = 110; ng.ng_GadgetText = (UBYTE *)"Name"; ng.ng_GadgetID = V_NAME;
    g = g_name = CreateGadget(STRING_KIND, g, &ng, GTST_MaxChars, 38, TAG_DONE);
    y += fh + 7;
    {
        static const char *const bt[7] = {"Copy", "Cut", "Paste", "Delete", "-", "+", "Undo"};
        static const int ids[7] = {V_COPY, V_CUT, V_PASTE, V_DEL, V_OUT, V_IN, V_UNDO};
        static const WORD bw[7] = {50, 44, 54, 60, 24, 24, 50};
        int k;
        x = (WORD)(scr->WBorLeft + 4);
        ng.ng_Flags = PLACETEXT_IN; ng.ng_TopEdge = y;
        for (k = 0; k < 7; k++) {
            ng.ng_LeftEdge = x; ng.ng_Width = bw[k]; ng.ng_GadgetText = (UBYTE *)bt[k]; ng.ng_GadgetID = ids[k];
            g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
            x += bw[k] + 4;
        }
        ng.ng_LeftEdge = x + 4; ng.ng_Width = 240; ng.ng_GadgetText = NULL; ng.ng_GadgetID = V_INFO;
        g = g_info = CreateGadget(TEXT_KIND, g, &ng, GTTX_Text, (ULONG)"Paste goes to the selected track at Bar.", GTTX_Border, TRUE, TAG_DONE);
    }
    tb = (WORD)(y + fh + 7);
    if (!g) { FreeGadgets(vgl); vgl = NULL; FreeVisualInfo(vvi); UnlockPubScreen(NULL, scr); return 0; }
    vw = OpenWindowTags(NULL, WA_Title, (ULONG)"Track View", WA_Left, 0, WA_Top, scr->BarHeight + 1,
        WA_Width, scr->Width > 700 ? 680 : scr->Width, WA_Height, scr->Height - scr->BarHeight - 1 > 360 ? 360 : scr->Height - scr->BarHeight - 1,
        WA_MinWidth, 600, WA_MinHeight, 140, WA_MaxWidth, ~0, WA_MaxHeight, ~0,
        WA_Gadgets, (ULONG)vgl, WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_SizeGadget, TRUE,
        WA_SizeBBottom, TRUE, WA_SizeBRight, TRUE, WA_Activate, TRUE, WA_SimpleRefresh, TRUE, WA_PubScreen, (ULONG)scr,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_NEWSIZE | IDCMP_REFRESHWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_RAWKEY |
                  IDCMP_IDCMPUPDATE | IDCMP_GADGETUP | INTEGERIDCMP | STRINGIDCMP | BUTTONIDCMP, TAG_DONE);
    UnlockPubScreen(NULL, scr);
    if (!vw) { FreeGadgets(vgl); vgl = NULL; FreeVisualInfo(vvi); return 0; }
    hprop = (struct Gadget *)NewObject(NULL, (CONST_STRPTR)"propgclass",
        GA_ID, V_HPROP, GA_Left, vw->BorderLeft, GA_RelBottom, -(vw->BorderBottom - 3), GA_RelWidth, -(vw->BorderLeft + vw->BorderRight + 2),
        GA_Height, vw->BorderBottom - 4, GA_BottomBorder, TRUE, PGA_Freedom, FREEHORIZ, PGA_NewLook, TRUE, PGA_Borderless, TRUE,
        PGA_Total, 100, PGA_Visible, 10, PGA_Top, 0, ICA_TARGET, ICTARGET_IDCMP, TAG_DONE);
    vprop = (struct Gadget *)NewObject(NULL, (CONST_STRPTR)"propgclass",
        GA_ID, V_VPROP, GA_RelRight, -(vw->BorderRight - 5), GA_Top, vw->BorderTop + 1, GA_Width, vw->BorderRight - 8,
        GA_RelHeight, -(vw->BorderTop + vw->BorderBottom + 2), GA_RightBorder, TRUE, PGA_Freedom, FREEVERT, PGA_NewLook, TRUE,
        PGA_Borderless, TRUE, PGA_Total, 10, PGA_Visible, 10, PGA_Top, 0, ICA_TARGET, ICTARGET_IDCMP, TAG_DONE);
    if (hprop) AddGadget(vw, hprop, (UWORD)~0);
    if (vprop) AddGadget(vw, vprop, (UWORD)~0);
    RefreshGList(hprop ? hprop : vprop, vw, NULL, -1);
    GT_RefreshWindow(vw, NULL);
    htop = 0;
    ttop = 0;
    nsel = 0;
    vtrk = edw_selected_track();
    draw(a);
    return 1;
}

void tv_close(void)
{
    int k;
    if (!vw) return;
    if (hprop) RemoveGadget(vw, hprop);
    if (vprop) RemoveGadget(vw, vprop);
    CloseWindow(vw);
    vw = NULL;
    if (hprop) DisposeObject(hprop);
    if (vprop) DisposeObject(vprop);
    hprop = vprop = NULL;
    FreeGadgets(vgl); vgl = NULL;
    FreeVisualInfo(vvi); vvi = NULL;
    for (k = 0; k < nclip; k++) bk_clip_free(&clip[k]);
    nclip = 0;
}

void tv_refresh(App *a)
{
    if (!vw) return;
    if (a->cur < 0) { tv_close(); return; }
    if (vtrk >= (int)a->ed.ntrk) vtrk = -1;
    nsel = 0;
    draw(a);
}

static ULONG tag_of(struct TagItem *t, ULONG tag, ULONG def)
{
    while (t && t->ti_Tag != TAG_DONE) {
        if (t->ti_Tag == TAG_MORE) { t = (struct TagItem *)t->ti_Data; continue; }
        if (t->ti_Tag == tag) return t->ti_Data;
        t++;
    }
    return def;
}

static long num(struct Gadget *g) { return ((struct StringInfo *)g->SpecialInfo)->LongInt; }

/* the selected blocks moved by d ticks: all taken, all removed, all put back (as XGWorks NextGen) */
static void move_sel(App *a, long d)
{
    uint32_t bar = bar_ticks(a);
    static BkClip c[64];
    static uint32_t st[64];
    int i, j, n = 0;
    for (i = 0; i < nsel; i++) bk_materialize(&a->ed, (uint32_t)sel[i].trk, bar);
    for (i = 0; i < nsel; i++)
        for (j = i + 1; j < nsel; j++)
            if (sel[j].trk > sel[i].trk || (sel[j].trk == sel[i].trk && sel[j].bi > sel[i].bi)) { int tt = sel[i].trk, bb = sel[i].bi; sel[i] = sel[j]; sel[j].trk = tt; sel[j].bi = bb; }
    for (i = 0; i < nsel; i++) {
        st[n] = a->ed.trk[sel[i].trk].blk[sel[i].bi].start;
        if (bk_take(&a->ed, (uint32_t)sel[i].trk, (uint32_t)sel[i].bi, st[n], &c[n])) n++;
    }
    for (i = 0; i < nsel; i++) bk_remove(&a->ed, (uint32_t)sel[i].trk, (uint32_t)sel[i].bi);
    for (i = 0; i < n; i++) {
        long to = (long)st[i] + d;
        bk_paste(&a->ed, (uint32_t)c[i].trk, &c[i], (uint32_t)(to < 0 ? 0 : to), bar);
        bk_clip_free(&c[i]);
    }
    nsel = 0;
}

/* the selected blocks onto the clipboard (cut: removed too) */
static int copy_sel(App *a, int cut)
{
    uint32_t bar = bar_ticks(a), t0 = 0xFFFFFFFFUL;
    int k;
    for (k = 0; k < nclip; k++) bk_clip_free(&clip[k]);
    nclip = 0;
    if (!nsel) { info("Select blocks first."); return 0; }
    for (k = 0; k < nsel; k++) {
        bk_materialize(&a->ed, (uint32_t)sel[k].trk, bar);
        if (a->ed.trk[sel[k].trk].blk[sel[k].bi].start < t0) t0 = a->ed.trk[sel[k].trk].blk[sel[k].bi].start;
    }
    for (k = 0; k < nsel && nclip < 64; k++)
        if (bk_take(&a->ed, (uint32_t)sel[k].trk, (uint32_t)sel[k].bi, t0, &clip[nclip])) nclip++;
    if (cut) {
        int i, j;
        /* the highest index first, the others stay right */
        for (i = 0; i < nsel; i++)
            for (j = i + 1; j < nsel; j++)
                if (sel[j].trk > sel[i].trk || (sel[j].trk == sel[i].trk && sel[j].bi > sel[i].bi)) { int tt = sel[i].trk, bb = sel[i].bi; sel[i] = sel[j]; sel[j].trk = tt; sel[j].bi = bb; }
        app_ed_begin(a);
        for (i = 0; i < nsel; i++) bk_remove(&a->ed, (uint32_t)sel[i].trk, (uint32_t)sel[i].bi);
        nsel = 0;
        app_ed_commit(a);
    }
    {
        char m[60];
        snprintf(m, sizeof m, "%d block(s) %s.", nclip, cut ? "cut" : "copied");
        info(m);
    }
    return cut;
}

int tv_handle(App *a)
{
    struct IntuiMessage *m;
    int changed = 0, close = 0;
    while (vw && (m = GT_GetIMsg(vw->UserPort))) {
        ULONG cls = m->Class, gid = 0, top = 0, s = m->Seconds, mi = m->Micros;
        UWORD code = m->Code, qual = m->Qualifier;
        WORD mx = m->MouseX, my = m->MouseY;
        struct Gadget *src = (struct Gadget *)m->IAddress;
        uint32_t bar = bar_ticks(a);
        if (cls == IDCMP_IDCMPUPDATE) {
            gid = tag_of((struct TagItem *)m->IAddress, GA_ID, 0);
            top = tag_of((struct TagItem *)m->IAddress, PGA_Top, 0);
        }
        GT_ReplyIMsg(m);
        switch (cls) {
        case IDCMP_CLOSEWINDOW: close = 1; break;
        case IDCMP_REFRESHWINDOW: GT_BeginRefresh(vw); GT_EndRefresh(vw, TRUE); draw(a); break;
        case IDCMP_NEWSIZE: draw(a); break;
        case IDCMP_IDCMPUPDATE:
            if (gid == V_HPROP) htop = top * bar;
            else if (gid == V_VPROP) ttop = (int)top;
            draw(a);
            break;
        case IDCMP_GADGETUP:
            switch (src->GadgetID) {
            case V_CREATE: {
                long f = num(g_from), t = num(g_to);
                if (vtrk < 0) { info("Select a track first."); break; }
                if (t <= f) { info("The end must be after the start."); break; }
                app_ed_begin(a);
                bk_create(&a->ed, (uint32_t)vtrk, ed_tick_of(&a->ed, (uint32_t)f, 1, 0), ed_tick_of(&a->ed, (uint32_t)t, 1, 0), "", bar);
                app_ed_commit(a);
                info("Block created.");
                changed = 1;
                break;
            }
            case V_REPEAT: {
                long cnt = num(g_count);
                uint32_t f = 0xFFFFFFFFUL, e = 0;
                int k, n;
                if (!nsel) { info("Select blocks first."); break; }
                for (k = 0; k < nsel; k++) {
                    EdBlock blks[256];
                    int nb = bk_view(&a->ed, (uint32_t)sel[k].trk, bar, blks, 256);
                    if (sel[k].bi >= nb) continue;
                    if (blks[sel[k].bi].start < f) f = blks[sel[k].bi].start;
                    if (blks[sel[k].bi].end > e) e = blks[sel[k].bi].end;
                }
                if (e <= f || cnt < 1) break;
                app_ed_begin(a);
                n = bk_repeat(&a->ed, f, e, e, (uint32_t)cnt, bar);
                app_ed_commit(a);
                {
                    char msg[60];
                    snprintf(msg, sizeof msg, "%d block copies made.", n);
                    info(msg);
                }
                nsel = 0;
                changed = 1;
                break;
            }
            case V_NAME:
                if (nsel) {
                    uint8_t enc[40];
                    size_t n = tm_to_cp1250((const char *)((struct StringInfo *)g_name->SpecialInfo)->Buffer, enc, 38, a->letters);
                    int k;
                    enc[n] = 0;
                    app_ed_begin(a);
                    for (k = 0; k < nsel; k++) {
                        bk_materialize(&a->ed, (uint32_t)sel[k].trk, bar);
                        bk_rename(&a->ed, (uint32_t)sel[k].trk, (uint32_t)sel[k].bi, (const char *)enc);
                    }
                    app_ed_commit(a);
                    changed = 1;
                }
                break;
            case V_COPY: copy_sel(a, 0); break;
            case V_CUT: changed |= copy_sel(a, 1); break;
            case V_PASTE: {
                int k;
                uint32_t at = ed_tick_of(&a->ed, (uint32_t)num(g_from), 1, 0);
                if (vtrk < 0 || !nclip) { info(nclip ? "Select a track first." : "Nothing copied."); break; }
                app_ed_begin(a);
                /* the blocks keep their track offsets from the first one's track */
                for (k = 0; k < nclip; k++) {
                    int tr = vtrk + (clip[k].trk - clip[0].trk);
                    if (tr >= 0 && (uint32_t)tr < a->ed.ntrk) bk_paste(&a->ed, (uint32_t)tr, &clip[k], at, bar);
                }
                app_ed_commit(a);
                info("Pasted.");
                nsel = 0;
                changed = 1;
                break;
            }
            case V_DEL: {
                int i, j;
                if (!nsel) break;
                for (i = 0; i < nsel; i++)
                    for (j = i + 1; j < nsel; j++)
                        if (sel[j].trk > sel[i].trk || (sel[j].trk == sel[i].trk && sel[j].bi > sel[i].bi)) { int tt = sel[i].trk, bb = sel[i].bi; sel[i] = sel[j]; sel[j].trk = tt; sel[j].bi = bb; }
                app_ed_begin(a);
                for (i = 0; i < nsel; i++) {
                    bk_materialize(&a->ed, (uint32_t)sel[i].trk, bar);
                    bk_remove(&a->ed, (uint32_t)sel[i].trk, (uint32_t)sel[i].bi);
                }
                nsel = 0;
                app_ed_commit(a);
                info("Deleted with their events.");
                changed = 1;
                break;
            }
            case V_OUT: if (bpx > 6) bpx -= 6; break;
            case V_IN: if (bpx < 120) bpx += 6; break;
            case V_UNDO: app_undo(a); nsel = 0; changed = 1; break;
            }
            draw(a);
            break;
        case IDCMP_MOUSEBUTTONS:
            if (code == SELECTDOWN && my >= gy0 && my <= gy1) {
                int r = (my - gy0) / rh + ttop, nb, k, hit = -1;
                EdBlock blks[256];
                uint32_t t;
                if (r >= (int)a->ed.ntrk) break;
                vtrk = r;
                edw_select_track(a, r);
                if (mx < gx0) {
                    /* M / S in the track column */
                    WORD col = (WORD)((mx - cx0) / (vw->RPort->TxWidth ? vw->RPort->TxWidth : 8));
                    if (col == 18) { a->ed.trk[r].mute = (uint8_t)!a->ed.trk[r].mute; app_track_mute(a); }
                    else if (col == 19) { a->ed.trk[r].solo = (uint8_t)!a->ed.trk[r].solo; app_track_mute(a); }
                    draw(a);
                    break;
                }
                t = tx(a, mx);
                nb = bk_view(&a->ed, (uint32_t)r, bar, blks, 256);
                for (k = 0; k < nb; k++)
                    if (t >= blks[k].start && t < (blks[k].end > blks[k].start ? blks[k].end : blks[k].start + bar)) hit = k;
                GT_SetGadgetAttrs(g_from, vw, NULL, GTIN_Number, t / bar + 1, TAG_DONE);
                if (hit < 0) { nsel = 0; draw(a); break; }
                if (is_sel(r, hit) && DoubleClick(cs, cm, s, mi)) {
                    /* its Piano Roll */
                    pr_open(a);
                    break;
                }
                cs = s; cm = mi;
                if (!is_sel(r, hit)) {
                    if (!(qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT))) nsel = 0;
                    if (nsel < 64) { sel[nsel].trk = r; sel[nsel].bi = hit; nsel++; }
                }
                GT_SetGadgetAttrs(g_name, vw, NULL, GTST_String, (ULONG)blks[hit].name, TAG_DONE);
                draw(a);
                drag = 1; drag_trk = r; drag_bi = hit;
                dx0 = dx1 = mx;
                ReportMouse(TRUE, vw);
                drag_xor(a);
            } else if (code == SELECTUP && drag) {
                long d = ((long)tx(a, dx1) - (long)tx(a, dx0)) / (long)bar * (long)bar;
                drag_xor(a);
                drag = 0;
                ReportMouse(FALSE, vw);
                if (d) {
                    app_ed_begin(a);
                    move_sel(a, d);
                    app_ed_commit(a);
                    info("Moved with their events.");
                    changed = 1;
                }
                draw(a);
            }
            break;
        case IDCMP_MOUSEMOVE:
            if (!drag) break;
            drag_xor(a);
            dx1 = mx < gx0 ? gx0 : mx > gx1 ? gx1 : mx;
            drag_xor(a);
            break;
        case IDCMP_RAWKEY:
            if (code == 0x46 && nsel) {
                /* Del: as the Delete button */
                int i, j;
                for (i = 0; i < nsel; i++)
                    for (j = i + 1; j < nsel; j++)
                        if (sel[j].trk > sel[i].trk || (sel[j].trk == sel[i].trk && sel[j].bi > sel[i].bi)) { int tt = sel[i].trk, bb = sel[i].bi; sel[i] = sel[j]; sel[j].trk = tt; sel[j].bi = bb; }
                app_ed_begin(a);
                for (i = 0; i < nsel; i++) {
                    bk_materialize(&a->ed, (uint32_t)sel[i].trk, bar);
                    bk_remove(&a->ed, (uint32_t)sel[i].trk, (uint32_t)sel[i].bi);
                }
                nsel = 0;
                app_ed_commit(a);
                changed = 1;
                draw(a);
            } else if (code == 0x45) close = 1;   /* Esc */
            break;
        }
    }
    if (close) tv_close();
    return changed;
}
