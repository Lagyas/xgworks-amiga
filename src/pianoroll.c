/* XGWorks Lite: the Piano Roll - the notes of a track on a time / key grid.
 * Tools: Select (click selects, Shift adds, drag moves, drag the right end
 * to change the length, drag on empty space selects a rectangle), Draw (click
 * / drag draws a note), Erase. Keys: Del deletes, cursor up / down moves the
 * selection a half-tone (Shift: an octave), left / right a grid step, A
 * selects all, C copies the selection a bar later, + / - zoom. A click on the
 * keyboard plays the key. Under the notes the velocities. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/icclass.h>
#include <libraries/gadtools.h>
#include <graphics/text.h>
#include <graphics/rastport.h>
#include <utility/tagitem.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <stdio.h>
#include <string.h>
#include "pianoroll.h"
#include "rollops.h"
#include "edwin.h"

extern struct Library *GadToolsBase;

enum { R_TOOL = 1, R_GRID, R_CH, R_VEL, R_HOUT, R_HIN, R_VOUT, R_VIN, R_UNDO, R_HPROP, R_VPROP };
enum { TOOL_SELECT, TOOL_DRAW, TOOL_ERASE };
enum { DRAG_NONE, DRAG_MOVE, DRAG_SIZE, DRAG_RUBBER, DRAG_DRAW, DRAG_KEY };

static struct Window *rw;
static struct Gadget *rgl, *rg_vel, *hprop, *vprop;
static void *rvi;
static int rtrk = -1, rch = -1, tool, grid = 2, vel = 100;
static uint32_t htop;          /* tick at the left edge */
static int vtop = 84;          /* key of the top row */
static int kh = 8;             /* pixels per key */
static uint32_t tpp = 20;      /* ticks per pixel */
static WORD ax0, ay0, ax1, ay1, vy0, vy1, ry0, kx0, tb;
static UWORD pen_bg = 0, pen_tx = 1, pen_sh = 2, pen_fill = 3;
static int drag, drag_key, cur_key = -1;
static WORD dx0, dy0, dx1, dy1;
static long drag_note = -1;
static uint32_t cursor_tick = 0xFFFFFFFFUL;
static uint32_t sig_tk[64], sig_num[64], sig_bl[64];
static int nsig;
static char status[100];

