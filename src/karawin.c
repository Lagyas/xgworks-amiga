/* XGWorks Lite: the Lyrics / Karaoke window, laid out like XGWorks NextGen's
 * (web and PC) Lyrics window:
 *   top:   lyrics track, text size, letters, Export / Import (Midimusic),
 *          Clear all, No diacritics
 *   left:  the lyric events (bar:beat:tick, text; click = select + song
 *          position), the selected one's text with Apply / Insert at the song
 *          position / Move to the song position / Delete
 *   right: the karaoke preview of the page being sung (dark, the sung part
 *          yellow, the chords on top), the Position line, Assign (typed
 *          text onto the notes from a bar), Fill Lyrics (beat marks, PSRUTI).
 * Everything undo-able (Undo in the other windows). */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <libraries/asl.h>
#include <graphics/text.h>
#include <graphics/view.h>
#include <graphics/gfxbase.h>
#include <libraries/diskfont.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <proto/diskfont.h>
#include <proto/asl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "karawin.h"
#include "lyrtools.h"
#include "textmap.h"
#include "edwin.h"
#include "rollops.h"
#include "screen.h"

struct Library *DiskfontBase;
extern struct Library *GadToolsBase, *AslBase;

enum { K_TRK = 1, K_SMALLER, K_BIGGER, K_LETTERS, K_EXPORT, K_IMPORT, K_CLEAR, K_PLAIN, K_SCROLL, K_EDIT, K_APPLY, K_INSERT, K_MOVE,
       K_DELETE, K_POS, K_TEXT, K_BAR, K_ASSIGN, K_FM, K_FB, K_FE, K_METHOD, K_FILL, K_UNFILL, K_INFO };

static struct Window *kw;
static struct Gadget *kgl, *g_trk, *g_scroll, *g_edit, *g_pos, *g_text, *g_bar, *g_fm, *g_fb, *g_fe, *g_info;
static void *kvi;
static struct TextFont *big, *mono;
static struct Screen *kscr;
static LONG pen_dark = -1, pen_yellow = -1, pen_white = -1;
static UWORD p_bg = 0, p_tx = 1, p_sh = 2, p_fill = 3;
static WORD lx0, ly0, lw, lh, px0, py0, pw, ph;
static int ktrk = -1, ksel = -1, ktop, krows, kmethod = 1;
static uint32_t *kmap, kmapn, kmapcap;
static uint32_t kpos, last_drawn = 0xFFFFFFFFUL;
static long last_syl = -2;
static char kinfo[120], kline[300], kfm[16] = ".-- ", kfb[16] = "--- ", kfe[16] = "---";

static const char *const LETTERS[] = {"Plain", "ISO-8859-2", "Latin-1", NULL};

int kw_is_open(void) { return kw != NULL; }
unsigned long kw_signal(void) { return kw ? 1UL << kw->UserPort->mp_SigBit : 0; }

static void info(const char *t)
{
    snprintf(kinfo, sizeof kinfo, "%s", t);
    if (kw) GT_SetGadgetAttrs(g_info, kw, NULL, GTTX_Text, (ULONG)kinfo, TAG_DONE);
}

/* the lyrics track: the one with lyrics, else the selected one, else the first with notes */
static int lyr_track(App *a)
{
    uint32_t t, i;
    int l = lt_lyrics_track(&a->ed), s = edw_selected_track();
    if (ktrk >= 0 && (uint32_t)ktrk < a->ed.ntrk) return ktrk;
    if (l >= 0) return l;
    if (s >= 0 && (uint32_t)s < a->ed.ntrk) return s;
    for (t = 0; t < a->ed.ntrk; t++)
        for (i = 0; i < a->ed.trk[t].n; i++)
            if (ro_is_note(&a->ed.trk[t].ev[i], -1) && (a->ed.trk[t].ev[i].st & 0x0F) != 9) return (int)t;
    return 0;
}

/* the lyric events of the track (their indices) */
static uint32_t make_map(App *a)
{
    uint32_t i, n;
    int t = lyr_track(a);
    kmapn = 0;
    if ((uint32_t)t >= a->ed.ntrk) return 0;
    n = a->ed.trk[t].n;
    if (n + 1 > kmapcap) {
        uint32_t *nm = (uint32_t *)realloc(kmap, (n + 64) * sizeof(uint32_t));
        if (!nm) return 0;
        kmap = nm;
        kmapcap = n + 64;
    }
    for (i = 0; i < n; i++)
        if (a->ed.trk[t].ev[i].st == 0xFF && a->ed.trk[t].ev[i].d1 == 0x05) kmap[kmapn++] = i;
    return kmapn;
}

