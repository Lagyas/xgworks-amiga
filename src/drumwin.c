/* XGWorks Lite: the Drum editor - one row per drum (GM names), one column
 * per grid step; a click puts a hit there or takes it away (new hits get
 * the velocity Vel; louder hits are drawn bigger). A click on a name plays
 * the drum. Rows: the drums the track uses, or all GM drums. */
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
#include "drumwin.h"
#include "drumops.h"
#include "rollops.h"
#include "edwin.h"

extern struct Library *GadToolsBase;

enum { D_GRID = 1, D_CH, D_VEL, D_ROWS, D_HOUT, D_HIN, D_UNDO, D_HPROP, D_VPROP };

static struct Window *dw;
static struct Gadget *dgl, *hprop, *vprop;
static void *dvi;
static int dtrk = -1, dch = 9, dgrid = 2, dvel = 100, dall;
static uint32_t htop, spx = 12;        /* tick at the left, pixels per step */
static int rtop;                        /* first row shown */
static int keys[128], nkeys;
static WORD tb, nx0, gx0, gx1, gy0, gy1, rh;
static UWORD pen_bg = 0, pen_tx = 1, pen_sh = 2, pen_fill = 3;
static int play_key = -1;

static const char *const DGRIDS[] = {"1/4", "1/8", "1/16", "1/32", "1/8 T", "1/16 T", NULL};
static const char *const DCHS[] = {"All", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", NULL};
static const char *const DROWS[] = {"Used", "All GM", NULL};

int dw_is_open(void) { return dw != NULL; }
unsigned long dw_signal(void) { return dw ? 1UL << dw->UserPort->mp_SigBit : 0; }

static uint32_t step_ticks(App *a)
{
    uint32_t q = a->ed.ppqn;
    switch (dgrid) {
    case 0: return q;
    case 1: return q / 2;
    case 2: return q / 4;
    case 3: return q / 8;
    case 4: return q / 3;
    default: return q / 6;
    }
}

static void make_rows(App *a)
{
    uint32_t used[128];
    int k;
    dr_keys_used(&a->ed, (uint32_t)dtrk, dch, used);
    nkeys = 0;
    /* high drums at the top, as on a drum machine: cymbals over toms over the bass drum */
    for (k = 127; k >= 0; k--)
        if (used[k] || (dall && dr_gm_names[k])) keys[nkeys++] = k;
}

static void layout(void)
{
    rh = (WORD)(dw->RPort->TxHeight + 3);
    nx0 = dw->BorderLeft;
    gx0 = (WORD)(nx0 + 132);
    gx1 = (WORD)(dw->Width - dw->BorderRight - 1);
    gy0 = (WORD)(tb + dw->RPort->TxHeight + 3);
    gy1 = (WORD)(dw->Height - dw->BorderBottom - 1);
}

/* beat (1-) and tick in the beat of `tick`, from the time signatures */
static void beat_of(uint32_t tick, const uint32_t *tk, const uint32_t *num, const uint32_t *bl, int ns, uint32_t *beat, uint32_t *t)
{
    int k = ns - 1;
    uint32_t rem;
    while (k > 0 && tk[k] > tick) k--;
    rem = tick - tk[k];
    *beat = rem % (num[k] * bl[k]) / bl[k] + 1;
    *t = rem % bl[k];
}

static void draw(App *a)
{
    struct RastPort *rp;
    static uint8_t cell[64][160];
    int rowof[128];
    uint32_t st, t0, endt, i, tk[64], num[64], bl[64];
    int r, c, ns, rows, cols;
    char txt[40];
    if (!dw) return;
    rp = dw->RPort;
    layout();
    make_rows(a);
    st = step_ticks(a);
    if (!st) st = 1;
    t0 = htop - htop % st;
    cols = (gx1 - gx0 + 1) / (WORD)spx;
    if (cols > 160) cols = 160;
    rows = (gy1 - gy0 + 1) / rh;
    if (rows > 64) rows = 64;
    endt = t0 + (uint32_t)cols * st;
    ns = ed_sigs(&a->ed, tk, num, bl, 64);
    /* the hits on the screen: one pass over the track */
    memset(cell, 0, sizeof cell);
    for (r = 0; r < 128; r++) rowof[r] = -1;
    for (r = 0; r < rows && r + rtop < nkeys; r++) rowof[keys[r + rtop]] = r;
    for (i = 0; i < a->ed.trk[dtrk].n; i++) {
        const EdEv *e = &a->ed.trk[dtrk].ev[i];
        if (e->tick >= endt) break;
        if (e->tick < t0 || !ro_is_note(e, dch) || rowof[e->d1] < 0) continue;
        c = (int)((e->tick - t0) / st);
        if (e->d2 > cell[rowof[e->d1]][c]) cell[rowof[e->d1]][c] = e->d2;
    }
    SetDrMd(rp, JAM1);
    SetAPen(rp, pen_bg);
    RectFill(rp, nx0, tb, gx1, gy1);
    /* bar numbers */
    for (c = 0; c < cols; c++) {
        uint32_t t = t0 + (uint32_t)c * st, beat, tt, bar;
        beat_of(t, tk, num, bl, ns, &beat, &tt);
        if (beat == 1 && tt == 0 && gx0 + c * (WORD)spx + 20 < gx1) {
            ed_mbt(&a->ed, t, &bar, &beat, &tt);
            sprintf(txt, "%lu", (unsigned long)bar);
            SetAPen(rp, pen_tx);
            Move(rp, (WORD)(gx0 + c * (WORD)spx + 2), (WORD)(tb + rp->TxBaseline));
            Text(rp, (CONST_STRPTR)txt, strlen(txt));
        }
    }
    for (r = 0; r < rows && r + rtop < nkeys; r++) {
        int key = keys[r + rtop];
        WORD y = (WORD)(gy0 + r * rh);
        const char *nm = dr_gm_names[key] ? dr_gm_names[key] : "";
        sprintf(txt, "%3d %-18.18s", key, nm);
        SetAPen(rp, pen_tx);
        Move(rp, (WORD)(nx0 + 2), (WORD)(y + 1 + rp->TxBaseline));
        Text(rp, (CONST_STRPTR)txt, strlen(txt) > 17 ? 17 : strlen(txt));
        for (c = 0; c < cols; c++) {
            uint32_t beat, tt;
            WORD x = (WORD)(gx0 + c * (WORD)spx);
            beat_of(t0 + (uint32_t)c * st, tk, num, bl, ns, &beat, &tt);
            SetAPen(rp, tt == 0 && beat == 1 ? pen_fill : pen_sh);
            RectFill(rp, x, y, (WORD)(x + spx - 2), (WORD)(y + rh - 2));
            if (cell[r][c]) {
                int v = cell[r][c], m = v >= 96 ? 1 : v >= 64 ? 2 : 3;
                SetAPen(rp, pen_tx);
                RectFill(rp, (WORD)(x + m), (WORD)(y + m), (WORD)(x + spx - 2 - m), (WORD)(y + rh - 2 - m));
            }
        }
    }
    if (nkeys == 0) {
        static const char none[] = "No drums on this track / channel - choose All GM";
        SetAPen(rp, pen_tx);
        Move(rp, gx0, (WORD)(gy0 + rp->TxBaseline));
        Text(rp, (CONST_STRPTR)none, sizeof none - 1);
    }
    {
        uint32_t songend = 0, tt;
        for (tt = 0; tt < a->ed.ntrk; tt++)
            if (a->ed.trk[tt].n) {
                const EdEv *l = &a->ed.trk[tt].ev[a->ed.trk[tt].n - 1];
                if (l->tick + l->dur > songend) songend = l->tick + l->dur;
            }
        SetGadgetAttrs(hprop, dw, NULL, PGA_Total, songend / st + (uint32_t)cols, PGA_Visible, cols, PGA_Top, htop / st, TAG_DONE);
        SetGadgetAttrs(vprop, dw, NULL, PGA_Total, nkeys ? nkeys : 1, PGA_Visible, rows, PGA_Top, rtop, TAG_DONE);
    }
}

static void title(App *a)
{
    static char t[100];
    uint32_t len;
    const uint8_t *nm = ed_name(&a->ed, (uint32_t)dtrk, &len);
    char n[44] = "";
    if (nm) { uint32_t l = len > 40 ? 40 : len; memcpy(n, nm, l); n[l] = 0; }
    snprintf(t, sizeof t, "Drums - %d %s", dtrk + 1, n);
    SetWindowTitles(dw, (CONST_STRPTR)t, (CONST_STRPTR)~0);
}

static int drum_track(App *a)
{
    uint32_t t, i;
    int sel = edw_selected_track();
    if (sel >= 0 && (uint32_t)sel < a->ed.ntrk) return sel;
    /* the track with channel 10 notes */
    for (t = 0; t < a->ed.ntrk; t++)
        for (i = 0; i < a->ed.trk[t].n; i++)
            if (ro_is_note(&a->ed.trk[t].ev[i], 9)) return (int)t;
    return 0;
}

int dw_open(App *a)
{
    struct Screen *scr;
    struct NewGadget ng;
    struct Gadget *g;
    struct DrawInfo *dri;
    WORD fh, x;
    uint32_t i;
    if (a->cur < 0) { snprintf(a->info, sizeof a->info, "Load a song first."); return 0; }
    dtrk = drum_track(a);
    /* channel 10 when the track has it, else the track's channel */
    dch = -1;
    for (i = 0; i < a->ed.trk[dtrk].n; i++)
        if (ro_is_note(&a->ed.trk[dtrk].ev[i], 9)) { dch = 9; break; }
    if (dw) { WindowToFront(dw); ActivateWindow(dw); draw(a); title(a); return 1; }
    if (!(scr = LockPubScreen(NULL))) return 0;
    if ((dri = GetScreenDrawInfo(scr))) {
        pen_bg = dri->dri_Pens[BACKGROUNDPEN]; pen_tx = dri->dri_Pens[SHADOWPEN];
        pen_sh = dri->dri_Pens[SHINEPEN]; pen_fill = dri->dri_Pens[FILLPEN];
        FreeScreenDrawInfo(scr, dri);
    }
    dvi = GetVisualInfo(scr, TAG_DONE);
    fh = scr->Font->ta_YSize;
    tb = (WORD)(scr->WBorTop + fh + 1 + 2);
    memset(&ng, 0, sizeof ng);
    ng.ng_TextAttr = scr->Font;
    ng.ng_VisualInfo = dvi;
    g = CreateContext(&dgl);
    ng.ng_TopEdge = tb; ng.ng_Height = fh + 4; ng.ng_Flags = PLACETEXT_LEFT;
    x = (WORD)(scr->WBorLeft + 40);
    ng.ng_LeftEdge = x; ng.ng_Width = 70; ng.ng_GadgetText = (UBYTE *)"Grid"; ng.ng_GadgetID = D_GRID;
    g = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)DGRIDS, GTCY_Active, dgrid, TAG_DONE);
    x += 70 + 28;
    ng.ng_LeftEdge = x; ng.ng_Width = 54; ng.ng_GadgetText = (UBYTE *)"Ch"; ng.ng_GadgetID = D_CH;
    g = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)DCHS, GTCY_Active, dch + 1, TAG_DONE);
    x += 54 + 34;
    ng.ng_LeftEdge = x; ng.ng_Width = 40; ng.ng_GadgetText = (UBYTE *)"Vel"; ng.ng_GadgetID = D_VEL;
    g = CreateGadget(INTEGER_KIND, g, &ng, GTIN_Number, dvel, GTIN_MaxChars, 3, TAG_DONE);
    x += 40 + 40;
    ng.ng_LeftEdge = x; ng.ng_Width = 80; ng.ng_GadgetText = (UBYTE *)"Rows"; ng.ng_GadgetID = D_ROWS;
    g = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)DROWS, GTCY_Active, dall, TAG_DONE);
    x += 84;
    ng.ng_Flags = PLACETEXT_IN;
    ng.ng_LeftEdge = x; ng.ng_Width = 26; ng.ng_GadgetText = (UBYTE *)"-"; ng.ng_GadgetID = D_HOUT;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    x += 28;
    ng.ng_LeftEdge = x; ng.ng_GadgetText = (UBYTE *)"+"; ng.ng_GadgetID = D_HIN;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    x += 32;
    ng.ng_LeftEdge = x; ng.ng_Width = 50; ng.ng_GadgetText = (UBYTE *)"Undo"; ng.ng_GadgetID = D_UNDO;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    tb = (WORD)(tb + fh + 7);
    if (!g) { FreeGadgets(dgl); dgl = NULL; FreeVisualInfo(dvi); UnlockPubScreen(NULL, scr); return 0; }
    dw = OpenWindowTags(NULL, WA_Title, (ULONG)"Drums", WA_Left, 20, WA_Top, scr->BarHeight + 1,
        WA_Width, scr->Width > 700 ? 660 : scr->Width, WA_Height, scr->Height - scr->BarHeight - 1 > 380 ? 380 : scr->Height - scr->BarHeight - 1,
        WA_MinWidth, 560, WA_MinHeight, 120, WA_MaxWidth, ~0, WA_MaxHeight, ~0,
        WA_Gadgets, (ULONG)dgl, WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_SizeGadget, TRUE,
        WA_SizeBBottom, TRUE, WA_SizeBRight, TRUE, WA_Activate, TRUE, WA_SimpleRefresh, TRUE, WA_PubScreen, (ULONG)scr,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_NEWSIZE | IDCMP_REFRESHWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_VANILLAKEY |
                  IDCMP_IDCMPUPDATE | IDCMP_GADGETUP | CYCLEIDCMP | INTEGERIDCMP | BUTTONIDCMP, TAG_DONE);
    UnlockPubScreen(NULL, scr);
    if (!dw) { FreeGadgets(dgl); dgl = NULL; FreeVisualInfo(dvi); return 0; }
    hprop = (struct Gadget *)NewObject(NULL, (CONST_STRPTR)"propgclass",
        GA_ID, D_HPROP, GA_Left, dw->BorderLeft, GA_RelBottom, -(dw->BorderBottom - 3), GA_RelWidth, -(dw->BorderLeft + dw->BorderRight + 2),
        GA_Height, dw->BorderBottom - 4, GA_BottomBorder, TRUE, PGA_Freedom, FREEHORIZ, PGA_NewLook, TRUE, PGA_Borderless, TRUE,
        PGA_Total, 100, PGA_Visible, 10, PGA_Top, 0, ICA_TARGET, ICTARGET_IDCMP, TAG_DONE);
    vprop = (struct Gadget *)NewObject(NULL, (CONST_STRPTR)"propgclass",
        GA_ID, D_VPROP, GA_RelRight, -(dw->BorderRight - 5), GA_Top, dw->BorderTop + 1, GA_Width, dw->BorderRight - 8,
        GA_RelHeight, -(dw->BorderTop + dw->BorderBottom + 2), GA_RightBorder, TRUE, PGA_Freedom, FREEVERT, PGA_NewLook, TRUE,
        PGA_Borderless, TRUE, PGA_Total, 10, PGA_Visible, 10, PGA_Top, 0, ICA_TARGET, ICTARGET_IDCMP, TAG_DONE);
    if (hprop) AddGadget(dw, hprop, (UWORD)~0);
    if (vprop) AddGadget(dw, vprop, (UWORD)~0);
    RefreshGList(hprop ? hprop : vprop, dw, NULL, -1);
    GT_RefreshWindow(dw, NULL);
    htop = 0;
    rtop = 0;
    draw(a);
    title(a);
    return 1;
}