static const char *const TOOLS[] = {"Select", "Draw", "Erase", NULL};
static const char *const GRIDS[] = {"1/4", "1/8", "1/16", "1/32", "1/4 T", "1/8 T", "Off", NULL};
static const char *const CHS[] = {"All", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", NULL};

int pr_is_open(void) { return rw != NULL; }
unsigned long pr_signal(void) { return rw ? 1UL << rw->UserPort->mp_SigBit : 0; }

static uint32_t grid_ticks(App *a)
{
    uint32_t q = a->ed.ppqn;
    switch (grid) {
    case 0: return q;
    case 1: return q / 2;
    case 2: return q / 4;
    case 3: return q / 8;
    case 4: return q * 2 / 3;
    case 5: return q / 3;
    default: return 1;
    }
}

static int is_black(int k) { int n = k % 12; return n == 1 || n == 3 || n == 6 || n == 8 || n == 10; }

static WORD xt(uint32_t t) { return (WORD)(ax0 + ((long)t - (long)htop) / (long)tpp); }
static long tx(WORD x) { long t = (long)htop + (long)(x - ax0) * (long)tpp; return t < 0 ? 0 : t; }
static WORD yk(int k) { return (WORD)(ay0 + (vtop - k) * kh); }
static int ky(WORD y) { return vtop - (y - ay0) / kh; }

static void layout(void)
{
    WORD fh = rw->RPort->TxHeight;
    kx0 = rw->BorderLeft;
    ax0 = (WORD)(kx0 + 36);
    ax1 = (WORD)(rw->Width - rw->BorderRight - 1);
    ry0 = tb;
    ay0 = (WORD)(ry0 + fh + 3);
    vy1 = (WORD)(rw->Height - rw->BorderBottom - 1);
    vy0 = (WORD)(vy1 - 30);
    ay1 = (WORD)(vy0 - 3);
    if (ay1 < ay0 + kh) ay1 = (WORD)(ay0 + kh);
}

static void fill(UWORD pen, WORD x0, WORD y0, WORD x1, WORD y1)
{
    if (x0 < ax0 - 36 + 0) x0 = (WORD)(ax0 - 36);
    if (x1 > ax1) x1 = ax1;
    if (x1 < x0 || y1 < y0) return;
    SetAPen(rw->RPort, pen);
    RectFill(rw->RPort, x0, y0, x1, y1);
}

static void cursor_xor(void)
{
    struct RastPort *rp = rw->RPort;
    WORD x;
    if (cursor_tick == 0xFFFFFFFFUL) return;
    x = xt(cursor_tick);
    if (x < ax0 || x > ax1) return;
    SetDrMd(rp, COMPLEMENT);
    Move(rp, x, ay0);
    Draw(rp, x, vy1);
    SetDrMd(rp, JAM1);
}

static void draw(App *a)
{
    struct RastPort *rp;
    EdTrack *t;
    uint32_t i, beat, end_tick;
    int k, s;
    char txt[16];
    if (!rw) return;
    rp = rw->RPort;
    layout();
    nsig = ed_sigs(&a->ed, sig_tk, sig_num, sig_bl, 64);
    SetDrMd(rp, JAM1);
    /* rows */
    for (k = vtop; yk(k) <= ay1 && k >= 0; k--) {
        WORD y = yk(k), y2 = (WORD)(y + kh - 1);
        if (y2 > ay1) y2 = ay1;
        fill(is_black(k) ? pen_bg : pen_sh, ax0, y, ax1, y2);
        /* the keyboard */
        SetAPen(rp, is_black(k) ? pen_tx : pen_sh);
        RectFill(rp, kx0, y, ax0 - 2, y2);
        if (k % 12 == 0) {
            SetAPen(rp, pen_tx);
            Move(rp, kx0, y2);
            Draw(rp, ax0 - 2, y2);
            if (kh >= rp->TxHeight - 1) {
                sprintf(txt, "C%d", k / 12 - 2);
                SetAPen(rp, pen_tx);
                SetBPen(rp, pen_sh);
                SetDrMd(rp, JAM2);
                Move(rp, kx0 + 2, (WORD)(y2 - rp->TxHeight + 1 + rp->TxBaseline));
                Text(rp, (CONST_STRPTR)txt, strlen(txt));
                SetDrMd(rp, JAM1);
            }
        }
    }
    if (yk(k) <= ay1) fill(pen_bg, ax0, yk(k), ax1, ay1);
    /* ruler and the velocity lane */
    fill(pen_bg, ax0, ry0, ax1, ay0 - 1);
    fill(pen_bg, kx0, ry0, ax0 - 1, ay0 - 1);
    fill(pen_bg, kx0, ay1 + 1, ax1, vy1);
    fill(pen_sh, ax0, vy0, ax1, vy1);
    /* beat and bar lines */
    end_tick = (uint32_t)tx(ax1);
    for (s = 0; s < nsig; s++) {
        uint32_t from = sig_tk[s], until = s + 1 < nsig ? sig_tk[s + 1] : end_tick + 1, bl = sig_bl[s], bars = 0;
        uint32_t bar0 = 1;
        int j;
        for (j = 0; j < s; j++) bar0 += (sig_tk[j + 1] - sig_tk[j] + sig_num[j] * sig_bl[j] - 1) / (sig_num[j] * sig_bl[j]);
        if (until <= htop) continue;
        for (beat = from; beat < until && beat <= end_tick; beat += bl, bars++) {
            WORD x;
            int bar_line = bars % sig_num[s] == 0;
            if (beat < htop) continue;
            x = xt(beat);
            if (bar_line || (long)bl / (long)tpp >= 4) {
                SetAPen(rp, bar_line ? pen_tx : pen_bg);
                Move(rp, x, ay0);
                Draw(rp, x, ay1);
            }
            if (bar_line) {
                sprintf(txt, "%lu", (unsigned long)(bar0 + bars / sig_num[s]));
                if (x + (WORD)TextLength(rp, (CONST_STRPTR)txt, strlen(txt)) < ax1) {
                    SetAPen(rp, pen_tx);
                    Move(rp, (WORD)(x + 2), (WORD)(ry0 + rp->TxBaseline));
                    Text(rp, (CONST_STRPTR)txt, strlen(txt));
                    Move(rp, x, (WORD)(ay0 - 3));
                    Draw(rp, x, (WORD)(ay0 - 1));
                }
            }
        }
    }
    /* the notes and their velocities */
    if (rtrk >= 0 && (uint32_t)rtrk < a->ed.ntrk) {
        t = &a->ed.trk[rtrk];
        for (i = 0; i < t->n; i++) {
            EdEv *e = &t->ev[i];
            WORD x0, x1, y0, y1;
            if (!ro_is_note(e, rch) || e->tick > end_tick || e->tick + e->dur < htop) continue;
            x0 = xt(e->tick);
            x1 = (WORD)(xt(e->tick + e->dur) - 1);
            if (x1 < x0) x1 = x0;
            if (x0 < ax0) x0 = ax0;
            if (x1 > ax1) x1 = ax1;
            /* the velocity */
            if (x0 <= ax1) {
                WORD h = (WORD)(e->d2 * (vy1 - vy0) / 127);
                SetAPen(rp, e->sel ? pen_tx : pen_fill);
                RectFill(rp, x0, (WORD)(vy1 - h), (WORD)(x0 + 1 > ax1 ? ax1 : x0 + 1), vy1);
            }
            if (e->d1 > vtop || yk(e->d1) > ay1) continue;
            y0 = (WORD)(yk(e->d1) + 1);
            y1 = (WORD)(yk(e->d1) + kh - 1);
            if (y1 > ay1) y1 = ay1;
            if (y1 < y0) y1 = y0;
            SetAPen(rp, e->sel ? pen_tx : pen_fill);
            RectFill(rp, x0, y0, x1, y1);
            if (x1 - x0 > 3 && !e->sel) {
                SetAPen(rp, pen_tx);
                Move(rp, x0, y1); Draw(rp, x0, y0); Draw(rp, x1, y0); Draw(rp, x1, y1); Draw(rp, x0, y1);
            }
        }
    }
    cursor_xor();
    /* scrollers */
    {
        uint32_t total = 16, vis = 1, top;
        uint32_t unit = a->ed.ppqn / 4 ? a->ed.ppqn / 4 : 1, songend = 0;
        uint32_t tt;
        for (tt = 0; tt < a->ed.ntrk; tt++)
            if (a->ed.trk[tt].n) {
                const EdEv *l = &a->ed.trk[tt].ev[a->ed.trk[tt].n - 1];
                if (l->tick + l->dur > songend) songend = l->tick + l->dur;
            }
        total = (songend + (uint32_t)(ax1 - ax0) * tpp) / unit + 1;
        if (total > 65000) total = 65000;
        vis = (uint32_t)(ax1 - ax0) * tpp / unit;
        top = htop / unit;
        SetGadgetAttrs(hprop, rw, NULL, PGA_Total, total, PGA_Visible, vis ? vis : 1, PGA_Top, top, TAG_DONE);
        SetGadgetAttrs(vprop, rw, NULL, PGA_Total, 128, PGA_Visible, (ay1 - ay0 + 1) / kh, PGA_Top, 127 - vtop, TAG_DONE);
    }
}

static void title(App *a)
{
    static char tbuf[140];
    uint32_t len;
    const uint8_t *nm = rtrk >= 0 ? ed_name(&a->ed, (uint32_t)rtrk, &len) : NULL;
    char n[44] = "";
    int sel = rtrk >= 0 ? ro_count_sel(&a->ed, (uint32_t)rtrk) : 0;
    if (nm) {
        uint32_t l = len > 40 ? 40 : len;
        memcpy(n, nm, l);
        n[l] = 0;
    }
    snprintf(tbuf, sizeof tbuf, "Piano Roll - %d %s%s%s", rtrk + 1, n, sel ? "  selected: " : "", "");
    if (sel) snprintf(tbuf + strlen(tbuf), sizeof tbuf - strlen(tbuf), "%d", sel);
    if (status[0]) snprintf(tbuf + strlen(tbuf), sizeof tbuf - strlen(tbuf), "  %s", status);
    SetWindowTitles(rw, (CONST_STRPTR)tbuf, (CONST_STRPTR)~0);
}

static int default_track(App *a)
{
    uint32_t t, i;
    int sel = edw_selected_track();
    if (sel >= 0 && (uint32_t)sel < a->ed.ntrk) return sel;
    for (t = 0; t < a->ed.ntrk; t++)
        for (i = 0; i < a->ed.trk[t].n; i++)
            if (ro_is_note(&a->ed.trk[t].ev[i], -1)) return (int)t;
    return 0;
}

int pr_open(App *a)
{
    struct Screen *scr;
    struct NewGadget ng;
    struct Gadget *g;
    struct DrawInfo *dri;
    WORD fh, x;
    if (a->cur < 0) { snprintf(a->info, sizeof a->info, "Load a song first."); return 0; }
    rtrk = default_track(a);
    if (rw) { WindowToFront(rw); ActivateWindow(rw); draw(a); title(a); return 1; }
    if (!(scr = LockPubScreen(NULL))) return 0;
    if ((dri = GetScreenDrawInfo(scr))) {
        pen_bg = dri->dri_Pens[BACKGROUNDPEN]; pen_tx = dri->dri_Pens[SHADOWPEN];
        pen_sh = dri->dri_Pens[SHINEPEN]; pen_fill = dri->dri_Pens[FILLPEN];
        FreeScreenDrawInfo(scr, dri);
    }
    rvi = GetVisualInfo(scr, TAG_DONE);
    fh = scr->Font->ta_YSize;
    tb = (WORD)(scr->WBorTop + fh + 1 + 2);
    memset(&ng, 0, sizeof ng);
    ng.ng_TextAttr = scr->Font;
    ng.ng_VisualInfo = rvi;
    g = CreateContext(&rgl);
    ng.ng_TopEdge = tb; ng.ng_Height = fh + 4;
    x = (WORD)(scr->WBorLeft + 2);
    ng.ng_LeftEdge = x; ng.ng_Width = 76; ng.ng_GadgetID = R_TOOL;
    g = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)TOOLS, GTCY_Active, tool, TAG_DONE);
    x += 76 + 40;
    ng.ng_LeftEdge = x; ng.ng_Width = 66; ng.ng_GadgetText = (UBYTE *)"Grid"; ng.ng_Flags = PLACETEXT_LEFT; ng.ng_GadgetID = R_GRID;
    g = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)GRIDS, GTCY_Active, grid, TAG_DONE);
    x += 66 + 28;
    ng.ng_LeftEdge = x; ng.ng_Width = 54; ng.ng_GadgetText = (UBYTE *)"Ch"; ng.ng_GadgetID = R_CH;
    g = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)CHS, GTCY_Active, rch + 1, TAG_DONE);
    x += 54 + 34;
    ng.ng_LeftEdge = x; ng.ng_Width = 40; ng.ng_GadgetText = (UBYTE *)"Vel"; ng.ng_GadgetID = R_VEL;
    g = rg_vel = CreateGadget(INTEGER_KIND, g, &ng, GTIN_Number, vel, GTIN_MaxChars, 3, TAG_DONE);
    x += 44;
    {
        static const char *const zt[4] = {"H-", "H+", "V-", "V+"};
        int k;
        ng.ng_Flags = PLACETEXT_IN;
        for (k = 0; k < 4; k++) {
            ng.ng_LeftEdge = x; ng.ng_Width = 26; ng.ng_GadgetText = (UBYTE *)zt[k]; ng.ng_GadgetID = R_HOUT + k;
            g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
            x += 28;
        }
        ng.ng_LeftEdge = x + 4; ng.ng_Width = 50; ng.ng_GadgetText = (UBYTE *)"Undo"; ng.ng_GadgetID = R_UNDO;
        g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    }
    tb = (WORD)(tb + fh + 7);
    if (!g) { FreeGadgets(rgl); rgl = NULL; FreeVisualInfo(rvi); UnlockPubScreen(NULL, scr); return 0; }
    rw = OpenWindowTags(NULL, WA_Title, (ULONG)"Piano Roll", WA_Left, 0, WA_Top, scr->BarHeight + 1,
        WA_Width, scr->Width > 700 ? 680 : scr->Width, WA_Height, scr->Height - scr->BarHeight - 1 > 400 ? 400 : scr->Height - scr->BarHeight - 1,
        WA_MinWidth, 560, WA_MinHeight, 140, WA_MaxWidth, ~0, WA_MaxHeight, ~0,
        WA_Gadgets, (ULONG)rgl, WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_SizeGadget, TRUE,
        WA_SizeBBottom, TRUE, WA_SizeBRight, TRUE, WA_Activate, TRUE, WA_SimpleRefresh, TRUE, WA_PubScreen, (ULONG)scr,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_NEWSIZE | IDCMP_REFRESHWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_RAWKEY |
                  IDCMP_VANILLAKEY | IDCMP_IDCMPUPDATE | IDCMP_GADGETUP | CYCLEIDCMP | INTEGERIDCMP | BUTTONIDCMP,
        TAG_DONE);
    UnlockPubScreen(NULL, scr);
    if (!rw) { FreeGadgets(rgl); rgl = NULL; FreeVisualInfo(rvi); return 0; }
    /* the scrollers in the borders */
    hprop = (struct Gadget *)NewObject(NULL, (CONST_STRPTR)"propgclass",
        GA_ID, R_HPROP, GA_Left, rw->BorderLeft, GA_RelBottom, -(rw->BorderBottom - 3), GA_RelWidth, -(rw->BorderLeft + rw->BorderRight + 2),
        GA_Height, rw->BorderBottom - 4, GA_BottomBorder, TRUE, PGA_Freedom, FREEHORIZ, PGA_NewLook, TRUE, PGA_Borderless, TRUE,
        PGA_Total, 100, PGA_Visible, 10, PGA_Top, 0, ICA_TARGET, ICTARGET_IDCMP, TAG_DONE);
    vprop = (struct Gadget *)NewObject(NULL, (CONST_STRPTR)"propgclass",
        GA_ID, R_VPROP, GA_RelRight, -(rw->BorderRight - 5), GA_Top, rw->BorderTop + 1, GA_Width, rw->BorderRight - 8,
        GA_RelHeight, -(rw->BorderTop + rw->BorderBottom + 2), GA_RightBorder, TRUE, PGA_Freedom, FREEVERT, PGA_NewLook, TRUE,
        PGA_Borderless, TRUE, PGA_Total, 128, PGA_Visible, 40, PGA_Top, 43, ICA_TARGET, ICTARGET_IDCMP, TAG_DONE);
    if (hprop) AddGadget(rw, hprop, (UWORD)~0);
    if (vprop) AddGadget(rw, vprop, (UWORD)~0);
    RefreshGList(hprop ? hprop : vprop, rw, NULL, -1);
    GT_RefreshWindow(rw, NULL);
    htop = 0;
    tpp = a->ed.ppqn / 24 ? a->ed.ppqn / 24 : 1;
    cursor_tick = 0xFFFFFFFFUL;
    draw(a);
    title(a);
    return 1;
}