/* a lyric as readable text in the chosen letters (line / page ends as / and <) */
static void shown_text(App *a, const EdEv *e, char *out, int max)
{
    char *p;
    tm_convert(a->ed.pool + e->data, e->len, out, (size_t)max, a->letters);
    for (p = out; *p; p++)
        if (*p == '\r') *p = '/'; else if (*p == '\n') *p = '<';
}

/* typed text into the song's bytes; "/" at the end and "<" stand for a line / page end */
static size_t typed_bytes(App *a, const char *in, uint8_t *out, size_t max)
{
    char t[300];
    size_t n = strlen(in);
    strncpy(t, in, sizeof t - 1);
    t[sizeof t - 1] = 0;
    if (n > sizeof t - 1) n = sizeof t - 1;
    if (n && t[n - 1] == '/') t[n - 1] = '\r';
    else if (n && t[n - 1] == '<') t[n - 1] = '\n';
    return tm_to_cp1250(t, out, max, a->letters);
}

/* ---- drawing ---- */

static void draw_list(App *a)
{
    struct RastPort *rp = kw->RPort;
    int t = lyr_track(a), r;
    uint32_t n = make_map(a);
    if (mono) SetFont(rp, mono);
    SetDrMd(rp, JAM2);
    for (r = 0; r < krows; r++) {
        long k = ktop + r;
        char line[120], txt[90];
        int len, cols = lw / (rp->TxWidth ? rp->TxWidth : 8);
        WORD y = (WORD)(ly0 + r * rp->TxHeight);
        line[0] = 0;
        if (k < (long)n) {
            const EdEv *e = &a->ed.trk[t].ev[kmap[k]];
            uint32_t bar, beat, tk;
            ed_mbt(&a->ed, e->tick, &bar, &beat, &tk);
            shown_text(a, e, txt, sizeof txt);
            snprintf(line, sizeof line, "%04lu:%lu:%03lu %s", (unsigned long)bar, (unsigned long)beat, (unsigned long)tk, txt);
        }
        len = (int)strlen(line);
        while (len < cols && len < (int)sizeof line - 1) line[len++] = ' ';
        if (len > cols) len = cols;
        SetAPen(rp, k == ksel ? p_sh : p_tx);
        SetBPen(rp, k == ksel ? p_fill : p_bg);
        Move(rp, lx0, (WORD)(y + rp->TxBaseline));
        Text(rp, (CONST_STRPTR)line, (ULONG)len);
    }
    SetBPen(rp, p_bg);
    SetDrMd(rp, JAM1);
    SetFont(rp, kscr->RastPort.Font);
    GT_SetGadgetAttrs(g_scroll, kw, NULL, GTSC_Total, n, GTSC_Visible, krows, GTSC_Top, ktop, TAG_DONE);
}

