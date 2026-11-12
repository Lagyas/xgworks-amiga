/* XGWorks Lite: the Tracks window and the Event List editor (see edwin.h). */
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <graphics/text.h>
#include <graphics/gfxmacros.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "edwin.h"
#include "edtext.h"
#include "textmap.h"
#include "screen.h"

extern struct Library *GadToolsBase;

/* ================= Tracks ================= */

enum { T_LIST, T_NAME, T_CH, T_EVENTS, T_NEW, T_DEL, T_MUTE, T_SOLO, T_UNDO, T_SAVE, T_SAVEAS, T_INFO, T_N };
static struct Window *tw;
static struct Gadget *tgl, *tg[T_N];
static void *tvi;
static struct List tlist;
static struct Node *tnodes;
static char (*tlines)[64];
static int tsel = -1;

/* ================= Event List ================= */

enum { E_SCROLL, E_EDIT, E_APPLY, E_INSERT, E_DELETE, E_UNDO, E_FILTER, E_INFO, E_N };
static const char *const FILTERS[] = {"All", "Notes", "Controls", "Lyrics", "Chords", "Texts", "SysEx", "Tempo", NULL};
static int efilter;
static uint32_t *emap, emapn, emapcap;   /* the rows shown: event indices */
static struct Window *ew;
static struct Gadget *egl, *eg[E_N];
static void *evi;
static struct TextFont *mono;
static int etrk = -1;          /* the track shown */
static long etop, esel = -1;   /* first row shown, selected row */
static int erows;
static WORD ex0, ey0, ewid;
static char eline[220], einfo[120];
static ULONG eclick_s, eclick_m;

static void name_of(App *a, uint32_t t, char *out, int max)
{
    uint32_t len;
    const uint8_t *n = ed_name(&a->ed, t, &len);
    if (n && len) tm_convert(n, len > 40 ? 40 : len, out, (size_t)max, a->letters);
    else snprintf(out, (size_t)max, "Track %lu", (unsigned long)t + 1);
}

/* ---------- Tracks ---------- */

static void tracks_fill(App *a)
{
    uint32_t t, n = a->ed.ntrk;
    if (!tw) return;
    GT_SetGadgetAttrs(tg[T_LIST], tw, NULL, GTLV_Labels, ~0UL, TAG_DONE);
    free(tnodes); free(tlines);
    tnodes = NULL; tlines = NULL;
    NewList(&tlist);
    if (n && (tnodes = (struct Node *)calloc(n, sizeof(struct Node))) && (tlines = (char (*)[64])calloc(n, 64))) {
        for (t = 0; t < n; t++) {
            char nm[44], chb[4];
            int ch = ed_channel(&a->ed, t);
            uint32_t notes = 0, k;
            for (k = 0; k < a->ed.trk[t].n; k++)
                if ((a->ed.trk[t].ev[k].st & 0xF0) == 0x90 && a->ed.trk[t].ev[k].d2) notes++;
            name_of(a, t, nm, sizeof nm);
            if (ch < 0) strcpy(chb, "-"); else sprintf(chb, "%d", ch + 1);
            snprintf(tlines[t], 64, "%2lu %-22.22s %3s %5lu %c%c", (unsigned long)t + 1, nm, chb, (unsigned long)notes,
                     a->ed.trk[t].mute ? 'M' : ' ', a->ed.trk[t].solo ? 'S' : ' ');
            tnodes[t].ln_Name = tlines[t];
            AddTail(&tlist, &tnodes[t]);
        }
    }
    GT_SetGadgetAttrs(tg[T_LIST], tw, NULL, GTLV_Labels, (ULONG)&tlist, GTLV_Selected, tsel < 0 ? ~0UL : (ULONG)tsel, TAG_DONE);
    GT_SetGadgetAttrs(tg[T_INFO], tw, NULL, GTTX_Text, (ULONG)a->info, TAG_DONE);
    if (tsel >= 0 && (uint32_t)tsel < n) {
        static char nm[44];
        name_of(a, (uint32_t)tsel, nm, sizeof nm);
        GT_SetGadgetAttrs(tg[T_NAME], tw, NULL, GTST_String, (ULONG)nm, TAG_DONE);
        GT_SetGadgetAttrs(tg[T_CH], tw, NULL, GTIN_Number, ed_channel(&a->ed, (uint32_t)tsel) + 1, TAG_DONE);
    }
}