void dw_close(void)
{
    if (!dw) return;
    if (hprop) RemoveGadget(dw, hprop);
    if (vprop) RemoveGadget(dw, vprop);
    CloseWindow(dw);
    dw = NULL;
    if (hprop) DisposeObject(hprop);
    if (vprop) DisposeObject(vprop);
    hprop = vprop = NULL;
    FreeGadgets(dgl); dgl = NULL;
    FreeVisualInfo(dvi); dvi = NULL;
}

void dw_refresh(App *a)
{
    if (!dw) return;
    if (a->cur < 0 || (uint32_t)dtrk >= a->ed.ntrk) { dw_close(); return; }
    draw(a);
    title(a);
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

int dw_handle(App *a)
{
    struct IntuiMessage *m;
    int changed = 0, close = 0;
    while (dw && (m = GT_GetIMsg(dw->UserPort))) {
        ULONG cls = m->Class, gid = 0, top = 0;
        UWORD code = m->Code;
        WORD mx = m->MouseX, my = m->MouseY;
        struct Gadget *src = (struct Gadget *)m->IAddress;
        if (cls == IDCMP_IDCMPUPDATE) {
            gid = tag_of((struct TagItem *)m->IAddress, GA_ID, 0);
            top = tag_of((struct TagItem *)m->IAddress, PGA_Top, 0);
        }
        GT_ReplyIMsg(m);
        if ((uint32_t)dtrk >= a->ed.ntrk) { close = 1; break; }
        switch (cls) {
        case IDCMP_CLOSEWINDOW: close = 1; break;
        case IDCMP_REFRESHWINDOW: GT_BeginRefresh(dw); GT_EndRefresh(dw, TRUE); draw(a); break;
        case IDCMP_NEWSIZE: draw(a); break;
        case IDCMP_IDCMPUPDATE:
            if (gid == D_HPROP) htop = top * step_ticks(a);
            else if (gid == D_VPROP) rtop = (int)top;
            draw(a);
            break;
        case IDCMP_GADGETUP:
            switch (src->GadgetID) {
            case D_GRID: dgrid = code; htop = 0; break;
            case D_CH: dch = (int)code - 1; break;
            case D_VEL: dvel = (int)((struct StringInfo *)src->SpecialInfo)->LongInt; break;
            case D_ROWS: dall = code; rtop = 0; break;
            case D_HOUT: if (spx > 4) spx -= 2; break;
            case D_HIN: if (spx < 40) spx += 2; break;
            case D_UNDO: app_undo(a); changed = 1; break;
            }
            draw(a);
            break;
        case IDCMP_MOUSEBUTTONS:
            if (code == SELECTDOWN && my >= gy0 && my <= gy1) {
                int r = (my - gy0) / rh + rtop;
                if (r >= nkeys) break;
                if (mx >= nx0 && mx < gx0) {
                    /* the name: hear the drum */
                    play_key = keys[r];
                    app_note(a, dch < 0 ? 9 : dch, play_key, dvel);
                } else if (mx >= gx0 && mx <= gx1) {
                    uint32_t st = step_ticks(a), t = (htop - htop % st) + (uint32_t)((mx - gx0) / (WORD)spx) * st;
                    int res;
                    app_ed_begin(a);
                    res = dr_toggle(&a->ed, (uint32_t)dtrk, dch, keys[r], t, st, dvel);
                    if (res > 0) { play_key = keys[r]; app_note(a, dch < 0 ? 9 : dch, play_key, dvel); }
                    app_ed_commit(a);
                    changed = 1;
                    draw(a);
                }
            } else if (code == SELECTUP && play_key >= 0) {
                app_note(a, dch < 0 ? 9 : dch, play_key, 0);
                play_key = -1;
            }
            break;
        case IDCMP_VANILLAKEY:
            if (code == 27) close = 1;
            break;
        }
    }
    if (close) dw_close();
    return changed;
}