/* the karaoke preview of the page at `pos` */
static void draw_preview(App *a, uint32_t pos)
{
    struct RastPort *rp = kw->RPort;
    const Lyrics *ly = &a->ly;
    uint32_t tick = a->cur >= 0 ? xg_us_tick(&a->song, pos) : 0, first, n, L;
    long syl = ly_syl_at(ly, tick), ch = ly_chord_at(ly, tick);
    UWORD bg = pen_dark >= 0 ? (UWORD)pen_dark : p_tx;
    UWORD white = pen_white >= 0 ? (UWORD)pen_white : p_sh;
    int distinct = pen_yellow >= 0 && pen_yellow != (LONG)white && pen_yellow != (LONG)bg;
    UWORD yellow = distinct ? (UWORD)pen_yellow : p_fill;
    WORD y, lhgt;
    last_syl = syl;
    SetDrMd(rp, JAM1);
    SetAPen(rp, bg);
    RectFill(rp, px0, py0, (WORD)(px0 + pw - 1), (WORD)(py0 + ph - 1));
    /* the chords: now (yellow) and the next ones */
    SetFont(rp, kscr->RastPort.Font);
    if (ly->nchord) {
        char next[60];
        long k;
        const char *now = ch >= 0 ? ly->chord[ch].name : "";
        next[0] = 0;
        for (k = ch + 1; k < (long)ly->nchord && k <= ch + 4; k++) {
            strcat(next, "  ");
            strcat(next, ly->chord[k].name);
        }
        Move(rp, (WORD)(px0 + 6), (WORD)(py0 + 3 + rp->TxBaseline));
        SetAPen(rp, yellow);
        Text(rp, (CONST_STRPTR)now, strlen(now));
        SetAPen(rp, white);
        Text(rp, (CONST_STRPTR)next, strlen(next));
    }
    if (!ly->nline) {
        static const char none[] = "No lyrics in this song.";
        SetAPen(rp, white);
        Move(rp, (WORD)(px0 + 6), (WORD)(py0 + ph / 2));
        Text(rp, (CONST_STRPTR)none, sizeof none - 1);
        return;
    }
    if (big) SetFont(rp, big);
    lhgt = (WORD)(rp->TxHeight + 2);
    ly_page_at(ly, tick, &first, &n);
    /* as many lines as fit; the line being sung stays in */
    {
        WORD top = (WORD)(py0 + rp->TxHeight + 8);
        uint32_t fit = (uint32_t)((py0 + ph - top) / lhgt), cur = syl >= 0 ? ly->syl[syl].line : first;
        if (!fit) fit = 1;
        if (n > fit) {
            if (cur >= first + fit) first = cur - fit + 1;
            n = fit;
        }
        y = top;
        if (big) y = (WORD)(py0 + 4 + kscr->RastPort.Font->tf_YSize + 4);
    }
    for (L = first; L < first + n; L++, y += lhgt) {
        const LyLine *ln = &ly->line[L];
        LONG width = 0;
        uint32_t j;
        WORD x;
        for (j = 0; j < ln->n; j++) {
            const char *t = ly->text + ly->syl[ln->first + j].off;
            width += TextLength(rp, (CONST_STRPTR)t, strlen(t));
        }
        x = (WORD)(px0 + (width < pw ? (pw - width) / 2 : 2));
        Move(rp, x, (WORD)(y + rp->TxBaseline));
        for (j = 0; j < ln->n; j++) {
            const char *t = ly->text + ly->syl[ln->first + j].off;
            int sung = (long)(ln->first + j) <= syl;
            if (sung && !distinct) {
                /* few colours: the sung part on a filled background */
                SetDrMd(rp, JAM2);
                SetAPen(rp, p_sh);
                SetBPen(rp, p_fill);
            } else {
                SetDrMd(rp, JAM1);
                SetAPen(rp, sung ? yellow : white);
            }
            if (rp->cp_x + TextLength(rp, (CONST_STRPTR)t, strlen(t)) < px0 + pw) Text(rp, (CONST_STRPTR)t, strlen(t));
        }
        SetDrMd(rp, JAM1);
        SetBPen(rp, p_bg);
    }
    SetFont(rp, kscr->RastPort.Font);
}

void kw_update(App *a, uint32_t pos_us, int force)
{
    long syl;
    if (!kw) return;
    kpos = pos_us;
    a->pos_us = pos_us;
    syl = a->cur >= 0 ? ly_syl_at(&a->ly, xg_us_tick(&a->song, pos_us)) : -1;
    if (force || syl != last_syl) draw_preview(a, pos_us);
    if (force) draw_list(a);
    if (force || pos_us / 1000000 != last_drawn) {
        last_drawn = pos_us / 1000000;
        GT_SetGadgetAttrs(g_pos, kw, NULL, GTSL_Level, (WORD)(pos_us / 1000000), GTSL_Max, (WORD)(a->len_us / 1000000 + 1), TAG_DONE);
    }
}

/* ---- the window ---- */

static void open_fonts(App *a)
{
    struct TextAttr ta;
    struct Process *me = (struct Process *)FindTask(NULL);
    APTR old = me->pr_WindowPtr;
    if (big) { CloseFont(big); big = NULL; }
    if (!DiskfontBase) DiskfontBase = OpenLibrary((CONST_STRPTR)"diskfont.library", 36);
    ta.ta_Name = (STRPTR)"helvetica.font";
    ta.ta_YSize = (UWORD)a->kfont;
    ta.ta_Style = FS_NORMAL;
    ta.ta_Flags = 0;
    /* no "insert volume FONTS:" requester (it would wait unseen) */
    me->pr_WindowPtr = (APTR)-1;
    if (DiskfontBase) big = OpenDiskFont(&ta);
    me->pr_WindowPtr = old;
    if (!mono) {
        ta.ta_Name = (STRPTR)"topaz.font";
        ta.ta_YSize = 8;
        mono = OpenFont(&ta);
    }
}