int edw_tracks_open(App *a)
{
    struct Screen *scr;
    struct NewGadget ng;
    struct Gadget *g;
    WORD fh, top, x0, y, w = 420;
    if (tw) { WindowToFront(tw); ActivateWindow(tw); return 1; }
    if (a->cur < 0) { snprintf(a->info, sizeof a->info, "Load a song first."); return 0; }
    if (!(scr = LockPubScreen((CONST_STRPTR)scr_pubname()))) return 0;
    tvi = GetVisualInfo(scr, TAG_DONE);
    fh = scr->Font->ta_YSize;
    top = scr->WBorTop + fh + 1 + 4;
    x0 = scr->WBorLeft + 6;
    memset(&ng, 0, sizeof ng);
    ng.ng_TextAttr = scr->Font;
    ng.ng_VisualInfo = tvi;
    g = CreateContext(&tgl);
    NewList(&tlist);
    ng.ng_LeftEdge = x0; ng.ng_TopEdge = top; ng.ng_Width = w; ng.ng_Height = fh * 12 + 4; ng.ng_GadgetID = T_LIST;
    g = tg[T_LIST] = CreateGadget(LISTVIEW_KIND, g, &ng, GTLV_Labels, (ULONG)&tlist, GTLV_ShowSelected, 0, TAG_DONE);
    y = top + fh * 12 + 8;
    ng.ng_TopEdge = y; ng.ng_Height = fh + 6; ng.ng_Flags = PLACETEXT_LEFT;
    ng.ng_LeftEdge = x0 + 48; ng.ng_Width = 220; ng.ng_GadgetText = (UBYTE *)"Name"; ng.ng_GadgetID = T_NAME;
    g = tg[T_NAME] = CreateGadget(STRING_KIND, g, &ng, GTST_MaxChars, 40, TAG_DONE);
    ng.ng_LeftEdge = x0 + 310; ng.ng_Width = 40; ng.ng_GadgetText = (UBYTE *)"Ch"; ng.ng_GadgetID = T_CH;
    g = tg[T_CH] = CreateGadget(INTEGER_KIND, g, &ng, GTIN_MaxChars, 2, GTIN_Number, 1, TAG_DONE);
    ng.ng_LeftEdge = x0 + 356; ng.ng_Width = 32; ng.ng_GadgetText = (UBYTE *)"M"; ng.ng_GadgetID = T_MUTE; ng.ng_Flags = PLACETEXT_IN;
    g = tg[T_MUTE] = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    ng.ng_LeftEdge = x0 + 390; ng.ng_GadgetText = (UBYTE *)"S"; ng.ng_GadgetID = T_SOLO;
    g = tg[T_SOLO] = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    y += fh + 10;
    {
        static const char *const txt[7] = {"Events...", "New", "Delete", "Undo", "Save", "Save as...", NULL};
        static const int ids[6] = {T_EVENTS, T_NEW, T_DEL, T_UNDO, T_SAVE, T_SAVEAS};
        static const WORD bw[6] = {84, 50, 60, 54, 54, 92};
        WORD x = x0;
        int k;
        for (k = 0; k < 6; k++) {
            ng.ng_LeftEdge = x; ng.ng_TopEdge = y; ng.ng_Width = bw[k]; ng.ng_Height = fh + 6;
            ng.ng_GadgetText = (UBYTE *)txt[k]; ng.ng_GadgetID = ids[k];
            g = tg[ids[k]] = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
            x += bw[k] + 4;
        }
    }
    y += fh + 10;
    ng.ng_LeftEdge = x0; ng.ng_TopEdge = y; ng.ng_Width = w; ng.ng_Height = fh + 4; ng.ng_GadgetText = NULL; ng.ng_GadgetID = T_INFO;
    g = tg[T_INFO] = CreateGadget(TEXT_KIND, g, &ng, GTTX_Text, (ULONG)"", GTTX_Border, TRUE, TAG_DONE);
    y += fh + 8;
    if (!g) { FreeGadgets(tgl); tgl = NULL; FreeVisualInfo(tvi); UnlockPubScreen(NULL, scr); return 0; }
    tw = OpenWindowTags(NULL, WA_Title, (ULONG)"Tracks", WA_Left, 60, WA_Top, 20,
        WA_InnerWidth, w + 12, WA_InnerHeight, y - scr->WBorTop - fh - 1,
        WA_Gadgets, (ULONG)tgl, WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE,
        WA_PubScreen, (ULONG)scr,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | LISTVIEWIDCMP | STRINGIDCMP | INTEGERIDCMP | BUTTONIDCMP, TAG_DONE);
    UnlockPubScreen(NULL, scr);
    if (!tw) { FreeGadgets(tgl); tgl = NULL; FreeVisualInfo(tvi); return 0; }
    GT_RefreshWindow(tw, NULL);
    if (tsel < 0 || (uint32_t)tsel >= a->ed.ntrk) tsel = a->ed.ntrk > 1 ? 1 : 0;
    tracks_fill(a);
    return 1;
}