void pr_close(void)
{
    if (!rw) return;
    if (hprop) RemoveGadget(rw, hprop);
    if (vprop) RemoveGadget(rw, vprop);
    CloseWindow(rw);
    rw = NULL;
    if (hprop) DisposeObject(hprop);
    if (vprop) DisposeObject(vprop);
    hprop = vprop = NULL;
    FreeGadgets(rgl); rgl = NULL;
    FreeVisualInfo(rvi); rvi = NULL;
}

void pr_refresh(App *a)
{
    if (!rw) return;
    if (a->cur < 0 || (uint32_t)rtrk >= a->ed.ntrk) { pr_close(); return; }
    cursor_tick = 0xFFFFFFFFUL;
    draw(a);
    title(a);
}

void pr_position(App *a, uint32_t pos_us, int playing)
{
    uint32_t t;
    if (!rw || a->cur < 0 || drag) return;
    t = xg_us_tick(&a->song, pos_us);
    if (t == cursor_tick) return;
    cursor_xor();                     /* off where it was */
    cursor_tick = t;
    if (playing && (xt(t) > ax1 || t < htop)) {
        /* the next page */
        htop = ro_snap(t, a->ed.ppqn);
        cursor_tick = 0xFFFFFFFFUL;
        draw(a);
        cursor_tick = t;
    }
    cursor_xor();
}