int kw_open(App *a)
{
    struct NewGadget ng;
    struct Gadget *g;
    struct DrawInfo *dri;
    WORD fh, x0, y, W, H, bh, rx;
    if (kw) { WindowToFront(kw); ActivateWindow(kw); kw_update(a, a->pos_us, 1); return 1; }
    if (!(kscr = LockPubScreen((CONST_STRPTR)scr_pubname()))) return 0;
    if ((dri = GetScreenDrawInfo(kscr))) {
        p_bg = dri->dri_Pens[BACKGROUNDPEN]; p_tx = dri->dri_Pens[SHADOWPEN];
        p_sh = dri->dri_Pens[SHINEPEN]; p_fill = dri->dri_Pens[FILLPEN];
        FreeScreenDrawInfo(kscr, dri);
    }
    /* the web's colours where the screen has them */
    if (GfxBase->LibNode.lib_Version >= 39) {
        struct ColorMap *cm = kscr->ViewPort.ColorMap;
        pen_dark = ObtainBestPen(cm, 0x10000000, 0x14000000, 0x28000000, OBP_Precision, PRECISION_GUI, TAG_DONE);
        pen_yellow = ObtainBestPen(cm, 0xFF000000, 0xC8000000, 0x28000000, OBP_Precision, PRECISION_GUI, TAG_DONE);
        pen_white = ObtainBestPen(cm, 0xFF000000, 0xFF000000, 0xFF000000, OBP_Precision, PRECISION_GUI, TAG_DONE);
    }
    open_fonts(a);
    kvi = GetVisualInfo(kscr, TAG_DONE);
    fh = kscr->Font->ta_YSize;
    bh = fh + 4;
    W = kscr->Width > 660 ? 640 : kscr->Width - 12;
    H = kscr->Height - kscr->BarHeight - kscr->WBorTop - fh - 6;
    if (H > 330) H = 330;
    x0 = (WORD)(kscr->WBorLeft + 4);
    y = (WORD)(kscr->WBorTop + fh + 1 + 3);
    memset(&ng, 0, sizeof ng);
    ng.ng_TextAttr = kscr->Font;
    ng.ng_VisualInfo = kvi;
    g = CreateContext(&kgl);
    /* the top row */
    ng.ng_TopEdge = y; ng.ng_Height = bh; ng.ng_Flags = PLACETEXT_LEFT;
    ng.ng_LeftEdge = x0 + 44; ng.ng_Width = 34; ng.ng_GadgetText = (UBYTE *)"Track"; ng.ng_GadgetID = K_TRK;
    g = g_trk = CreateGadget(INTEGER_KIND, g, &ng, GTIN_Number, lyr_track(a) + 1, GTIN_MaxChars, 3, TAG_DONE);
    {
        static const char *const tt[6] = {"A-", "A+", "Export...", "Import...", "Clear all", "No diacr."};
        static const int ids[6] = {K_SMALLER, K_BIGGER, K_EXPORT, K_IMPORT, K_CLEAR, K_PLAIN};
        static const WORD bw[6] = {26, 26, 74, 74, 72, 76};
        WORD x = (WORD)(x0 + 84);
        int k;
        ng.ng_Flags = PLACETEXT_IN;
        for (k = 0; k < 6; k++) {
            if (k == 2) {
                ng.ng_LeftEdge = x; ng.ng_Width = 100; ng.ng_GadgetText = NULL; ng.ng_GadgetID = K_LETTERS;
                g = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)LETTERS, GTCY_Active, a->letters, TAG_DONE);
                x += 104;
            }
            ng.ng_LeftEdge = x; ng.ng_Width = bw[k]; ng.ng_GadgetText = (UBYTE *)tt[k]; ng.ng_GadgetID = ids[k];
            g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
            x += bw[k] + 2;
        }
    }
    y += bh + 4;
    /* the left column: the list and its editing */
    lx0 = x0;
    ly0 = y;
    lw = 232;
    krows = (H - (y - kscr->WBorTop - fh - 1) - 2 * (bh + 4) - 4) / (mono ? mono->tf_YSize : 8);
    if (krows < 4) krows = 4;
    lh = (WORD)(krows * (mono ? mono->tf_YSize : 8));
    ng.ng_LeftEdge = (WORD)(lx0 + lw); ng.ng_TopEdge = ly0; ng.ng_Width = 14; ng.ng_Height = lh; ng.ng_GadgetID = K_SCROLL;
    g = g_scroll = CreateGadget(SCROLLER_KIND, g, &ng, GTSC_Total, 1, GTSC_Visible, krows, PGA_Freedom, LORIENT_VERT, GA_RelVerify, TRUE, TAG_DONE);
    ng.ng_LeftEdge = lx0; ng.ng_TopEdge = (WORD)(ly0 + lh + 4); ng.ng_Width = lw + 14; ng.ng_Height = bh; ng.ng_GadgetID = K_EDIT;
    g = g_edit = CreateGadget(STRING_KIND, g, &ng, GTST_MaxChars, 120, TAG_DONE);
    {
        static const char *const tt[4] = {"Apply", "Insert", "Move", "Delete"};
        static const int ids[4] = {K_APPLY, K_INSERT, K_MOVE, K_DELETE};
        int k;
        for (k = 0; k < 4; k++) {
            ng.ng_LeftEdge = (WORD)(lx0 + k * 62); ng.ng_TopEdge = (WORD)(ly0 + lh + bh + 8); ng.ng_Width = 60;
            ng.ng_GadgetText = (UBYTE *)tt[k]; ng.ng_GadgetID = ids[k];
            g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
        }
    }
    /* the right column: the preview, the position, Assign, Fill Lyrics */
    rx = (WORD)(lx0 + lw + 22);
    px0 = rx;
    py0 = ly0;
    pw = (WORD)(x0 + W - rx - 4);
    ph = (WORD)(lh + 4 - 3 * (bh + 4) + bh + 4);
    if (ph < 40) ph = 40;
    ng.ng_Flags = PLACETEXT_LEFT;
    y = (WORD)(py0 + ph + 4);
    ng.ng_LeftEdge = (WORD)(rx + 30); ng.ng_TopEdge = y; ng.ng_Width = pw - 30; ng.ng_Height = bh; ng.ng_GadgetText = (UBYTE *)"Pos"; ng.ng_GadgetID = K_POS;
    g = g_pos = CreateGadget(SLIDER_KIND, g, &ng, GTSL_Min, 0, GTSL_Max, (WORD)(a->len_us / 1000000 + 1), GTSL_Level, 0, GA_RelVerify, TRUE, TAG_DONE);
    y += bh + 4;
    ng.ng_LeftEdge = (WORD)(rx + 30); ng.ng_TopEdge = y; ng.ng_Width = pw - 30 - 120; ng.ng_GadgetText = (UBYTE *)"Text"; ng.ng_GadgetID = K_TEXT;
    g = g_text = CreateGadget(STRING_KIND, g, &ng, GTST_MaxChars, 250, TAG_DONE);
    ng.ng_LeftEdge = (WORD)(rx + pw - 86); ng.ng_Width = 32; ng.ng_GadgetText = (UBYTE *)"Bar"; ng.ng_GadgetID = K_BAR;
    g = g_bar = CreateGadget(INTEGER_KIND, g, &ng, GTIN_Number, 1, GTIN_MaxChars, 4, TAG_DONE);
    ng.ng_LeftEdge = (WORD)(rx + pw - 52); ng.ng_Width = 52; ng.ng_Flags = PLACETEXT_IN; ng.ng_GadgetText = (UBYTE *)"Assign"; ng.ng_GadgetID = K_ASSIGN;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    y += bh + 4;
    ng.ng_Flags = PLACETEXT_LEFT; ng.ng_TopEdge = y; ng.ng_Width = 34;
    ng.ng_LeftEdge = (WORD)(rx + 30); ng.ng_GadgetText = (UBYTE *)"Fill"; ng.ng_GadgetID = K_FM;
    g = g_fm = CreateGadget(STRING_KIND, g, &ng, GTST_String, (ULONG)kfm, GTST_MaxChars, 12, TAG_DONE);
    ng.ng_LeftEdge = (WORD)(rx + 66); ng.ng_GadgetText = NULL; ng.ng_GadgetID = K_FB;
    g = g_fb = CreateGadget(STRING_KIND, g, &ng, GTST_String, (ULONG)kfb, GTST_MaxChars, 12, TAG_DONE);
    ng.ng_LeftEdge = (WORD)(rx + 102); ng.ng_GadgetID = K_FE;
    g = g_fe = CreateGadget(STRING_KIND, g, &ng, GTST_String, (ULONG)kfe, GTST_MaxChars, 12, TAG_DONE);
    ng.ng_LeftEdge = (WORD)(rx + 138); ng.ng_Width = pw - 138 - 104; ng.ng_GadgetID = K_METHOD;
    g = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)lt_fill_methods, GTCY_Active, kmethod, TAG_DONE);
    ng.ng_Flags = PLACETEXT_IN; ng.ng_LeftEdge = (WORD)(rx + pw - 102); ng.ng_Width = 48; ng.ng_GadgetText = (UBYTE *)"Fill"; ng.ng_GadgetID = K_FILL;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    ng.ng_LeftEdge = (WORD)(rx + pw - 52); ng.ng_Width = 52; ng.ng_GadgetText = (UBYTE *)"Unfill"; ng.ng_GadgetID = K_UNFILL;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    y = (WORD)(ly0 + lh + 2 * bh + 12);
    ng.ng_LeftEdge = x0; ng.ng_TopEdge = y; ng.ng_Width = W - 8; ng.ng_Height = fh + 4; ng.ng_GadgetText = NULL; ng.ng_GadgetID = K_INFO;
    g = g_info = CreateGadget(TEXT_KIND, g, &ng, GTTX_Text, (ULONG)"Click a lyric: it is selected and the song goes there.", GTTX_Border, TRUE, TAG_DONE);
    y += fh + 6;
    if (!g) { FreeGadgets(kgl); kgl = NULL; FreeVisualInfo(kvi); UnlockPubScreen(NULL, kscr); return 0; }
    kw = OpenWindowTags(NULL, WA_Title, (ULONG)"Lyrics / Karaoke", WA_Left, 0, WA_Top, kscr->BarHeight + 1,
        WA_InnerWidth, W, WA_InnerHeight, y - kscr->WBorTop - fh - 1,
        WA_Gadgets, (ULONG)kgl, WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE,
        WA_SimpleRefresh, TRUE, WA_PubScreen, (ULONG)kscr,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | SCROLLERIDCMP | SLIDERIDCMP |
                  STRINGIDCMP | INTEGERIDCMP | BUTTONIDCMP | CYCLEIDCMP, TAG_DONE);
    if (!kw) {
        FreeGadgets(kgl); kgl = NULL; FreeVisualInfo(kvi); kvi = NULL;
        UnlockPubScreen(NULL, kscr);
        return 0;
    }
    GT_RefreshWindow(kw, NULL);
    ksel = -1;
    ktop = 0;
    kw_update(a, a->pos_us, 1);
    return 1;
}