static void tracks_close(void)
{
    if (!tw) return;
    CloseWindow(tw);
    tw = NULL;
    FreeGadgets(tgl); tgl = NULL;
    FreeVisualInfo(tvi); tvi = NULL;
    free(tnodes); free(tlines);
    tnodes = NULL; tlines = NULL;
}

/* ---------- Event List ---------- */

static int shown(const EdSong *s, const EdEv *e)
{
    const uint8_t *d = s->pool + e->data;
    int k = e->st & 0xF0;
    switch (efilter) {
    case 1: return k == 0x80 || k == 0x90 || k == 0xA0;
    case 2: return k >= 0xB0 && k <= 0xE0;
    case 3: return e->st == 0xFF && e->d1 == 0x05;
    case 4: return e->st == 0xFF && e->d1 == 0x7F && e->len == 7 && d[0] == 0x43 && d[1] == 0x7B;
    case 5: return e->st == 0xFF && e->d1 >= 0x01 && e->d1 <= 0x07 && e->d1 != 0x05;
    case 6: return e->st == 0xF0 || e->st == 0xF7;
    case 7: return e->st == 0xFF && (e->d1 == 0x51 || e->d1 == 0x58 || e->d1 == 0x59);
    default: return 1;
    }
}

/* The rows of the filter (made again each time: the events change). */
static uint32_t enum_ev(App *a)
{
    uint32_t i, n;
    emapn = 0;
    if (etrk < 0 || (uint32_t)etrk >= a->ed.ntrk) return 0;
    n = a->ed.trk[etrk].n;
    if (n > emapcap) {
        uint32_t *nm = (uint32_t *)realloc(emap, (n + 64) * sizeof(uint32_t));
        if (!nm) return 0;
        emap = nm;
        emapcap = n + 64;
    }
    for (i = 0; i < n; i++)
        if (shown(&a->ed, &a->ed.trk[etrk].ev[i])) emap[emapn++] = i;
    return emapn;
}

/* The row of event i (or the row after it). */
static long row_of(App *a, long i)
{
    uint32_t r, n = enum_ev(a);
    for (r = 0; r < n; r++)
        if ((long)emap[r] >= i) return (long)r;
    return (long)n - 1;
}