/* the outline of the dragged notes (XOR: drawn twice it is gone) */
static void drag_xor(App *a)
{
    struct RastPort *rp = rw->RPort;
    EdTrack *t = &a->ed.trk[rtrk];
    uint32_t i;
    long dt = 0, dd = 0;
    int dk = 0;
    SetDrMd(rp, COMPLEMENT);
    if (drag == DRAG_RUBBER) {
        Move(rp, dx0, dy0); Draw(rp, dx1, dy0); Draw(rp, dx1, dy1); Draw(rp, dx0, dy1); Draw(rp, dx0, dy0);
        SetDrMd(rp, JAM1);
        return;
    }
    if (drag == DRAG_MOVE) { dt = (tx(dx1) - tx(dx0)); dt = dt / (long)grid_ticks(a) * (long)grid_ticks(a); dk = ky(dy1) - ky(dy0); }
    if (drag == DRAG_SIZE) { dd = (tx(dx1) - tx(dx0)); dd = dd / (long)grid_ticks(a) * (long)grid_ticks(a); }
    for (i = 0; i < t->n; i++) {
        const EdEv *e = &t->ev[i];
        long st, en;
        int k;
        WORD x0, x1, y0;
        if (!e->sel || !ro_is_note(e, rch)) continue;
        st = (long)e->tick + dt;
        en = st + (long)e->dur + dd;
        if (en <= st) en = st + 1;
        k = e->d1 + dk;
        x0 = xt((uint32_t)(st < 0 ? 0 : st));
        x1 = (WORD)(xt((uint32_t)en) - 1);
        y0 = yk(k);
        if (x0 < ax0) x0 = ax0;
        if (x1 > ax1) x1 = ax1;
        if (x1 < x0 || y0 < ay0 || y0 + kh - 1 > ay1) continue;
        Move(rp, x0, y0); Draw(rp, x1, y0); Draw(rp, x1, (WORD)(y0 + kh - 1)); Draw(rp, x0, (WORD)(y0 + kh - 1)); Draw(rp, x0, y0);
    }
    SetDrMd(rp, JAM1);
}