void kw_close(void)
{
    if (!kw) return;
    CloseWindow(kw);
    kw = NULL;
    FreeGadgets(kgl); kgl = NULL;
    FreeVisualInfo(kvi); kvi = NULL;
    if (GfxBase->LibNode.lib_Version >= 39) {
        struct ColorMap *cm = kscr->ViewPort.ColorMap;
        if (pen_dark >= 0) ReleasePen(cm, (ULONG)pen_dark);
        if (pen_yellow >= 0) ReleasePen(cm, (ULONG)pen_yellow);
        if (pen_white >= 0) ReleasePen(cm, (ULONG)pen_white);
    }
    pen_dark = pen_yellow = pen_white = -1;
    UnlockPubScreen(NULL, kscr);
    kscr = NULL;
    if (big) CloseFont(big);
    if (mono) CloseFont(mono);
    big = mono = NULL;
}

/* ---- the actions ---- */

static const char *str(struct Gadget *g) { return (const char *)((struct StringInfo *)g->SpecialInfo)->Buffer; }
static long num(struct Gadget *g) { return ((struct StringInfo *)g->SpecialInfo)->LongInt; }

/* static: the line as it was put into the edit line (to see whether it was changed) */
static char kshown[300];

static void select_row(App *a, long k)
{
    int t = lyr_track(a);
    uint32_t n = make_map(a);
    if (!n) { ksel = -1; draw_list(a); return; }
    if (k < 0) k = 0;
    if (k >= (long)n) k = (long)n - 1;
    ksel = k;
    if (ksel < ktop) ktop = (int)ksel;
    if (ksel >= ktop + krows) ktop = (int)ksel - krows + 1;
    {
        const EdEv *e = &a->ed.trk[t].ev[kmap[ksel]];
        shown_text(a, e, kline, sizeof kline);
        strcpy(kshown, kline);
        GT_SetGadgetAttrs(g_edit, kw, NULL, GTST_String, (ULONG)kline, TAG_DONE);
        a->locate_us = (long)xg_tick_us(&a->song, e->tick);
    }
    draw_list(a);
}