static void ev_draw(App *a)
{
    struct RastPort *rp;
    long i;
    uint32_t n = enum_ev(a);
    struct DrawInfo *dri;
    UWORD tx = 1, bg = 0, fill = 3, ftx = 2;
    if (!ew) return;
    rp = ew->RPort;
    if ((dri = GetScreenDrawInfo(ew->WScreen))) {
        tx = dri->dri_Pens[TEXTPEN]; bg = dri->dri_Pens[BACKGROUNDPEN];
        fill = dri->dri_Pens[FILLPEN]; ftx = dri->dri_Pens[FILLTEXTPEN];
        FreeScreenDrawInfo(ew->WScreen, dri);
    }
    if (mono) SetFont(rp, mono);
    SetDrMd(rp, JAM2);
    for (i = 0; i < erows; i++) {
        long k = etop + i;
        char line[220];
        WORD y = (WORD)(ey0 + i * rp->TxHeight);
        int len;
        line[0] = 0;
        if (k < (long)n) et_format(&a->ed, &a->ed.trk[etrk].ev[emap[k]], line, sizeof line, a->letters);
        len = (int)strlen(line);
        {
            int cols = ewid / (rp->TxWidth ? rp->TxWidth : 8);
            while (len < cols && len < (int)sizeof line - 1) line[len++] = ' ';
            if (len > cols) len = cols;
        }
        SetAPen(rp, k == esel ? ftx : tx);
        SetBPen(rp, k == esel ? fill : bg);
        Move(rp, ex0, (WORD)(y + rp->TxBaseline));
        Text(rp, (CONST_STRPTR)line, (ULONG)len);
    }
    SetBPen(rp, bg);
    GT_SetGadgetAttrs(eg[E_SCROLL], ew, NULL, GTSC_Total, n, GTSC_Visible, erows, GTSC_Top, etop, TAG_DONE);
}

static void ev_title(App *a)
{
    static char title[120];
    char nm[44];
    if (!ew || etrk < 0) return;
    name_of(a, (uint32_t)etrk, nm, sizeof nm);
    snprintf(title, sizeof title, "Events - %lu %s (%lu)", (unsigned long)etrk + 1, nm, (unsigned long)enum_ev(a));
    SetWindowTitles(ew, (CONST_STRPTR)title, (CONST_STRPTR)~0);
}

static void ev_select(App *a, long k, int show_line)
{
    uint32_t n = enum_ev(a);
    if (!n) { esel = -1; ev_draw(a); return; }
    if (k < 0) k = 0;
    if (k >= (long)n) k = (long)n - 1;
    esel = k;
    if (esel < etop) etop = esel;
    if (esel >= etop + erows) etop = esel - erows + 1;
    if (show_line) {
        et_format(&a->ed, &a->ed.trk[etrk].ev[emap[esel]], eline, sizeof eline, a->letters);
        GT_SetGadgetAttrs(eg[E_EDIT], ew, NULL, GTST_String, (ULONG)eline, TAG_DONE);
    }
    ev_draw(a);
}

