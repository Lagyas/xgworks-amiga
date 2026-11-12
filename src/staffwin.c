/* XGWorks Lite: the Staff window - the notes of a track as notation (to
 * read; editing is in the Piano Roll / Event List): a grand staff (treble
 * from C4 up, bass below), notes placed by the grid of sixteenths, heads
 * filled / open, stems, flags, dots, sharps, ledger lines, bar lines and
 * numbers. While playing the view follows the song. */
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
#include <string.h>
#include "staffwin.h"
#include "rollops.h"
#include "edwin.h"
#include "screen.h"

extern struct Library *GadToolsBase;

enum { S_CH = 1, S_OUT, S_IN, S_HPROP };

static struct Window *sw;
static struct Gadget *sgl, *hprop;
static void *svi;
static int strk = -1, sch = -1;
static uint32_t stop;            /* tick at the left */
static int qpx = 40;             /* pixels per quarter note */
static WORD tb, ls = 6;          /* toolbar bottom, line spacing */
static UWORD pen_bg = 0, pen_tx = 1;
static uint32_t scursor = 0xFFFFFFFFUL;

static const char *const SCHS[] = {"All", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", NULL};
static const int STEP[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
static const int SHARP[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};

int sw_is_open(void) { return sw != NULL; }
unsigned long sw_signal(void) { return sw ? 1UL << sw->UserPort->mp_SigBit : 0; }

static WORD treble_bottom(void) { return (WORD)(tb + 16 + 8 * ls); }      /* the E4 line */
static WORD bass_bottom(void) { return (WORD)(treble_bottom() + 7 * ls); } /* the G2 line */

static int dia(int k) { return (k / 12) * 7 + STEP[k % 12]; }

/* y of a diatonic step and the staff it is on */
static WORD ydia(int d, int treble)
{
    return treble ? (WORD)(treble_bottom() - (d - 37) * ls / 2) : (WORD)(bass_bottom() - (d - 25) * ls / 2);
}

static WORD xt(uint32_t t, uint32_t ppqn) { return (WORD)(sw->BorderLeft + 40 + ((long)t - (long)stop) * qpx / (long)ppqn); }

static void line(WORD x0, WORD y0, WORD x1, WORD y1)
{
    Move(sw->RPort, x0, y0);
    Draw(sw->RPort, x1, y1);
}

static void head(WORD x, WORD y, int open)
{
    struct RastPort *rp = sw->RPort;
    /* an oval of 7 x 5 */
    if (open) {
        line((WORD)(x - 2), (WORD)(y - 2), (WORD)(x + 2), (WORD)(y - 2));
        line((WORD)(x - 2), (WORD)(y + 2), (WORD)(x + 2), (WORD)(y + 2));
        line((WORD)(x - 3), (WORD)(y - 1), (WORD)(x - 3), (WORD)(y + 1));
        line((WORD)(x + 3), (WORD)(y - 1), (WORD)(x + 3), (WORD)(y + 1));
    } else {
        RectFill(rp, (WORD)(x - 2), (WORD)(y - 2), (WORD)(x + 2), (WORD)(y + 2));
        RectFill(rp, (WORD)(x - 3), (WORD)(y - 1), (WORD)(x + 3), (WORD)(y + 1));
    }
}

static uint32_t view_end(App *a)
{
    return stop + (uint32_t)((sw->Width - sw->BorderRight - sw->BorderLeft - 40) * (long)a->ed.ppqn / qpx);
}

/* the position line (XOR: drawn again it is gone) */
static void cursor_xor(App *a)
{
    WORD xc;
    if (scursor == 0xFFFFFFFFUL || scursor < stop || scursor > view_end(a)) return;
    xc = xt(scursor, a->ed.ppqn);
    SetDrMd(sw->RPort, COMPLEMENT);
    line(xc, (WORD)(treble_bottom() - 6 * ls), xc, (WORD)(bass_bottom() + 2 * ls));
    SetDrMd(sw->RPort, JAM1);
}

static void draw(App *a)
{
    struct RastPort *rp;
    WORD x0, x1, y;
    uint32_t i, q, endt, tk[64], num[64], bl[64];
    int k, ns, s;
    char txt[16];
    if (!sw) return;
    rp = sw->RPort;
    x0 = sw->BorderLeft;
    x1 = (WORD)(sw->Width - sw->BorderRight - 1);
    q = a->ed.ppqn;
    SetDrMd(rp, JAM1);
    SetAPen(rp, pen_bg);
    RectFill(rp, x0, tb, x1, (WORD)(sw->Height - sw->BorderBottom - 1));
    SetAPen(rp, pen_tx);
    /* the staves and the clefs */
    for (k = 0; k < 5; k++) {
        line(x0, (WORD)(treble_bottom() - k * ls), x1, (WORD)(treble_bottom() - k * ls));
        line(x0, (WORD)(bass_bottom() - k * ls), x1, (WORD)(bass_bottom() - k * ls));
    }
    Move(rp, (WORD)(x0 + 4), (WORD)(treble_bottom() - 2 * ls + rp->TxBaseline / 2));
    Text(rp, (CONST_STRPTR)"G", 1);
    Move(rp, (WORD)(x0 + 4), (WORD)(bass_bottom() - 2 * ls + rp->TxBaseline / 2));
    Text(rp, (CONST_STRPTR)"F", 1);
    line((WORD)(x0 + 30), (WORD)(treble_bottom() - 4 * ls), (WORD)(x0 + 30), bass_bottom());
    endt = stop + (uint32_t)((x1 - x0 - 40) * (long)q / qpx);
    /* bar lines and numbers */
    ns = ed_sigs(&a->ed, tk, num, bl, 64);
    for (s = 0; s < ns; s++) {
        uint32_t from = tk[s], until = s + 1 < ns ? tk[s + 1] : endt + 1, barlen = num[s] * bl[s], t;
        for (t = from; t < until && t <= endt; t += barlen) {
            uint32_t bar, beat, tt;
            WORD x;
            if (t < stop) continue;
            x = (WORD)(xt(t, q) - 6);
            if (x <= x0 + 34) continue;
            line(x, (WORD)(treble_bottom() - 4 * ls), x, bass_bottom());
            ed_mbt(&a->ed, t, &bar, &beat, &tt);
            sprintf(txt, "%lu", (unsigned long)bar);
            Move(rp, (WORD)(x + 2), (WORD)(tb + 2 + rp->TxBaseline));
            Text(rp, (CONST_STRPTR)txt, strlen(txt));
        }
    }
    /* the notes */
    if ((uint32_t)strk < a->ed.ntrk) {
        const EdTrack *t = &a->ed.trk[strk];
        uint32_t six = q / 4 ? q / 4 : 1;
        for (i = 0; i < t->n; i++) {
            const EdEv *e = &t->ev[i];
            uint32_t st, len;
            int d, treble, up, open, flags, dot;
            WORD x, yh, mid;
            if (!ro_is_note(e, sch)) continue;
            st = (e->tick + six / 2) / six * six;
            if (st < stop) continue;
            if (st > endt) break;
            len = (e->dur + six / 2) / six;
            if (len < 1) len = 1;
            if (len > 16) len = 16;
            dot = len == 3 || len == 6 || len == 12;
            open = len >= 8;
            flags = len <= 1 ? 2 : len <= 3 ? 1 : 0;
            d = dia(e->d1);
            treble = e->d1 >= 60;
            x = xt(st, q);
            if (x > x1 - 8) break;
            yh = ydia(d, treble);
            /* ledger lines */
            if (treble) {
                for (k = 35; k >= d; k -= 2) line((WORD)(x - 5), ydia(k, 1), (WORD)(x + 5), ydia(k, 1));
                for (k = 47; k <= d; k += 2) line((WORD)(x - 5), ydia(k, 1), (WORD)(x + 5), ydia(k, 1));
            } else {
                for (k = 23; k >= d; k -= 2) line((WORD)(x - 5), ydia(k, 0), (WORD)(x + 5), ydia(k, 0));
                for (k = 35; k <= d; k += 2) line((WORD)(x - 5), ydia(k, 0), (WORD)(x + 5), ydia(k, 0));
            }
            head(x, yh, open);
            if (SHARP[e->d1 % 12]) {
                /* a small sharp */
                line((WORD)(x - 9), (WORD)(yh - 3), (WORD)(x - 9), (WORD)(yh + 3));
                line((WORD)(x - 7), (WORD)(yh - 3), (WORD)(x - 7), (WORD)(yh + 3));
                line((WORD)(x - 11), (WORD)(yh - 1), (WORD)(x - 5), (WORD)(yh - 1));
                line((WORD)(x - 11), (WORD)(yh + 1), (WORD)(x - 5), (WORD)(yh + 1));
            }
            if (dot) RectFill(rp, (WORD)(x + 6), (WORD)(yh - 1), (WORD)(x + 7), yh);
            if (len >= 16) continue;            /* a whole note: no stem */
            mid = treble ? ydia(41, 1) : ydia(29, 0);
            up = yh >= mid;
            if (up) {
                line((WORD)(x + 3), yh, (WORD)(x + 3), (WORD)(yh - 7 * ls / 2));
                for (k = 0; k < flags; k++) line((WORD)(x + 3), (WORD)(yh - 7 * ls / 2 + k * 3), (WORD)(x + 7), (WORD)(yh - 7 * ls / 2 + k * 3 + 4));
            } else {
                line((WORD)(x - 3), yh, (WORD)(x - 3), (WORD)(yh + 7 * ls / 2));
                for (k = 0; k < flags; k++) line((WORD)(x - 3), (WORD)(yh + 7 * ls / 2 - k * 3), (WORD)(x + 1), (WORD)(yh + 7 * ls / 2 - k * 3 - 4));
            }
        }
    }
    /* the song position */
    cursor_xor(a);
    {
        uint32_t songend = 0, tt;
        for (tt = 0; tt < a->ed.ntrk; tt++)
            if (a->ed.trk[tt].n) {
                const EdEv *l = &a->ed.trk[tt].ev[a->ed.trk[tt].n - 1];
                if (l->tick + l->dur > songend) songend = l->tick + l->dur;
            }
        SetGadgetAttrs(hprop, sw, NULL, PGA_Total, songend / q + 8, PGA_Visible, (endt - stop) / q, PGA_Top, stop / q, TAG_DONE);
    }
    (void)y;
}

static void title(App *a)
{
    static char t[100];
    uint32_t len;
    const uint8_t *nm = ed_name(&a->ed, (uint32_t)strk, &len);
    char n[44] = "";
    if (nm) { uint32_t l = len > 40 ? 40 : len; memcpy(n, nm, l); n[l] = 0; }
    snprintf(t, sizeof t, "Staff - %d %s", strk + 1, n);
    SetWindowTitles(sw, (CONST_STRPTR)t, (CONST_STRPTR)~0);
}

int sw_open(App *a)
{
    struct Screen *scr;
    struct NewGadget ng;
    struct Gadget *g;
    struct DrawInfo *dri;
    WORD fh;
    uint32_t t, i;
    if (a->cur < 0) { snprintf(a->info, sizeof a->info, "Load a song first."); return 0; }
    strk = edw_selected_track();
    if (strk < 0 || (uint32_t)strk >= a->ed.ntrk) {
        strk = 0;
        for (t = 0; t < a->ed.ntrk && !strk; t++)
            for (i = 0; i < a->ed.trk[t].n; i++)
                if (ro_is_note(&a->ed.trk[t].ev[i], -1) && (a->ed.trk[t].ev[i].st & 0x0F) != 9) { strk = (int)t; break; }
    }
    if (sw) { WindowToFront(sw); ActivateWindow(sw); draw(a); title(a); return 1; }
    if (!(scr = LockPubScreen((CONST_STRPTR)scr_pubname()))) return 0;
    if ((dri = GetScreenDrawInfo(scr))) {
        pen_bg = dri->dri_Pens[SHINEPEN]; pen_tx = dri->dri_Pens[SHADOWPEN];
        FreeScreenDrawInfo(scr, dri);
    }
    svi = GetVisualInfo(scr, TAG_DONE);
    fh = scr->Font->ta_YSize;
    tb = (WORD)(scr->WBorTop + fh + 1 + 2);
    memset(&ng, 0, sizeof ng);
    ng.ng_TextAttr = scr->Font;
    ng.ng_VisualInfo = svi;
    g = CreateContext(&sgl);
    ng.ng_TopEdge = tb; ng.ng_Height = fh + 4; ng.ng_Flags = PLACETEXT_LEFT;
    ng.ng_LeftEdge = (WORD)(scr->WBorLeft + 30); ng.ng_Width = 54; ng.ng_GadgetText = (UBYTE *)"Ch"; ng.ng_GadgetID = S_CH;
    g = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)SCHS, GTCY_Active, sch + 1, TAG_DONE);
    ng.ng_Flags = PLACETEXT_IN;
    ng.ng_LeftEdge = (WORD)(scr->WBorLeft + 92); ng.ng_Width = 26; ng.ng_GadgetText = (UBYTE *)"-"; ng.ng_GadgetID = S_OUT;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    ng.ng_LeftEdge = (WORD)(scr->WBorLeft + 120); ng.ng_GadgetText = (UBYTE *)"+"; ng.ng_GadgetID = S_IN;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    tb = (WORD)(tb + fh + 6);
    if (!g) { FreeGadgets(sgl); sgl = NULL; FreeVisualInfo(svi); UnlockPubScreen(NULL, scr); return 0; }
    sw = OpenWindowTags(NULL, WA_Title, (ULONG)"Staff", WA_Left, 0, WA_Top, 30,
        WA_Width, scr->Width > 700 ? 660 : scr->Width, WA_Height, (WORD)(tb + 16 * 6 + 16 + 30 + 20),
        WA_MinWidth, 300, WA_MinHeight, 120, WA_MaxWidth, ~0, WA_MaxHeight, ~0,
        WA_Gadgets, (ULONG)sgl, WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_SizeGadget, TRUE,
        WA_SizeBBottom, TRUE, WA_Activate, TRUE, WA_SimpleRefresh, TRUE, WA_PubScreen, (ULONG)scr,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_NEWSIZE | IDCMP_REFRESHWINDOW | IDCMP_IDCMPUPDATE | IDCMP_VANILLAKEY | CYCLEIDCMP | BUTTONIDCMP, TAG_DONE);
    UnlockPubScreen(NULL, scr);
    if (!sw) { FreeGadgets(sgl); sgl = NULL; FreeVisualInfo(svi); return 0; }
    hprop = (struct Gadget *)NewObject(NULL, (CONST_STRPTR)"propgclass",
        GA_ID, S_HPROP, GA_Left, sw->BorderLeft, GA_RelBottom, -(sw->BorderBottom - 3), GA_RelWidth, -(sw->BorderLeft + sw->BorderRight + 2),
        GA_Height, sw->BorderBottom - 4, GA_BottomBorder, TRUE, PGA_Freedom, FREEHORIZ, PGA_NewLook, TRUE, PGA_Borderless, TRUE,
        PGA_Total, 100, PGA_Visible, 10, PGA_Top, 0, ICA_TARGET, ICTARGET_IDCMP, TAG_DONE);
    if (hprop) { AddGadget(sw, hprop, (UWORD)~0); RefreshGList(hprop, sw, NULL, 1); }
    GT_RefreshWindow(sw, NULL);
    stop = 0;
    draw(a);
    title(a);
    return 1;
}