static int add_lyric(App *a, uint32_t tick, const char *text)
{
    EdEv e;
    uint8_t enc[200];
    size_t n = typed_bytes(a, text, enc, sizeof enc);
    memset(&e, 0, sizeof e);
    e.tick = tick; e.st = 0xFF; e.d1 = 0x05; e.len = (uint32_t)n;
    if ((e.data = ed_pool_add(&a->ed, enc, (uint32_t)n)) == 0xFFFFFFFFUL) return 0;
    return ed_insert(&a->ed, (uint32_t)lyr_track(a), &e) > 0;
}

static int ask(App *a, int save, const char *title, const char *file, char *path, int max)
{
    struct FileRequester *fr;
    int ok = 0;
    if (!AslBase) return 0;
    fr = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest, ASLFR_TitleText, (ULONG)title, ASLFR_DoSaveMode, save,
        ASLFR_InitialDrawer, (ULONG)a->folder, ASLFR_InitialFile, (ULONG)file, ASLFR_DoPatterns, TRUE,
        ASLFR_InitialPattern, (ULONG)"#?.(txt|lyrics)", ASLFR_Window, (ULONG)kw, TAG_DONE);
    if (!fr) return 0;
    if (AslRequest(fr, NULL) && fr->fr_File && fr->fr_File[0]) {
        strncpy(path, (const char *)fr->fr_Drawer, (size_t)max - 1);
        path[max - 1] = 0;
        AddPart((STRPTR)path, (CONST_STRPTR)fr->fr_File, (ULONG)max);
        ok = 1;
    }
    FreeAslRequest(fr);
    return ok;
}