static int ev_open(App *a, int trk)
{
    struct Screen *scr;
    struct NewGadget ng;
    struct Gadget *g;
    struct TextAttr ta;
    WORD fh, top, x0, y, w = 600, rh;
    etrk = trk;
    if (ew) { WindowToFront(ew); ActivateWindow(ew); etop = 0; esel = -1; ev_title(a); ev_select(a, 0, 1); return 1; }
    ta.ta_Name = (STRPTR)"topaz.font"; ta.ta_YSize = 8; ta.ta_Style = 0; ta.ta_Flags = 0;
    if (!mono) mono = OpenFont(&ta);
    if (!(scr = LockPubScreen((CONST_STRPTR)scr_pubname()))) return 0;
    evi = GetVisualInfo(scr, TAG_DONE);
    fh = scr->Font->ta_YSize;
    rh = mono ? mono->tf_YSize : fh;
    top = scr->WBorTop + fh + 1 + 2;
    x0 = scr->WBorLeft + 4;
    erows = (scr->Height > 300 ? 30 : 16);
    ex0 = x0; ey0 = top; ewid = (WORD)(w - 20);
    memset(&ng, 0, sizeof ng);
    ng.ng_TextAttr = scr->Font;
    ng.ng_VisualInfo = evi;
    g = CreateContext(&egl);
    ng.ng_LeftEdge = x0 + w - 16; ng.ng_TopEdge = top; ng.ng_Width = 16; ng.ng_Height = (WORD)(erows * rh); ng.ng_GadgetID = E_SCROLL;
    g = eg[E_SCROLL] = CreateGadget(SCROLLER_KIND, g, &ng, GTSC_Total, 1, GTSC_Visible, erows, PGA_Freedom, LORIENT_VERT, GA_RelVerify, TRUE, GA_Immediate, TRUE, TAG_DONE);
    y = (WORD)(top + erows * rh + 4);
    ng.ng_LeftEdge = x0; ng.ng_TopEdge = y; ng.ng_Width = w; ng.ng_Height = fh + 6; ng.ng_GadgetID = E_EDIT;
    g = eg[E_EDIT] = CreateGadget(STRING_KIND, g, &ng, GTST_MaxChars, 200, TAG_DONE);
    y += fh + 8;
    {
        static const char *const txt[4] = {"Apply", "Insert", "Delete", "Undo"};
        static const int ids[4] = {E_APPLY, E_INSERT, E_DELETE, E_UNDO};
        int k;
        for (k = 0; k < 4; k++) {
            ng.ng_LeftEdge = (WORD)(x0 + k * 74); ng.ng_TopEdge = y; ng.ng_Width = 70; ng.ng_Height = fh + 6;
            ng.ng_GadgetText = (UBYTE *)txt[k]; ng.ng_GadgetID = ids[k]; ng.ng_Flags = PLACETEXT_IN;
            g = eg[ids[k]] = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
        }
        ng.ng_LeftEdge = x0 + 300; ng.ng_Width = 96; ng.ng_GadgetText = NULL; ng.ng_GadgetID = E_FILTER;
        g = eg[E_FILTER] = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)FILTERS, GTCY_Active, efilter, TAG_DONE);
        ng.ng_LeftEdge = x0 + 400; ng.ng_Width = w - 400; ng.ng_GadgetText = NULL; ng.ng_GadgetID = E_INFO;
        g = eg[E_INFO] = CreateGadget(TEXT_KIND, g, &ng, GTTX_Text, (ULONG)"Enter = apply, Del = delete", GTTX_Border, TRUE, TAG_DONE);
    }
    y += fh + 8;
    if (!g) { FreeGadgets(egl); egl = NULL; FreeVisualInfo(evi); UnlockPubScreen(NULL, scr); return 0; }
    ew = OpenWindowTags(NULL, WA_Title, (ULONG)"Events", WA_Left, 10, WA_Top, 14,
        WA_InnerWidth, w + 8, WA_InnerHeight, y - scr->WBorTop - fh - 1,
        WA_Gadgets, (ULONG)egl, WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE,
        WA_PubScreen, (ULONG)scr,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_VANILLAKEY | SCROLLERIDCMP | STRINGIDCMP | BUTTONIDCMP | CYCLEIDCMP,
        TAG_DONE);
    UnlockPubScreen(NULL, scr);
    if (!ew) { FreeGadgets(egl); egl = NULL; FreeVisualInfo(evi); return 0; }
    GT_RefreshWindow(ew, NULL);
    etop = 0;
    ev_title(a);
    ev_select(a, 0, 1);
    return 1;
}

static void ev_close(void)
{
    if (!ew) return;
    CloseWindow(ew);
    ew = NULL;
    FreeGadgets(egl); egl = NULL;
    FreeVisualInfo(evi); evi = NULL;
    if (mono) CloseFont(mono);
    mono = NULL;
}

void edw_close_all(void)
{
    ev_close();
    tracks_close();
}

unsigned long edw_signal(void)
{
    return (tw ? 1UL << tw->UserPort->mp_SigBit : 0) | (ew ? 1UL << ew->UserPort->mp_SigBit : 0);
}

void edw_refresh(App *a)
{
    if (tw) {
        if (tsel >= (int)a->ed.ntrk) tsel = (int)a->ed.ntrk - 1;
        tracks_fill(a);
    }
    if (ew) {
        if (etrk >= (int)a->ed.ntrk || a->cur < 0) { ev_close(); return; }
        if (esel >= (long)enum_ev(a)) esel = (long)enum_ev(a) - 1;
        ev_title(a);
        ev_draw(a);
    }
}

int edw_events_open(App *a, int trk)
{
    if (a->cur < 0 || trk < 0 || (uint32_t)trk >= a->ed.ntrk) return 0;
    tsel = trk;
    return ev_open(a, trk);
}

int edw_selected_track(void)
{
    return tsel;
}

void edw_select_track(App *a, int t)
{
    tsel = t;
    if (tw) tracks_fill(a);
}