static int track_channel(App *a)
{
    int c = rch >= 0 ? rch : ed_channel(&a->ed, (uint32_t)rtrk);
    return c < 0 ? 0 : c;
}

static int commit(App *a, const char *what)
{
    app_ed_commit(a);
    snprintf(status, sizeof status, "%s", what);
    cursor_tick = 0xFFFFFFFFUL;
    draw(a);
    title(a);
    return 1;
}

static void show_vel(App *a, long i)
{
    if (i >= 0) {
        vel = a->ed.trk[rtrk].ev[i].d2;
        GT_SetGadgetAttrs(rg_vel, rw, NULL, GTIN_Number, vel, TAG_DONE);
    }
}

/* the value of a tag in an IDCMPUPDATE list */
static ULONG tag_of(struct TagItem *t, ULONG tag, ULONG def)
{
    while (t && t->ti_Tag != TAG_DONE) {
        if (t->ti_Tag == TAG_MORE) { t = (struct TagItem *)t->ti_Data; continue; }
        if (t->ti_Tag == tag) return t->ti_Data;
        t++;
    }
    return def;
}

int pr_handle(App *a)
{
    struct IntuiMessage *m;
    int changed = 0, close = 0;
    while (rw && (m = GT_GetIMsg(rw->UserPort))) {
        ULONG cls = m->Class, gid = 0, top = 0;
        UWORD code = m->Code, qual = m->Qualifier;
        WORD mx = m->MouseX, my = m->MouseY;
        struct Gadget *src = (struct Gadget *)m->IAddress;
        if (cls == IDCMP_IDCMPUPDATE) {
            gid = tag_of((struct TagItem *)m->IAddress, GA_ID, 0);
            top = tag_of((struct TagItem *)m->IAddress, PGA_Top, 0);
        }
        GT_ReplyIMsg(m);
        if (rtrk < 0 || (uint32_t)rtrk >= a->ed.ntrk) { close = 1; break; }
        switch (cls) {
        case IDCMP_CLOSEWINDOW:
            close = 1;
            break;
        case IDCMP_REFRESHWINDOW:
            GT_BeginRefresh(rw);
            GT_EndRefresh(rw, TRUE);
            cursor_tick = 0xFFFFFFFFUL;
            draw(a);
            break;
        case IDCMP_NEWSIZE:
            cursor_tick = 0xFFFFFFFFUL;
            draw(a);
            break;
        case IDCMP_IDCMPUPDATE:
            if (gid == R_HPROP) htop = top * (a->ed.ppqn / 4 ? a->ed.ppqn / 4 : 1);
            else if (gid == R_VPROP) vtop = 127 - (int)top;
            cursor_tick = 0xFFFFFFFFUL;
            draw(a);
            break;
        case IDCMP_GADGETUP:
            switch (src->GadgetID) {
            case R_TOOL: tool = code; break;
            case R_GRID: grid = code; break;
            case R_CH: rch = (int)code - 1; ro_select_none(&a->ed, (uint32_t)rtrk); draw(a); break;
            case R_VEL:
                vel = (int)((struct StringInfo *)src->SpecialInfo)->LongInt;
                if (vel < 1) vel = 1;
                if (vel > 127) vel = 127;
                if (ro_count_sel(&a->ed, (uint32_t)rtrk)) {
                    app_ed_begin(a);
                    ro_vel_sel(&a->ed, (uint32_t)rtrk, vel);
                    changed |= commit(a, "velocity");
                }
                break;
            case R_HOUT: if (tpp < 4000) tpp = tpp * 2; draw(a); break;
            case R_HIN: if (tpp > 1) tpp = tpp / 2; draw(a); break;
            case R_VOUT: if (kh > 3) kh--; draw(a); break;
            case R_VIN: if (kh < 16) kh++; draw(a); break;
            case R_UNDO: app_undo(a); changed = 1; cursor_tick = 0xFFFFFFFFUL; draw(a); break;
            }
            title(a);
            break;
        case IDCMP_MOUSEBUTTONS:
            if (code == SELECTDOWN) {
                long i;
                int key = ky(my);
                uint32_t t;
                if (mx >= kx0 && mx < ax0 && my >= ay0 && my <= ay1) {
                    /* the keyboard: play the key */
                    cur_key = key;
                    app_note(a, track_channel(a), key, vel);
                    drag = DRAG_KEY;
                    break;
                }
                if (mx < ax0 || mx > ax1 || my < ay0 || my > ay1) break;
                t = (uint32_t)tx(mx);
                i = ro_hit(&a->ed, (uint32_t)rtrk, rch, t, key);
                dx0 = dx1 = mx; dy0 = dy1 = my;
                if (tool == TOOL_ERASE) {
                    if (i >= 0) {
                        app_ed_begin(a);
                        ro_select_none(&a->ed, (uint32_t)rtrk);
                        a->ed.trk[rtrk].ev[i].sel = 1;
                        ro_delete_sel(&a->ed, (uint32_t)rtrk);
                        changed |= commit(a, "erased");
                    }
                    break;
                }
                if (tool == TOOL_DRAW && i < 0) {
                    drag = DRAG_DRAW;
                    drag_key = key;
                    app_note(a, track_channel(a), key, vel);
                    cur_key = key;
                    ReportMouse(TRUE, rw);
                    break;
                }
                if (i >= 0) {
                    EdEv *e = &a->ed.trk[rtrk].ev[i];
                    WORD xe = xt(e->tick + e->dur);
                    if (!e->sel) {
                        if (!(qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT))) ro_select_none(&a->ed, (uint32_t)rtrk);
                        e->sel = 1;
                        draw(a);
                    }
                    show_vel(a, i);
                    drag_note = i;
                    drag = (xe - mx <= 3 && xe - xt(e->tick) > 6) ? DRAG_SIZE : DRAG_MOVE;
                    app_note(a, e->st & 0x0F, e->d1, e->d2);
                    cur_key = e->d1;
                    drag_key = e->st & 0x0F;
                } else {
                    if (!(qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT))) { ro_select_none(&a->ed, (uint32_t)rtrk); draw(a); }
                    drag = DRAG_RUBBER;
                }
                ReportMouse(TRUE, rw);
                drag_xor(a);
                title(a);
            } else if (code == SELECTUP && drag) {
                int d = drag;
                if (d != DRAG_KEY && d != DRAG_DRAW) drag_xor(a);
                drag = DRAG_NONE;
                ReportMouse(FALSE, rw);
                if (cur_key >= 0) {
                    app_note(a, d == DRAG_MOVE || d == DRAG_SIZE ? drag_key : track_channel(a), cur_key, 0);
                    cur_key = -1;
                }
                if (d == DRAG_MOVE) {
                    long dt = (tx(dx1) - tx(dx0)) / (long)grid_ticks(a) * (long)grid_ticks(a);
                    int dk = ky(dy1) - ky(dy0);
                    if (dt || dk) {
                        app_ed_begin(a);
                        ro_move_sel(&a->ed, (uint32_t)rtrk, dt, dk);
                        changed |= commit(a, "moved");
                    }
                } else if (d == DRAG_SIZE) {
                    long dd = (tx(dx1) - tx(dx0)) / (long)grid_ticks(a) * (long)grid_ticks(a);
                    if (dd) {
                        app_ed_begin(a);
                        ro_size_sel(&a->ed, (uint32_t)rtrk, dd, grid_ticks(a) > 1 ? grid_ticks(a) / 2 : 1);
                        changed |= commit(a, "length");
                    }
                } else if (d == DRAG_RUBBER) {
                    ro_select_rect(&a->ed, (uint32_t)rtrk, rch, (uint32_t)tx(dx0 < dx1 ? dx0 : dx1), (uint32_t)tx(dx0 < dx1 ? dx1 : dx0) + 1,
                                   ky(dy0), ky(dy1), (qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0);
                    cursor_tick = 0xFFFFFFFFUL;
                    draw(a);
                } else if (d == DRAG_DRAW) {
                    uint32_t g = grid_ticks(a), t0 = ro_snap((uint32_t)tx(dx0), g), t1 = (uint32_t)tx(dx1);
                    uint32_t dur = t1 > t0 ? ro_snap(t1 - t0 + g - 1, g) : 0;
                    if (dur < g) dur = g > 1 ? g : (uint32_t)a->ed.ppqn / 4;
                    app_ed_begin(a);
                    ro_add_note(&a->ed, (uint32_t)rtrk, track_channel(a), t0, drag_key, vel, dur);
                    changed |= commit(a, "note drawn");
                }
                title(a);
            }
            break;
        case IDCMP_MOUSEMOVE:
            if (!drag || drag == DRAG_KEY) break;
            if (drag == DRAG_DRAW) { dx1 = mx; dy1 = my; break; }
            drag_xor(a);
            if (mx < ax0) mx = ax0;
            if (mx > ax1) mx = ax1;
            if (my < ay0) my = ay0;
            if (my > ay1) my = ay1;
            dx1 = mx; dy1 = my;
            drag_xor(a);
            break;
        case IDCMP_RAWKEY: {
            int shift = (qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0;
            long dt = 0;
            int dk = 0;
            if (code == 0x46 && ro_count_sel(&a->ed, (uint32_t)rtrk)) {          /* Del */
                app_ed_begin(a);
                ro_delete_sel(&a->ed, (uint32_t)rtrk);
                changed |= commit(a, "deleted");
                break;
            }
            if (code == 0x4C) dk = shift ? 12 : 1;
            else if (code == 0x4D) dk = shift ? -12 : -1;
            else if (code == 0x4E) dt = (long)grid_ticks(a);
            else if (code == 0x4F) dt = -(long)grid_ticks(a);
            else break;
            if (ro_count_sel(&a->ed, (uint32_t)rtrk)) {
                app_ed_begin(a);
                ro_move_sel(&a->ed, (uint32_t)rtrk, dt, dk);
                changed |= commit(a, dk ? "transposed" : "moved");
            }
            break;
        }
        case IDCMP_VANILLAKEY:
            if (code == 'a' || code == 'A') { ro_select_all(&a->ed, (uint32_t)rtrk, rch); cursor_tick = 0xFFFFFFFFUL; draw(a); title(a); }
            else if ((code == 'c' || code == 'C') && ro_count_sel(&a->ed, (uint32_t)rtrk)) {
                /* a copy one bar later */
                uint32_t bar = a->ed.ppqn * 4;
                if (nsig > 0) bar = sig_num[0] * sig_bl[0];
                app_ed_begin(a);
                ro_copy_sel(&a->ed, (uint32_t)rtrk, (long)bar);
                changed |= commit(a, "copied a bar later");
            }
            else if (code == '+') { if (tpp > 1) tpp /= 2; draw(a); }
            else if (code == '-') { if (tpp < 4000) tpp *= 2; draw(a); }
            else if (code == 27) close = 1;
            break;
        }
    }
    if (close) pr_close();
    return changed;
}