int kw_handle(App *a, uint32_t pos_us)
{
    struct IntuiMessage *m;
    int changed = 0, close = 0;
    while (kw && (m = GT_GetIMsg(kw->UserPort))) {
        ULONG cls = m->Class;
        UWORD code = m->Code;
        WORD mx = m->MouseX, my = m->MouseY;
        struct Gadget *src = (struct Gadget *)m->IAddress;
        int t = lyr_track(a), edit = 0;
        char msg[120];
        GT_ReplyIMsg(m);
        switch (cls) {
        case IDCMP_CLOSEWINDOW: close = 1; break;
        case IDCMP_REFRESHWINDOW:
            GT_BeginRefresh(kw);
            GT_EndRefresh(kw, TRUE);
            kw_update(a, pos_us, 1);
            break;
        case IDCMP_MOUSEBUTTONS:
            if (code == SELECTDOWN && mx >= lx0 && mx < lx0 + lw && my >= ly0 && my < ly0 + lh && mono)
                select_row(a, ktop + (my - ly0) / mono->tf_YSize);
            break;
        case IDCMP_RAWKEY:
            if (code == 0x4C) select_row(a, ksel - 1);
            else if (code == 0x4D) select_row(a, ksel + 1);
            else if (code == 0x45) close = 1;
            break;
        case IDCMP_MOUSEMOVE:
        case IDCMP_GADGETUP:
            if (src->GadgetID == K_SCROLL) { ktop = code; draw_list(a); break; }
            if (src->GadgetID == K_POS) { if (cls == IDCMP_GADGETUP) a->locate_us = (long)code * 1000000L; break; }
            if (cls != IDCMP_GADGETUP || a->cur < 0) break;
            switch (src->GadgetID) {
            case K_TRK: {
                long v = num(g_trk);
                if (v >= 1 && v <= (long)a->ed.ntrk) ktrk = (int)v - 1;
                ksel = -1; ktop = 0;
                draw_list(a);
                break;
            }
            case K_SMALLER:
            case K_BIGGER:
                a->kfont += src->GadgetID == K_BIGGER ? 2 : -2;
                if (a->kfont < 8) a->kfont = 8;
                if (a->kfont > 48) a->kfont = 48;
                open_fonts(a);
                kw_update(a, pos_us, 1);
                break;
            case K_LETTERS:
                app_set_letters(a, code);
                kw_update(a, pos_us, 1);
                break;
            case K_APPLY:
            case K_EDIT:
                if (ksel >= 0 && ksel < (long)make_map(a) && !strcmp(str(g_edit), kshown)) {
                    info("Not changed.");
                } else if (ksel >= 0 && ksel < (long)make_map(a)) {
                    /* only a changed text is written (the letters the window cannot show stay otherwise) */
                    EdEv *e = &a->ed.trk[t].ev[kmap[ksel]];
                    uint8_t enc[200];
                    size_t n = typed_bytes(a, str(g_edit), enc, sizeof enc);
                    uint32_t at;
                    app_ed_begin(a);
                    e = &a->ed.trk[t].ev[kmap[ksel]];
                    if ((at = ed_pool_add(&a->ed, enc, (uint32_t)n)) != 0xFFFFFFFFUL) { e->data = at; e->len = (uint32_t)n; }
                    edit = 1;
                    info("Changed.");
                }
                break;
            case K_INSERT:
                app_ed_begin(a);
                add_lyric(a, xg_us_tick(&a->song, a->pos_us), str(g_edit)[0] ? str(g_edit) : "la ");
                edit = 1;
                info("Inserted at the song position.");
                break;
            case K_MOVE:
                if (ksel >= 0 && ksel < (long)make_map(a)) {
                    EdEv e = a->ed.trk[t].ev[kmap[ksel]];
                    app_ed_begin(a);
                    e.tick = xg_us_tick(&a->song, a->pos_us);
                    ed_replace(&a->ed, (uint32_t)t, kmap[ksel], &e);
                    edit = 1;
                    info("Moved to the song position.");
                }
                break;
            case K_DELETE:
                if (ksel >= 0 && ksel < (long)make_map(a)) {
                    app_ed_begin(a);
                    ed_delete(&a->ed, (uint32_t)t, kmap[ksel]);
                    edit = 1;
                    info("Deleted.");
                }
                break;
            case K_CLEAR: {
                uint32_t i, o;
                EdTrack *tr = &a->ed.trk[t];
                app_ed_begin(a);
                for (i = o = 0; i < tr->n; i++)
                    if (!(tr->ev[i].st == 0xFF && tr->ev[i].d1 == 0x05)) tr->ev[o++] = tr->ev[i];
                snprintf(msg, sizeof msg, "%lu lyrics deleted (Undo brings them back).", (unsigned long)(tr->n - o));
                tr->n = o;
                ksel = -1;
                edit = 1;
                info(msg);
                break;
            }
            case K_PLAIN:
                app_ed_begin(a);
                snprintf(msg, sizeof msg, "%d texts without diacritics now.", lt_strip_diacritics(&a->ed));
                edit = 1;
                info(msg);
                break;
            case K_ASSIGN: {
                int left, nt = edw_selected_track(), n;
                long bar = num(g_bar);
                if (nt < 0 || (uint32_t)nt >= a->ed.ntrk) nt = t;
                app_ed_begin(a);
                n = lt_text_to_notes(&a->ed, (uint32_t)t, (uint32_t)nt, -1, ed_tick_of(&a->ed, (uint32_t)(bar < 1 ? 1 : bar), 1, 0), str(g_text), a->letters, &left);
                snprintf(msg, sizeof msg, "%d syllables on the notes of track %d%s", n, nt + 1, left ? " - more syllables than notes" : ".");
                edit = 1;
                info(msg);
                break;
            }
            case K_METHOD: kmethod = code; break;
            case K_FILL:
            case K_UNFILL:
                snprintf(kfm, sizeof kfm, "%s", str(g_fm));
                snprintf(kfb, sizeof kfb, "%s", str(g_fb));
                snprintf(kfe, sizeof kfe, "%s", str(g_fe));
                app_ed_begin(a);
                if (src->GadgetID == K_FILL) snprintf(msg, sizeof msg, "%d fill lyrics in.", lt_fill_insert(&a->ed, (uint32_t)t, kfm, kfb, kfe, 30, kmethod, a->letters));
                else snprintf(msg, sizeof msg, "%d fill lyrics taken away.", lt_fill_delete(&a->ed, (uint32_t)t, kfm, kfb, kfe, a->letters));
                edit = 1;
                info(msg);
                break;
            case K_EXPORT: {
                char name[140], path[256], *dot;
                strncpy(name, a->names[a->cur], sizeof name - 12);
                name[sizeof name - 12] = 0;
                if ((dot = strrchr(name, '.'))) *dot = 0;
                strcat(name, "_lyrics.txt");
                if (ask(a, 1, "Export lyrics and chords (Midimusic)", name, path, sizeof path)) {
                    size_t n;
                    char *txt = lt_export(&a->ed, t, a->names[a->cur], 0, &n);
                    BPTR fh;
                    if (txt && (fh = Open((CONST_STRPTR)path, MODE_NEWFILE))) { Write(fh, txt, (LONG)n); Close(fh); info("Exported."); }
                    else info("Cannot write the file.");
                    free(txt);
                }
                break;
            }
            case K_IMPORT: {
                char path[256];
                if (ask(a, 0, "Import lyrics and chords (Midimusic): the old ones are replaced", "", path, sizeof path)) {
                    BPTR fh = Open((CONST_STRPTR)path, MODE_OLDFILE);
                    LONG n;
                    char *txt;
                    int bad, lines;
                    if (!fh) { info("The file cannot be read."); break; }
                    Seek(fh, 0, OFFSET_END);
                    n = Seek(fh, 0, OFFSET_BEGINNING);
                    if (n > 0 && (txt = (char *)malloc((size_t)n + 1))) {
                        Read(fh, txt, n);
                        txt[n] = 0;
                        app_ed_begin(a);
                        lines = lt_import(&a->ed, (uint32_t)t, 0, txt, (size_t)n, 1, &bad);
                        snprintf(msg, sizeof msg, "%d lines read%s", lines, bad ? " - some not understood." : ".");
                        info(msg);
                        free(txt);
                        edit = 1;
                    }
                    Close(fh);
                }
                break;
            }
            }
            if (edit) {
                app_ed_commit(a);
                changed = 1;
                kw_update(a, a->pos_us, 1);
            }
            break;
        }
    }
    if (close) kw_close();
    return changed;
}
