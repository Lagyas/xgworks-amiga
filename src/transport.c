/* XGWorks Lite: the Transport window (see transport.h). The display is
 * drawn with 7-segment digits, the big buttons with their symbols; the
 * locators are integer gadgets (bar) with Set / Go. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <graphics/text.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <stdio.h>
#include <string.h>
#include "transport.h"
#include "screen.h"
#include "pal.h"

extern struct Library *GadToolsBase;

enum { T_LBAR = 1, T_LSET, T_LGO, T_RBAR, T_RSET, T_RGO };
enum { B_TOP, B_BACK, B_FWD, B_STOP, B_PLAY, B_REC, B_LOOP, B_CLICK, B_N };
static const char *const BLABEL[B_N] = {"Top", "-1 bar", "+1 bar", "Stop", "Play", "Rec", "Loop", "Click"};

static struct Window *tw;
static struct Gadget *tgl, *g_lbar, *g_rbar;
static void *tvi;
static WORD dx0, dy0, dw_, dh_, bx0, by0, bw = 46, bh = 40;
static uint32_t shown_tick = 0xFFFFFFFFUL;
static int shown_play = -1, shown_loop = -1;
static UWORD pen_bg = 0, pen_tx = 1, pen_sh = 2;

int tr_is_open(void) { return tw != NULL; }
unsigned long tr_signal(void) { return tw ? 1UL << tw->UserPort->mp_SigBit : 0; }

static uint32_t cur_tick(App *a)
{
    return a->cur >= 0 ? xg_us_tick(&a->song, a->pos_us) : 0;
}

/* ---- 7-segment digits: segments a b c d e f g ---- */
static const UBYTE SEG[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

static void seg_digit(struct RastPort *rp, WORD x, WORD y, WORD w, WORD h, WORD t, int d, UWORD on, UWORD off)
{
    UBYTE m = d >= 0 && d <= 9 ? SEG[d] : 0x40;
    WORD h2 = (WORD)(h / 2);
    struct { int bit; WORD x0, y0, x1, y1; } s[7];
    int k;
    s[0].bit = 0x01; s[0].x0 = x + t; s[0].y0 = y; s[0].x1 = x + w - t - 1; s[0].y1 = y + t - 1;
    s[1].bit = 0x02; s[1].x0 = x + w - t; s[1].y0 = y + t; s[1].x1 = x + w - 1; s[1].y1 = y + h2 - 1;
    s[2].bit = 0x04; s[2].x0 = x + w - t; s[2].y0 = y + h2 + 1; s[2].x1 = x + w - 1; s[2].y1 = y + h - t - 1;
    s[3].bit = 0x08; s[3].x0 = x + t; s[3].y0 = y + h - t; s[3].x1 = x + w - t - 1; s[3].y1 = y + h - 1;
    s[4].bit = 0x10; s[4].x0 = x; s[4].y0 = y + h2 + 1; s[4].x1 = x + t - 1; s[4].y1 = y + h - t - 1;
    s[5].bit = 0x20; s[5].x0 = x; s[5].y0 = y + t; s[5].x1 = x + t - 1; s[5].y1 = y + h2 - 1;
    s[6].bit = 0x40; s[6].x0 = x + t; s[6].y0 = y + h2 - t / 2; s[6].x1 = x + w - t - 1; s[6].y1 = y + h2 - t / 2 + t - 1;
    for (k = 0; k < 7; k++) {
        SetAPen(rp, (m & s[k].bit) ? on : off);
        RectFill(rp, s[k].x0, s[k].y0, s[k].x1, s[k].y1);
    }
}

/* digits, ':' and '.' in 7 segments; returns the x after it */
static WORD seg_text(struct RastPort *rp, WORD x, WORD y, WORD w, WORD h, WORD t, const char *s, UWORD on, UWORD off)
{
    for (; *s; s++) {
        if (*s == ':' || *s == '.') {
            SetAPen(rp, on);
            if (*s == ':') RectFill(rp, (WORD)(x + 1), (WORD)(y + h / 3 - 1), (WORD)(x + t), (WORD)(y + h / 3 + t - 2));
            RectFill(rp, (WORD)(x + 1), (WORD)(y + h - t - (*s == ':' ? h / 3 - 1 : 0)), (WORD)(x + t), (WORD)(y + h - 1 - (*s == ':' ? h / 3 - 1 : 0)));
            x += t + 4;
        } else {
            seg_digit(rp, x, y, w, h, t, *s >= '0' && *s <= '9' ? *s - '0' : -1, on, off);
            x += w + t + 1;
        }
    }
    return x;
}

static void draw_display(App *a)
{
    struct RastPort *rp = tw->RPort;
    uint32_t t = cur_tick(a), bar = 1, beat = 1, tk = 0, us = a->cur >= 0 ? a->pos_us : 0;
    char s[40];
    UWORD green = pal(PAL_K_PERC, pen_sh), dim = pal(PAL_HEAD, pen_bg), black = pal(PAL_BLACK, pen_tx), yellow = pal(PAL_RULER, pen_sh);
    WORD x;
    SetDrMd(rp, JAM1);
    SetAPen(rp, black);
    RectFill(rp, dx0, dy0, (WORD)(dx0 + dw_ - 1), (WORD)(dy0 + dh_ - 1));
    if (a->cur >= 0) ed_mbt(&a->ed, t, &bar, &beat, &tk);
    /* bar : beat : tick, big */
    snprintf(s, sizeof s, "%04lu:%02lu:%03lu", (unsigned long)(bar % 10000), (unsigned long)(beat % 100), (unsigned long)(tk % 1000));
    seg_text(rp, (WORD)(dx0 + 8), (WORD)(dy0 + 6), 13, 26, 3, s, green, dim);
    /* the time */
    snprintf(s, sizeof s, "%02lu:%02lu.%03lu", (unsigned long)(us / 60000000UL % 100), (unsigned long)(us / 1000000UL % 60), (unsigned long)(us / 1000 % 1000));
    x = seg_text(rp, (WORD)(dx0 + 8), (WORD)(dy0 + 40), 8, 15, 2, s, green, dim);
    (void)x;
    /* tempo, signature, state */
    {
        uint32_t bpm10 = 1200, num = 4, den = 4;
        if (a->cur >= 0) {
            uint32_t tks[64], nums[64], bls[64], q = a->ed.ppqn ? a->ed.ppqn : 480;
            uint32_t usq = xg_tick_us(&a->song, t + q) - xg_tick_us(&a->song, t);
            int n = ed_sigs(&a->ed, tks, nums, bls, 64), k = n - 1;
            if (usq) bpm10 = (uint32_t)(600000000UL / usq);
            bpm10 = bpm10 * (uint32_t)a->pl.tempo / 100;
            while (k > 0 && tks[k] > t) k--;
            if (n) { num = nums[k]; den = bls[k] ? q * 4 / bls[k] : 4; }
        }
        snprintf(s, sizeof s, "%lu.%lu BPM  %lu/%lu", (unsigned long)(bpm10 / 10), (unsigned long)(bpm10 % 10), (unsigned long)num, (unsigned long)den);
        SetAPen(rp, yellow);
        Move(rp, (WORD)(dx0 + 8), (WORD)(dy0 + dh_ - 6 - rp->TxHeight + rp->TxBaseline));
        Text(rp, (CONST_STRPTR)s, strlen(s));
        snprintf(s, sizeof s, "%s%s", app_playing(a) ? "PLAY" : "STOP", a->loop_on ? " LOOP" : "");
        SetAPen(rp, green);
        Move(rp, (WORD)(dx0 + dw_ - 8 - TextLength(rp, (CONST_STRPTR)s, strlen(s))), (WORD)(dy0 + dh_ - 6 - rp->TxHeight + rp->TxBaseline));
        Text(rp, (CONST_STRPTR)s, strlen(s));
    }
}

/* a filled triangle made of lines (no area buffers): dir 1 = right, -1 = left */
static void tri(struct RastPort *rp, WORD cx, WORD cy, WORD hw, WORD hh, int dir)
{
    WORD i;
    for (i = 0; i <= hw * 2; i++) {
        WORD x = (WORD)(cx - dir * hw + dir * i), half = (WORD)(hh - (hh * i) / (hw * 2 + 1));
        Move(rp, x, (WORD)(cy - half)); Draw(rp, x, (WORD)(cy + half));
    }
}

static void draw_button(App *a, int b, int pressed)
{
    struct RastPort *rp = tw->RPort;
    WORD x = (WORD)(bx0 + b * (bw + 4)), y = by0, cx = (WORD)(x + bw / 2), cy = (WORD)(y + 14);
    int on = (b == B_PLAY && app_playing(a)) || (b == B_STOP && !app_playing(a)) || (b == B_LOOP && a->loop_on);
    int en = b != B_REC && b != B_CLICK;
    UWORD face = on ? (b == B_PLAY ? pal(PAL_K_PERC, pen_sh) : pal(PAL_BLOCK, pen_sh)) : pal(PAL_LIGHT, pen_bg);
    UWORD fg = !en ? pal(PAL_EDGE, pen_sh) : on ? pal(PAL_WHITE, pen_sh) : b == B_PLAY ? pal(PAL_K_PERC, pen_tx) : pal(PAL_BLACK, pen_tx);
    const char *l = BLABEL[b];
    SetDrMd(rp, JAM1);
    SetAPen(rp, face);
    RectFill(rp, x, y, (WORD)(x + bw - 1), (WORD)(y + bh - 1));
    /* the 3D edge */
    SetAPen(rp, pressed ? pal(PAL_BLACK, pen_tx) : pal(PAL_WHITE, pen_sh));
    Move(rp, x, (WORD)(y + bh - 1)); Draw(rp, x, y); Draw(rp, (WORD)(x + bw - 1), y);
    SetAPen(rp, pressed ? pal(PAL_WHITE, pen_sh) : pal(PAL_EDGE, pen_tx));
    Draw(rp, (WORD)(x + bw - 1), (WORD)(y + bh - 1)); Draw(rp, x, (WORD)(y + bh - 1));
    SetAPen(rp, b == B_REC && en ? pal(PAL_RED, fg) : fg);
    switch (b) {
    case B_TOP: RectFill(rp, (WORD)(cx - 8), (WORD)(cy - 7), (WORD)(cx - 6), (WORD)(cy + 7)); tri(rp, (WORD)(cx + 1), cy, 6, 7, -1); break;
    case B_BACK: tri(rp, (WORD)(cx - 4), cy, 5, 7, -1); tri(rp, (WORD)(cx + 6), cy, 5, 7, -1); break;
    case B_FWD: tri(rp, (WORD)(cx - 6), cy, 5, 7, 1); tri(rp, (WORD)(cx + 4), cy, 5, 7, 1); break;
    case B_STOP: RectFill(rp, (WORD)(cx - 7), (WORD)(cy - 7), (WORD)(cx + 7), (WORD)(cy + 7)); break;
    case B_PLAY: tri(rp, cx, cy, 7, 8, 1); break;
    case B_REC: {
        WORD r;
        for (r = 0; r <= 7; r++) DrawEllipse(rp, cx, cy, r, r);
        break;
    }
    case B_LOOP:
        Move(rp, (WORD)(cx - 9), (WORD)(cy + 3)); Draw(rp, (WORD)(cx - 9), (WORD)(cy - 5)); Draw(rp, (WORD)(cx + 9), (WORD)(cy - 5));
        Draw(rp, (WORD)(cx + 9), (WORD)(cy + 5)); Draw(rp, (WORD)(cx - 3), (WORD)(cy + 5));
        Move(rp, (WORD)(cx - 8), (WORD)(cy + 3)); Draw(rp, (WORD)(cx - 8), (WORD)(cy - 4)); Draw(rp, (WORD)(cx + 8), (WORD)(cy - 4)); Draw(rp, (WORD)(cx + 8), (WORD)(cy + 4)); Draw(rp, (WORD)(cx - 3), (WORD)(cy + 4));
        tri(rp, (WORD)(cx - 5), (WORD)(cy + 5), 3, 4, -1);
        break;
    case B_CLICK:
        Move(rp, (WORD)(cx - 7), (WORD)(cy + 7)); Draw(rp, (WORD)(cx - 2), (WORD)(cy - 8)); Draw(rp, (WORD)(cx + 2), (WORD)(cy - 8)); Draw(rp, (WORD)(cx + 7), (WORD)(cy + 7)); Draw(rp, (WORD)(cx - 7), (WORD)(cy + 7));
        Move(rp, cx, (WORD)(cy + 3)); Draw(rp, (WORD)(cx + 6), (WORD)(cy - 7));
        break;
    }
    SetAPen(rp, fg);
    Move(rp, (WORD)(cx - TextLength(rp, (CONST_STRPTR)l, strlen(l)) / 2), (WORD)(y + bh - 4 - rp->TxHeight + rp->TxBaseline));
    Text(rp, (CONST_STRPTR)l, strlen(l));
    if (pressed < 0) {}
}

static void draw_all(App *a)
{
    int b;
    if (!tw) return;
    draw_display(a);
    for (b = 0; b < B_N; b++) draw_button(a, b, 0);
    shown_play = app_playing(a);
    shown_loop = a->loop_on;
}

static void show_locators(App *a)
{
    uint32_t bar, beat, tk;
    if (!tw) return;
    ed_mbt(&a->ed, a->loc_l, &bar, &beat, &tk);
    GT_SetGadgetAttrs(g_lbar, tw, NULL, GTIN_Number, a->cur >= 0 ? bar : 1, TAG_DONE);
    ed_mbt(&a->ed, a->loc_r, &bar, &beat, &tk);
    GT_SetGadgetAttrs(g_rbar, tw, NULL, GTIN_Number, a->cur >= 0 ? bar : 5, TAG_DONE);
}

int tr_open(App *a)
{
    struct Screen *scr;
    struct NewGadget ng;
    struct Gadget *g;
    struct DrawInfo *dri;
    WORD fh, x, y, ih;
    if (tw) { WindowToFront(tw); draw_all(a); return 1; }
    if (!(scr = LockPubScreen((CONST_STRPTR)scr_pubname()))) return 0;
    if ((dri = GetScreenDrawInfo(scr))) {
        pen_bg = dri->dri_Pens[BACKGROUNDPEN]; pen_tx = dri->dri_Pens[SHADOWPEN]; pen_sh = dri->dri_Pens[SHINEPEN];
        FreeScreenDrawInfo(scr, dri);
    }
    pal_open(scr);
    tvi = GetVisualInfo(scr, TAG_DONE);
    fh = scr->Font->ta_YSize;
    dx0 = (WORD)(scr->WBorLeft + 6);
    dy0 = (WORD)(scr->WBorTop + fh + 1 + 4);
    dw_ = 196;
    dh_ = (WORD)(62 + fh + 6);
    bx0 = (WORD)(dx0 + dw_ + 8);
    by0 = dy0;
    bh = (WORD)(28 + fh);
    memset(&ng, 0, sizeof ng);
    ng.ng_TextAttr = scr->Font;
    ng.ng_VisualInfo = tvi;
    g = CreateContext(&tgl);
    /* the locators under the buttons */
    y = (WORD)(by0 + bh + 6);
    ih = (WORD)(fh + 6);
    x = (WORD)(bx0 + 14);
    ng.ng_TopEdge = y; ng.ng_Height = ih; ng.ng_Flags = PLACETEXT_LEFT;
    ng.ng_LeftEdge = x; ng.ng_Width = 48; ng.ng_GadgetText = (UBYTE *)"L"; ng.ng_GadgetID = T_LBAR;
    g = g_lbar = CreateGadget(INTEGER_KIND, g, &ng, GTIN_Number, 1, GTIN_MaxChars, 4, TAG_DONE);
    ng.ng_Flags = PLACETEXT_IN; ng.ng_LeftEdge = x + 52; ng.ng_Width = 40; ng.ng_GadgetText = (UBYTE *)"Set"; ng.ng_GadgetID = T_LSET;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    ng.ng_LeftEdge = x + 96; ng.ng_Width = 36; ng.ng_GadgetText = (UBYTE *)"Go"; ng.ng_GadgetID = T_LGO;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    x = (WORD)(x + 170);
    ng.ng_Flags = PLACETEXT_LEFT; ng.ng_LeftEdge = x; ng.ng_Width = 48; ng.ng_GadgetText = (UBYTE *)"R"; ng.ng_GadgetID = T_RBAR;
    g = g_rbar = CreateGadget(INTEGER_KIND, g, &ng, GTIN_Number, 5, GTIN_MaxChars, 4, TAG_DONE);
    ng.ng_Flags = PLACETEXT_IN; ng.ng_LeftEdge = x + 52; ng.ng_Width = 40; ng.ng_GadgetText = (UBYTE *)"Set"; ng.ng_GadgetID = T_RSET;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    ng.ng_LeftEdge = x + 96; ng.ng_Width = 36; ng.ng_GadgetText = (UBYTE *)"Go"; ng.ng_GadgetID = T_RGO;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    if (y + ih > dy0 + dh_) dh_ = (WORD)(y + ih - dy0);
    if (!g) { FreeGadgets(tgl); tgl = NULL; FreeVisualInfo(tvi); tvi = NULL; UnlockPubScreen(NULL, scr); pal_close(); return 0; }
    {
        WORD iw = (WORD)(bx0 + B_N * (bw + 4) + 2 - scr->WBorLeft), ihh = (WORD)(dy0 + dh_ + 4 - scr->WBorTop - fh - 1);
        tw = OpenWindowTags(NULL, WA_Title, (ULONG)"Transport", WA_Left, 0,
            WA_Top, scr->Height - ihh - scr->WBorTop - fh - 1 - scr->WBorBottom - 2,
            WA_InnerWidth, iw, WA_InnerHeight, ihh,
            WA_Gadgets, (ULONG)tgl, WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, FALSE,
            WA_SimpleRefresh, TRUE, WA_PubScreen, (ULONG)scr,
            WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_VANILLAKEY | INTEGERIDCMP | BUTTONIDCMP, TAG_DONE);
    }
    UnlockPubScreen(NULL, scr);
    if (!tw) { FreeGadgets(tgl); tgl = NULL; FreeVisualInfo(tvi); tvi = NULL; pal_close(); return 0; }
    GT_RefreshWindow(tw, NULL);
    shown_tick = 0xFFFFFFFFUL;
    show_locators(a);
    draw_all(a);
    return 1;
}

void tr_close(void)
{
    if (!tw) return;
    CloseWindow(tw);
    tw = NULL;
    FreeGadgets(tgl); tgl = NULL;
    FreeVisualInfo(tvi); tvi = NULL;
    pal_close();
}

void tr_refresh(App *a)
{
    if (!tw) return;
    show_locators(a);
    draw_all(a);
}

void tr_position(App *a, uint32_t pos_us, int playing)
{
    uint32_t t;
    if (a->cur < 0) return;
    t = xg_us_tick(&a->song, pos_us);
    /* the loop: from R back to L */
    if (a->loop_on && playing && a->loc_r > a->loc_l && t >= a->loc_r && a->locate_us < 0)
        a->locate_us = (long)xg_tick_us(&a->song, a->loc_l);
    if (!tw) return;
    if (playing != shown_play || a->loop_on != shown_loop) draw_all(a);
    else if (t != shown_tick) draw_display(a);
    shown_tick = t;
}

static void locate_tick(App *a, uint32_t t)
{
    if (a->cur < 0) return;
    a->locate_us = (long)xg_tick_us(&a->song, t);
    a->pos_us = (uint32_t)a->locate_us;
}

static void button(App *a, int b)
{
    uint32_t t = cur_tick(a), bar, beat, tk;
    if (a->cur >= 0) ed_mbt(&a->ed, t, &bar, &beat, &tk);
    else bar = 1;
    switch (b) {
    case B_TOP: locate_tick(a, 0); break;
    case B_BACK: if (a->cur >= 0) locate_tick(a, ed_tick_of(&a->ed, bar > 1 ? bar - 1 : 1, 1, 0)); break;
    case B_FWD: if (a->cur >= 0) locate_tick(a, ed_tick_of(&a->ed, bar + 1, 1, 0)); break;
    case B_STOP: a->want_cmd = CMD_STOP; break;
    case B_PLAY: a->want_cmd = CMD_PLAY; break;
    case B_LOOP:
        a->loop_on = !a->loop_on;
        if (a->loop_on && a->loc_r <= a->loc_l) snprintf(a->info, sizeof a->info, "Loop: set R after L.");
        break;
    case B_REC: snprintf(a->info, sizeof a->info, "Recording comes with a later version (MIDI input)."); break;
    case B_CLICK: snprintf(a->info, sizeof a->info, "The metronome comes with a later version."); break;
    }
    draw_all(a);
}

int tr_handle(App *a)
{
    struct IntuiMessage *m;
    int close = 0;
    while (tw && (m = GT_GetIMsg(tw->UserPort))) {
        ULONG cls = m->Class;
        UWORD code = m->Code;
        WORD mx = m->MouseX, my = m->MouseY;
        struct Gadget *src = (struct Gadget *)m->IAddress;
        GT_ReplyIMsg(m);
        switch (cls) {
        case IDCMP_CLOSEWINDOW: close = 1; break;
        case IDCMP_REFRESHWINDOW: GT_BeginRefresh(tw); GT_EndRefresh(tw, TRUE); draw_all(a); break;
        case IDCMP_VANILLAKEY:
            if (code == ' ') a->want_cmd = app_playing(a) ? CMD_STOP : CMD_PLAY;
            break;
        case IDCMP_MOUSEBUTTONS:
            if (code == SELECTDOWN && my >= by0 && my < by0 + bh && mx >= bx0) {
                int b = (mx - bx0) / (bw + 4);
                if (b < B_N && mx < bx0 + b * (bw + 4) + bw) {
                    draw_button(a, b, 1);
                    button(a, b);
                }
            }
            break;
        case IDCMP_GADGETUP:
            if (a->cur < 0) break;
            switch (src->GadgetID) {
            case T_LBAR: a->loc_l = ed_tick_of(&a->ed, (uint32_t)((struct StringInfo *)g_lbar->SpecialInfo)->LongInt, 1, 0); break;
            case T_RBAR: a->loc_r = ed_tick_of(&a->ed, (uint32_t)((struct StringInfo *)g_rbar->SpecialInfo)->LongInt, 1, 0); break;
            case T_LSET: a->loc_l = cur_tick(a); show_locators(a); break;
            case T_RSET: a->loc_r = cur_tick(a); show_locators(a); break;
            case T_LGO: locate_tick(a, a->loc_l); break;
            case T_RGO: locate_tick(a, a->loc_r); break;
            }
            draw_all(a);
            break;
        }
    }
    if (close) tr_close();
    return 0;
}