void edw_position(App *a, uint32_t pos_us)
{
    (void)a; (void)pos_us;
}

static void ev_info(const char *t)
{
    snprintf(einfo, sizeof einfo, "%s", t);
    if (ew) GT_SetGadgetAttrs(eg[E_INFO], ew, NULL, GTTX_Text, (ULONG)einfo, TAG_DONE);
}

/* Apply (replace the selected event) or insert the typed line. */
static int ev_apply(App *a, int insert)
{
    EdEv e;
    char err[120];
    const char *line = ((struct StringInfo *)eg[E_EDIT]->SpecialInfo)->Buffer;
    int ch = ed_channel(&a->ed, (uint32_t)etrk);
    long ni;
    if (!insert && (esel < 0 || esel >= (long)enum_ev(a))) insert = 1;
    if (!insert && a->ed.trk[etrk].ev[emap[esel]].st < 0xF0) ch = a->ed.trk[etrk].ev[emap[esel]].st & 0x0F;
    if (!et_parse(&a->ed, line, &e, ch, a->letters, err, sizeof err)) { ev_info(err); return 0; }
    app_ed_begin(a);
    if (insert) ni = ed_insert(&a->ed, (uint32_t)etrk, &e) - 1L;
    else ni = ed_replace(&a->ed, (uint32_t)etrk, emap[esel], &e);
    if (ni < 0) { ev_info("Not enough memory."); return 0; }
    app_ed_commit(a);
    ev_info(insert ? "Inserted." : "Changed.");
    ev_select(a, row_of(a, ni), 1);
    ev_title(a);
    return 1;
}

static int ev_delete(App *a)
{
    if (esel < 0 || esel >= (long)enum_ev(a)) return 0;
    app_ed_begin(a);
    ed_delete(&a->ed, (uint32_t)etrk, emap[esel]);
    app_ed_commit(a);
    ev_info("Deleted.");
    ev_select(a, esel, 1);
    ev_title(a);
    return 1;
}