void sw_close(void)
{
    if (!sw) return;
    if (hprop) RemoveGadget(sw, hprop);
    CloseWindow(sw);
    sw = NULL;
    if (hprop) DisposeObject(hprop);
    hprop = NULL;
    FreeGadgets(sgl); sgl = NULL;
    FreeVisualInfo(svi); svi = NULL;
}

void sw_refresh(App *a)
{
    if (!sw) return;
    if (a->cur < 0 || (uint32_t)strk >= a->ed.ntrk) { sw_close(); return; }
    draw(a);
    title(a);
}

void sw_position(App *a, uint32_t pos_us, int playing)
{
    uint32_t t, endt;
    if (!sw || a->cur < 0) return;
    t = xg_us_tick(&a->song, pos_us);
    if (t == scursor) return;
    endt = view_end(a);
    if (playing && (t > endt || t < stop)) {
        /* the next page */
        stop = t - t % a->ed.ppqn;
        scursor = t;
        draw(a);
        return;
    }
    cursor_xor(a);
    scursor = t;
    cursor_xor(a);
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

int sw_handle(App *a)
{
    struct IntuiMessage *m;
    int close = 0;
    while (sw && (m = GT_GetIMsg(sw->UserPort))) {
        ULONG cls = m->Class, gid = 0, top = 0;
        UWORD code = m->Code;
        struct Gadget *src = (struct Gadget *)m->IAddress;
        if (cls == IDCMP_IDCMPUPDATE) {
            gid = tag_of((struct TagItem *)m->IAddress, GA_ID, 0);
            top = tag_of((struct TagItem *)m->IAddress, PGA_Top, 0);
        }
        GT_ReplyIMsg(m);
        switch (cls) {
        case IDCMP_CLOSEWINDOW: close = 1; break;
        case IDCMP_REFRESHWINDOW: GT_BeginRefresh(sw); GT_EndRefresh(sw, TRUE); draw(a); break;
        case IDCMP_NEWSIZE: draw(a); break;
        case IDCMP_IDCMPUPDATE: if (gid == S_HPROP) { stop = top * a->ed.ppqn; draw(a); } break;
        case IDCMP_GADGETUP:
            if (src->GadgetID == S_CH) sch = (int)code - 1;
            else if (src->GadgetID == S_OUT && qpx > 12) qpx -= 8;
            else if (src->GadgetID == S_IN && qpx < 120) qpx += 8;
            draw(a);
            break;
        case IDCMP_VANILLAKEY:
            if (code == 27) close = 1;
            break;
        }
    }
    if (close) sw_close();
    return 0;
}