int edw_handle(App *a)
{
    struct IntuiMessage *m;
    int changed = 0, tclose = 0, eclose = 0;
    while (tw && (m = GT_GetIMsg(tw->UserPort))) {
        ULONG cls = m->Class;
        UWORD code = m->Code;
        struct Gadget *src = (struct Gadget *)m->IAddress;
        GT_ReplyIMsg(m);
        if (cls == IDCMP_CLOSEWINDOW) { tclose = 1; break; }
        if (cls == IDCMP_REFRESHWINDOW) { GT_BeginRefresh(tw); GT_EndRefresh(tw, TRUE); continue; }
        if (cls != IDCMP_GADGETUP) continue;
        switch (src->GadgetID) {
        case T_LIST:
            tsel = code;
            tracks_fill(a);
            break;
        case T_NAME:
            if (tsel >= 0) {
                const char *s = ((struct StringInfo *)src->SpecialInfo)->Buffer;
                uint8_t enc[64];
                size_t n = tm_to_cp1250(s, enc, sizeof enc, a->letters);
                app_ed_begin(a);
                ed_set_name(&a->ed, (uint32_t)tsel, enc, (uint32_t)n);
                app_ed_commit(a);
                changed = 1;
            }
            break;
        case T_CH:
            if (tsel >= 0) {
                long c = ((struct StringInfo *)src->SpecialInfo)->LongInt;
                if (c >= 1 && c <= 16) {
                    app_ed_begin(a);
                    ed_set_channel(&a->ed, (uint32_t)tsel, (int)c - 1);
                    app_ed_commit(a);
                    changed = 1;
                }
            }
            break;
        case T_MUTE:
        case T_SOLO:
            if (tsel >= 0 && (uint32_t)tsel < a->ed.ntrk) {
                EdTrack *t = &a->ed.trk[tsel];
                if (src->GadgetID == T_MUTE) t->mute = (uint8_t)!t->mute; else t->solo = (uint8_t)!t->solo;
                app_track_mute(a);
                changed = 1;
            }
            break;
        case T_EVENTS:
            if (tsel >= 0) ev_open(a, tsel);
            break;
        case T_NEW:
            app_ed_begin(a);
            if (ed_add_track(&a->ed, tsel < 0 ? a->ed.ntrk : (uint32_t)tsel + 1)) {
                tsel = tsel < 0 ? (int)a->ed.ntrk - 1 : tsel + 1;
                if (etrk >= tsel) etrk++;
                ed_set_name(&a->ed, (uint32_t)tsel, (const uint8_t *)"New track", 9);
                app_ed_commit(a);
                changed = 1;
            }
            break;
        case T_DEL:
            if (tsel >= 0 && a->ed.ntrk > 1) {
                app_ed_begin(a);
                ed_del_track(&a->ed, (uint32_t)tsel);
                if (etrk == tsel) ev_close();
                else if (etrk > tsel) etrk--;
                if (tsel >= (int)a->ed.ntrk) tsel = (int)a->ed.ntrk - 1;
                app_ed_commit(a);
                changed = 1;
            }
            break;
        case T_UNDO:
            app_undo(a);
            changed = 1;
            break;
        case T_SAVE:
            app_save(a);
            changed = 1;
            break;
        case T_SAVEAS:
            app_save_as(a, tw);
            changed = 1;
            break;
        }
        if (changed) edw_refresh(a);
    }
    while (ew && (m = GT_GetIMsg(ew->UserPort))) {
        ULONG cls = m->Class, s = m->Seconds, mi = m->Micros;
        UWORD code = m->Code, qual = m->Qualifier;
        WORD mx = m->MouseX, my = m->MouseY;
        struct Gadget *src = (struct Gadget *)m->IAddress;
        GT_ReplyIMsg(m);
        switch (cls) {
        case IDCMP_CLOSEWINDOW:
            eclose = 1;
            break;
        case IDCMP_REFRESHWINDOW:
            GT_BeginRefresh(ew);
            GT_EndRefresh(ew, TRUE);
            ev_draw(a);
            break;
        case IDCMP_MOUSEBUTTONS:
            if (code == SELECTDOWN && mx >= ex0 && mx < ex0 + ewid && my >= ey0 && ew->RPort->TxHeight && my < ey0 + erows * ew->RPort->TxHeight) {
                long k = etop + (my - ey0) / ew->RPort->TxHeight;
                int dbl = k == esel && DoubleClick(eclick_s, eclick_m, s, mi);
                eclick_s = s; eclick_m = mi;
                if (k < (long)enum_ev(a)) ev_select(a, k, 1);
                if (dbl) ActivateGadget(eg[E_EDIT], ew, NULL);
            }
            break;
        case IDCMP_GADGETUP:
        case IDCMP_GADGETDOWN:
        case IDCMP_MOUSEMOVE:
            if (src->GadgetID == E_SCROLL) { etop = code; ev_draw(a); break; }
            if (cls != IDCMP_GADGETUP) break;
            switch (src->GadgetID) {
            case E_EDIT:
                /* Enter in the line: apply (Shift+Enter: insert) */
                if (code == 0 || code == 13) changed |= ev_apply(a, (qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0);
                break;
            case E_APPLY: changed |= ev_apply(a, 0); break;
            case E_INSERT: changed |= ev_apply(a, 1); break;
            case E_DELETE: changed |= ev_delete(a); break;
            case E_UNDO: app_undo(a); changed = 1; ev_info(a->info); edw_refresh(a); break;
            case E_FILTER: efilter = code; etop = 0; ev_title(a); ev_select(a, 0, 1); break;
            }
            break;
        case IDCMP_RAWKEY: {
            int page = qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT);
            if (code == 0x4C) ev_select(a, esel - (page ? erows : 1), 1);        /* up */
            else if (code == 0x4D) ev_select(a, esel + (page ? erows : 1), 1);   /* down */
            else if (code == 0x46) changed |= ev_delete(a);                       /* Del */
            break;
        }
        case IDCMP_VANILLAKEY:
            if (code == 13) ActivateGadget(eg[E_EDIT], ew, NULL);
            else if (code == 27) eclose = 1;
            break;
        }
    }
    if (eclose) ev_close();
    if (tclose) tracks_close();
    if (changed && tw) tracks_fill(a);
    return changed;
}
