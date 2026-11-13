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
#include <graphics/view.h>
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
#include "staffwin.h"
#include "drumwin.h"
#include "karawin.h"
#include "xgmwin.h"
#include "jobwin.h"
#include "jobs.h"
#include "voices.h"
#include "mixer.h"
#include "screen.h"
#include "pal.h"
#include "transport.h"
#include "lyrwin.h"
#include "lyredwin.h"
#include <libraries/gadtools.h>

extern struct Library *GadToolsBase;

enum { V_FROM = 1, V_TO, V_CREATE, V_COUNT, V_REPEAT, V_NAME, V_COPY, V_CUT, V_PASTE, V_DEL, V_OUT, V_IN, V_UNDO, V_HPROP, V_VPROP, V_INFO, V_DMCH, V_DMNOTE, V_ROLL, V_LIST, V_SOUND };

static struct Window *vw;
static struct Gadget *vgl, *hprop, *vprop, *g_from, *g_to, *g_count, *g_name, *g_info;
static void *vvi;
static WORD tb, rh, cx0, gx0, gx1, gy0, gy1;
static UWORD pen_bg = 0, pen_tx = 1, pen_sh = 2, pen_fill = 3;
/* the colours of the PC / web Track View (XGworks): obtained on open */
enum { C_HEAD, C_ROW, C_LANE, C_GRID, C_BLOCK, C_BSEL, C_RULER, C_WHITE, C_BLACK, C_SOLO, C_MUTE, C_CYAN, C_N };
static const int cmap[C_N] = {PAL_HEAD, PAL_ROW, PAL_LANE, PAL_GRID, PAL_BLOCK, PAL_RED, PAL_RULER, PAL_WHITE, PAL_BLACK, PAL_SOLO, PAL_MUTE, PAL_CYAN};
static uint32_t cur_tick = 0xFFFFFFFFUL;   /* the play cursor (XOR line), none = ~0 */
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
static struct Menu *vmenu;
static struct TextFont *vmono;
/* the Demix dialog */
static struct Window *dmw;
static struct Gadget *dmgl, *dm_scope, *dm_ch[16];
static void *dmvi;
static int dm_by_note, dm_all;
static App *dm_app;

/* the Track menu (the right mouse button), as the web / PC version's track menu */
enum { M_ROLL = 1, M_LIST, M_STAFF, M_DRUMS, M_LYRICS, M_MIXER, M_XGM, M_DEMIXCH, M_DEMIXNOTE, M_TRANSPOSE, M_JOBS,
       M_SETL, M_SETR, M_GOL, M_GOR, M_LOOP };
static struct NewMenu nm[] = {
    {NM_TITLE, (STRPTR)"Track", NULL, 0, 0, NULL},
    {NM_ITEM, (STRPTR)"Piano Roll", (STRPTR)"R", 0, 0, (APTR)M_ROLL},
    {NM_ITEM, (STRPTR)"Event List", (STRPTR)"L", 0, 0, (APTR)M_LIST},
    {NM_ITEM, (STRPTR)"Staff", (STRPTR)"S", 0, 0, (APTR)M_STAFF},
    {NM_ITEM, (STRPTR)"Drums", (STRPTR)"D", 0, 0, (APTR)M_DRUMS},
    {NM_ITEM, (STRPTR)"Lyrics / Karaoke", (STRPTR)"Y", 0, 0, (APTR)M_LYRICS},
    {NM_ITEM, (STRPTR)"Mixer", (STRPTR)"M", 0, 0, (APTR)M_MIXER},
    {NM_ITEM, (STRPTR)"Sound / bank...", (STRPTR)"X", 0, 0, (APTR)M_XGM},
    {NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
    {NM_ITEM, (STRPTR)"Demix by Channel...", NULL, 0, 0, (APTR)M_DEMIXCH},
    {NM_ITEM, (STRPTR)"Demix by Note...", NULL, 0, 0, (APTR)M_DEMIXNOTE},
    {NM_ITEM, (STRPTR)"Transpose...", (STRPTR)"T", 0, 0, (APTR)M_TRANSPOSE},
    {NM_ITEM, (STRPTR)"Jobs...", (STRPTR)"J", 0, 0, (APTR)M_JOBS},
    {NM_TITLE, (STRPTR)"Locators", NULL, 0, 0, NULL},
    {NM_ITEM, (STRPTR)"Set L here (the position)", (STRPTR)"1", 0, 0, (APTR)M_SETL},
    {NM_ITEM, (STRPTR)"Set R here (the position)", (STRPTR)"2", 0, 0, (APTR)M_SETR},
    {NM_ITEM, (STRPTR)"Go to L", NULL, 0, 0, (APTR)M_GOL},
    {NM_ITEM, (STRPTR)"Go to R", NULL, 0, 0, (APTR)M_GOR},
    {NM_ITEM, (STRPTR)"Loop L - R on / off", (STRPTR)"O", 0, 0, (APTR)M_LOOP},
    {NM_END, NULL, NULL, 0, 0, NULL},
};

int tv_is_open(void) { return vw != NULL; }
unsigned long tv_signal(void) { return (vw ? 1UL << vw->UserPort->mp_SigBit : 0) | (dmw ? 1UL << dmw->UserPort->mp_SigBit : 0); }

static uint32_t bar_ticks(App *a)
{
    uint32_t tk[64], num[64], bl[64];
    if (a->cur < 0 || ed_sigs(&a->ed, tk, num, bl, 64) < 1 || !num[0] || !bl[0]) return 1920;
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
    gx0 = (WORD)(cx0 + 36 * (vmono ? vmono->tf_XSize : 8) + 6);
    gx1 = (WORD)(vw->Width - vw->BorderRight - 1);
    gy0 = (WORD)(tb + vw->RPort->TxHeight + 5);
    gy1 = (WORD)(vw->Height - vw->BorderBottom - 1);
}

static void cursor_xor(App *a);

/* a colour of the PC Track View, else the screen's pen */
static UWORD CP(int c, UWORD def) { return pal(cmap[c], def); }

/* ---- the toolbar: the editors and the functions as icons (as the PC / web main window) ---- */
enum { I_NEW, I_OPEN, I_SAVE, I_SAVEAS, I_SONGS, I_UNDO, I_LIST, I_ROLL, I_STAFF, I_DRUM, I_MIXER, I_XGM, I_LYRICS, I_LYRED,
       I_TEXTS, I_JOBS, I_TRANSPORT, I_N };
static const char *const ILABEL[I_N] = {"New", "Open", "Save", "As..", "Songs", "Undo", "List", "Roll", "Staff", "Drum", "Mixer",
                                        "XGM", "Lyric", "LyrEd", "Texts", "Jobs", "Trans"};
static const char *const ITIP[I_N] = {"New song", "Open a song...", "Save the song", "Save as... (.MID / .XWS)", "Songs: the list, tempo, transpose, reset, output, screen",
                                      "Undo", "Event List of the selected track", "Piano Roll", "Staff (notation)", "Drum editor", "Mixer",
                                      "XGM: sound / bank of the track's part", "Lyrics / Karaoke", "Lyrics editor", "Texts: lyrics and chords tools",
                                      "Jobs: transpose, quantize, demix ...", "Transport: play, locators, loop"};
static WORD tbx0, tby0, tbw = 38, tbh;

static void line(struct RastPort *rp, WORD x0, WORD y0, WORD x1, WORD y1) { Move(rp, x0, y0); Draw(rp, x1, y1); }
static void box(struct RastPort *rp, WORD x0, WORD y0, WORD x1, WORD y1) { Move(rp, x0, y0); Draw(rp, x1, y0); Draw(rp, x1, y1); Draw(rp, x0, y1); Draw(rp, x0, y0); }

/* one icon, 22 x 14 at (x, y) */
static void draw_icon(struct RastPort *rp, int id, WORD x, WORD y)
{
    UWORD black = CP(C_BLACK, pen_tx), white = CP(C_WHITE, pen_sh), blue = pal(PAL_NOTE, pen_fill), red = pal(PAL_RED, pen_tx),
          yellow = pal(PAL_K_HAT, pen_fill), grey = pal(PAL_EDGE, pen_tx), green = pal(PAL_K_PERC, pen_fill), light = pal(PAL_LIGHT, pen_bg);
    int k;
    switch (id) {
    case I_NEW:
        SetAPen(rp, white); RectFill(rp, x + 6, y, x + 15, y + 13);
        SetAPen(rp, black); Move(rp, x + 6, y); Draw(rp, x + 12, y); Draw(rp, x + 15, y + 3); Draw(rp, x + 15, y + 13); Draw(rp, x + 6, y + 13); Draw(rp, x + 6, y);
        line(rp, x + 12, y, x + 12, y + 3); line(rp, x + 12, y + 3, x + 15, y + 3);
        break;
    case I_OPEN:
        SetAPen(rp, yellow); RectFill(rp, x + 3, y + 4, x + 18, y + 13);
        SetAPen(rp, black); box(rp, x + 3, y + 3, x + 18, y + 13); line(rp, x + 3, y + 3, x + 6, y + 1); line(rp, x + 6, y + 1, x + 10, y + 1); line(rp, x + 10, y + 1, x + 11, y + 3);
        break;
    case I_SAVE: case I_SAVEAS:
        SetAPen(rp, blue); RectFill(rp, x + 4, y, x + 17, y + 13);
        SetAPen(rp, white); RectFill(rp, x + 7, y + 1, x + 14, y + 5);
        SetAPen(rp, light); RectFill(rp, x + 7, y + 8, x + 14, y + 13);
        SetAPen(rp, black); box(rp, x + 4, y, x + 17, y + 13);
        if (id == I_SAVEAS) { SetAPen(rp, red); RectFill(rp, x + 16, y + 9, x + 21, y + 13); }
        break;
    case I_SONGS:
        SetAPen(rp, white); RectFill(rp, x + 2, y, x + 19, y + 13);
        SetAPen(rp, black); box(rp, x + 2, y, x + 19, y + 13);
        for (k = 0; k < 4; k++) { SetAPen(rp, k == 1 ? blue : black); line(rp, x + 5, y + 3 + k * 3, x + 16, y + 3 + k * 3); }
        break;
    case I_UNDO:
        SetAPen(rp, blue);
        line(rp, x + 6, y + 4, x + 15, y + 4); line(rp, x + 6, y + 5, x + 15, y + 5);
        line(rp, x + 15, y + 4, x + 18, y + 7); line(rp, x + 18, y + 7, x + 18, y + 10); line(rp, x + 18, y + 10, x + 15, y + 13);
        line(rp, x + 16, y + 4, x + 19, y + 7); line(rp, x + 19, y + 7, x + 19, y + 10);
        for (k = 0; k < 4; k++) line(rp, x + 3 + k, y + 4 - k, x + 3 + k, y + 5 + k);
        break;
    case I_LIST:
        SetAPen(rp, white); RectFill(rp, x + 2, y, x + 19, y + 13);
        SetAPen(rp, black); box(rp, x + 2, y, x + 19, y + 13);
        for (k = 0; k < 4; k++) { line(rp, x + 4, y + 3 + k * 3, x + 8, y + 3 + k * 3); line(rp, x + 10, y + 3 + k * 3, x + 17, y + 3 + k * 3); }
        break;
    case I_ROLL:
        SetAPen(rp, white); RectFill(rp, x + 2, y, x + 19, y + 13);
        SetAPen(rp, black); box(rp, x + 2, y, x + 19, y + 13); RectFill(rp, x + 2, y + 2, x + 5, y + 3); RectFill(rp, x + 2, y + 8, x + 5, y + 9);
        SetAPen(rp, blue); RectFill(rp, x + 7, y + 2, x + 12, y + 3); RectFill(rp, x + 11, y + 6, x + 17, y + 7); RectFill(rp, x + 8, y + 10, x + 14, y + 11);
        break;
    case I_STAFF:
        SetAPen(rp, black);
        for (k = 0; k < 5; k++) line(rp, x + 1, y + 2 + k * 2, x + 20, y + 2 + k * 2);
        RectFill(rp, x + 8, y + 7, x + 11, y + 9); line(rp, x + 11, y, x + 11, y + 8);
        RectFill(rp, x + 14, y + 5, x + 17, y + 7); line(rp, x + 17, y, x + 17, y + 6); line(rp, x + 11, y, x + 17, y);
        break;
    case I_DRUM:
        SetAPen(rp, red); RectFill(rp, x + 4, y + 5, x + 17, y + 12);
        SetAPen(rp, white); RectFill(rp, x + 4, y + 3, x + 17, y + 5);
        SetAPen(rp, black); box(rp, x + 4, y + 3, x + 17, y + 12); line(rp, x + 4, y + 5, x + 17, y + 5);
        line(rp, x + 2, y, x + 8, y + 3); line(rp, x + 19, y, x + 13, y + 3);
        break;
    case I_MIXER:
        SetAPen(rp, black);
        for (k = 0; k < 4; k++) line(rp, x + 4 + k * 5, y, x + 4 + k * 5, y + 13);
        SetAPen(rp, grey);
        for (k = 0; k < 4; k++) RectFill(rp, x + 2 + k * 5, y + (k == 0 ? 3 : k == 1 ? 8 : k == 2 ? 5 : 10), x + 6 + k * 5, y + (k == 0 ? 5 : k == 1 ? 10 : k == 2 ? 7 : 12));
        break;
    case I_XGM:
        SetAPen(rp, green); RectFill(rp, x + 1, y + 1, x + 20, y + 12);
        SetAPen(rp, black); box(rp, x + 1, y + 1, x + 20, y + 12);
        SetAPen(rp, white);
        line(rp, x + 4, y + 3, x + 9, y + 10); line(rp, x + 9, y + 3, x + 4, y + 10);
        line(rp, x + 17, y + 3, x + 12, y + 3); line(rp, x + 12, y + 3, x + 12, y + 10); line(rp, x + 12, y + 10, x + 17, y + 10); line(rp, x + 17, y + 10, x + 17, y + 7); line(rp, x + 17, y + 7, x + 15, y + 7);
        break;
    case I_LYRICS: case I_LYRED:
        SetAPen(rp, pal(PAL_LANE, pen_tx)); RectFill(rp, x + 1, y + 1, x + 20, y + 12);
        SetAPen(rp, yellow); line(rp, x + 3, y + 5, x + 18, y + 5); line(rp, x + 3, y + 9, x + 12, y + 9);
        SetAPen(rp, white); line(rp, x + 12, y + 9, x + 18, y + 9);
        if (id == I_LYRED) { SetAPen(rp, red); for (k = 0; k < 3; k++) line(rp, x + 12 + k, y + 13, x + 20 + k, y + 4); }
        break;
    case I_TEXTS:
        SetAPen(rp, black); RectFill(rp, x + 5, y + 1, x + 16, y + 2); RectFill(rp, x + 10, y + 1, x + 11, y + 13);
        SetAPen(rp, blue); RectFill(rp, x + 14, y + 8, x + 16, y + 10); line(rp, x + 16, y + 3, x + 16, y + 9);
        break;
    case I_JOBS:
        SetAPen(rp, grey); RectFill(rp, x + 3, y + 1, x + 12, y + 5);
        SetAPen(rp, pal(PAL_K_HAT, pen_tx)); for (k = 0; k < 3; k++) line(rp, x + 7 + k, y + 5, x + 15 + k, y + 13);
        SetAPen(rp, black); box(rp, x + 3, y + 1, x + 12, y + 5);
        break;
    case I_TRANSPORT:
        SetAPen(rp, black); RectFill(rp, x + 1, y + 1, x + 20, y + 12);
        SetAPen(rp, green);
        for (k = 0; k < 6; k++) line(rp, x + 5 + k, y + 3 + k / 2, x + 5 + k, y + 10 - k / 2);
        RectFill(rp, x + 13, y + 4, x + 17, y + 9);
        break;
    }
}

static void draw_toolbar(void)
{
    struct RastPort *rp;
    int i;
    if (!vw) return;
    rp = vw->RPort;
    SetDrMd(rp, JAM1);
    SetAPen(rp, pal(PAL_LIGHT, pen_bg));
    RectFill(rp, vw->BorderLeft, tby0, (WORD)(vw->Width - vw->BorderRight - 1), (WORD)(tby0 + tbh - 1));
    if (vmono) SetFont(rp, vmono);
    for (i = 0; i < I_N; i++) {
        WORD x = (WORD)(tbx0 + i * tbw + (i >= I_SONGS) * 4 + (i >= I_UNDO) * 4 + (i >= I_LIST) * 6 + (i >= I_TRANSPORT) * 6);
        const char *l = ILABEL[i];
        if (x + tbw > vw->Width - vw->BorderRight) break;
        /* the button: raised */
        SetAPen(rp, CP(C_WHITE, pen_sh)); Move(rp, x, (WORD)(tby0 + tbh - 2)); Draw(rp, x, (WORD)(tby0 + 1)); Draw(rp, (WORD)(x + tbw - 2), (WORD)(tby0 + 1));
        SetAPen(rp, pal(PAL_EDGE, pen_tx)); Draw(rp, (WORD)(x + tbw - 2), (WORD)(tby0 + tbh - 2)); Draw(rp, x, (WORD)(tby0 + tbh - 2));
        draw_icon(rp, i, (WORD)(x + (tbw - 22) / 2), (WORD)(tby0 + 3));
        SetAPen(rp, CP(C_BLACK, pen_tx));
        Move(rp, (WORD)(x + (tbw - 1 - TextLength(rp, (CONST_STRPTR)l, strlen(l))) / 2), (WORD)(tby0 + tbh - 3 - rp->TxHeight + rp->TxBaseline));
        Text(rp, (CONST_STRPTR)l, strlen(l));
    }
    SetFont(rp, vw->WScreen->RastPort.Font);
}

/* the button at x (-1 = none) */
static int toolbar_hit(WORD mx)
{
    int i;
    for (i = 0; i < I_N; i++) {
        WORD x = (WORD)(tbx0 + i * tbw + (i >= I_SONGS) * 4 + (i >= I_UNDO) * 4 + (i >= I_LIST) * 6 + (i >= I_TRANSPORT) * 6);
        if (mx >= x && mx < x + tbw - 1) return i;
    }
    return -1;
}

struct Window *tv_window(void) { return vw; }


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
    endt = tx(a, gx1);
    /* the head row: the column heads on purple, the ruler on blue (as the PC version) */
    SetAPen(rp, CP(C_HEAD, pen_fill));
    RectFill(rp, cx0, tb, (WORD)(gx0 - 1), (WORD)(gy0 - 1));
    SetAPen(rp, CP(C_LANE, pen_bg));
    RectFill(rp, gx0, tb, gx1, (WORD)(gy0 - 1));
    /* the track column and the lanes */
    SetAPen(rp, CP(C_ROW, pen_bg));
    RectFill(rp, cx0, gy0, (WORD)(gx0 - 1), gy1);
    SetAPen(rp, CP(C_LANE, pen_bg));
    RectFill(rp, gx0, gy0, gx1, gy1);
    if (vmono) SetFont(rp, vmono);
    SetAPen(rp, CP(C_WHITE, pen_sh));
    Move(rp, (WORD)(cx0 + 2), (WORD)(tb + 2 + rp->TxBaseline));
    Text(rp, (CONST_STRPTR)"Trk Ch Name         S M Sound", 29);
    SetFont(rp, vw->WScreen->RastPort.Font);
    /* bar numbers and the bar lines over all rows */
    for (b0 = htop / bar; b0 * bar <= endt; b0++) {
        WORD x = xt(a, b0 * bar);
        if (x < gx0) continue;
        SetAPen(rp, CP(C_GRID, pen_sh));
        Move(rp, x, gy0); Draw(rp, x, gy1);
        if (bpx >= 16 || b0 % 4 == 0) {
            sprintf(txt, "%lu", (unsigned long)b0 + 1);
            SetAPen(rp, CP(C_RULER, pen_sh));
            Move(rp, (WORD)(x + 2), (WORD)(tb + 2 + rp->TxBaseline));
            Text(rp, (CONST_STRPTR)txt, strlen(txt));
        }
    }
    /* the locators in the ruler: L / R lines with their letters, the loop between them */
    if (a->cur >= 0) {
        UWORD col = pal(PAL_SOLO, pen_sh);
        WORD xl = xt(a, a->loc_l), xr = xt(a, a->loc_r);
        if (a->loop_on && a->loc_r > a->loc_l) {
            WORD x0 = xl < gx0 ? gx0 : xl, x1 = xr > gx1 ? gx1 : xr;
            if (x1 > x0) { SetAPen(rp, col); RectFill(rp, x0, (WORD)(gy0 - 4), x1, (WORD)(gy0 - 2)); }
        }
        if (vmono) SetFont(rp, vmono);
        SetAPen(rp, col);
        if (xl >= gx0 && xl <= gx1) {
            Move(rp, xl, tb); Draw(rp, xl, (WORD)(gy0 - 1));
            if (xl + 9 < gx1) { Move(rp, (WORD)(xl + 2), (WORD)(tb + 1 + rp->TxBaseline)); Text(rp, (CONST_STRPTR)"L", 1); }
        }
        if (xr >= gx0 && xr <= gx1) {
            Move(rp, xr, tb); Draw(rp, xr, (WORD)(gy0 - 1));
            if (xr + 9 < gx1) { Move(rp, (WORD)(xr + 2), (WORD)(tb + 1 + rp->TxBaseline)); Text(rp, (CONST_STRPTR)"R", 1); }
        }
        SetFont(rp, vw->WScreen->RastPort.Font);
    }
    /* the separator under the head */
    SetAPen(rp, CP(C_BLACK, pen_tx));
    Move(rp, cx0, (WORD)(gy0 - 1)); Draw(rp, gx1, (WORD)(gy0 - 1));
    Move(rp, (WORD)(gx0 - 1), tb); Draw(rp, (WORD)(gx0 - 1), gy1);
    for (r = 0; r + ttop < (int)a->ed.ntrk; r++) {
        int ti = r + ttop, nb, k, ch;
        WORD y = (WORD)(gy0 + r * rh);
        static EdBlock blks[256];
        char nm[44] = "";
        uint32_t len;
        const uint8_t *n;
        if (y + rh > gy1) break;
        /* the track column: the selected track darker */
        if (ti == vtrk) { SetAPen(rp, CP(C_HEAD, pen_fill)); RectFill(rp, cx0, y, (WORD)(gx0 - 2), (WORD)(y + rh - 2)); }
        n = ed_name(&a->ed, (uint32_t)ti, &len);
        if (n) tm_convert(n, len > 40 ? 40 : len, nm, sizeof nm, a->letters);
        ch = ed_channel(&a->ed, (uint32_t)ti);
        {
            /* Trk Ch Name S M Sound */
            char chs[4], snd[44] = "";
            if (ch >= 0) {
                const uint8_t *v = a->mx.val[ch];
                sprintf(chs, "%2d", ch + 1);
                snprintf(snd, sizeof snd, "%s", vx_sound(ch, v[MX_MSB], v[MX_LSB], v[MX_PROG]));
            } else strcpy(chs, " -");
            snprintf(txt, sizeof txt, "%3d %s %-12.12s %c %c %-12.12s", ti + 1, chs, nm, a->ed.trk[ti].solo ? 'S' : '.', a->ed.trk[ti].mute ? 'M' : '.', snd);
        }
        if (vmono) SetFont(rp, vmono);
        {
            WORD cw = vmono ? vmono->tf_XSize : 8, bx = (WORD)(cx0 + 2);
            /* S / M as coloured boxes when on (as the PC version) */
            if (a->ed.trk[ti].solo) { SetAPen(rp, CP(C_SOLO, pen_fill)); RectFill(rp, (WORD)(bx + 20 * cw - 1), (WORD)(y + 1), (WORD)(bx + 21 * cw), (WORD)(y + rh - 3)); }
            if (a->ed.trk[ti].mute) { SetAPen(rp, CP(C_MUTE, pen_fill)); RectFill(rp, (WORD)(bx + 22 * cw - 1), (WORD)(y + 1), (WORD)(bx + 23 * cw), (WORD)(y + rh - 3)); }
            SetAPen(rp, CP(C_WHITE, ti == vtrk ? pen_sh : pen_tx));
            Move(rp, bx, (WORD)(y + 2 + rp->TxBaseline));
            Text(rp, (CONST_STRPTR)txt, strlen(txt));
            if (a->ed.trk[ti].solo) { SetAPen(rp, CP(C_BLACK, pen_tx)); Move(rp, (WORD)(bx + 20 * cw), (WORD)(y + 2 + rp->TxBaseline)); Text(rp, (CONST_STRPTR)"S", 1); }
        }
        SetFont(rp, vw->WScreen->RastPort.Font);
        /* the row lines */
        SetAPen(rp, CP(C_GRID, pen_sh));
        Move(rp, gx0, (WORD)(y + rh - 1)); Draw(rp, gx1, (WORD)(y + rh - 1));
        SetAPen(rp, CP(C_BLACK, pen_tx));
        Move(rp, cx0, (WORD)(y + rh - 1)); Draw(rp, (WORD)(gx0 - 2), (WORD)(y + rh - 1));
        if (ti == vtrk) {
            SetAPen(rp, CP(C_CYAN, pen_sh));
            Move(rp, cx0, y); Draw(rp, (WORD)(gx0 - 2), y); Draw(rp, (WORD)(gx0 - 2), (WORD)(y + rh - 2)); Draw(rp, cx0, (WORD)(y + rh - 2)); Draw(rp, cx0, y);
        }
        /* the blocks */
        nb = bk_view(&a->ed, (uint32_t)ti, bar, blks, 256);
        for (k = 0; k < nb; k++) {
            WORD x0 = xt(a, blks[k].start), x1 = (WORD)(xt(a, blks[k].end > blks[k].start ? blks[k].end : blks[k].start + bar) - 1);
            int s = is_sel(ti, k);
            if (x1 < gx0 || x0 > gx1) continue;
            if (x0 < gx0) x0 = gx0;
            if (x1 > gx1) x1 = gx1;
            SetAPen(rp, s ? CP(C_BSEL, pen_tx) : CP(C_BLOCK, pen_fill));
            RectFill(rp, x0, (WORD)(y + 1), x1, (WORD)(y + rh - 3));
            SetAPen(rp, CP(C_BLACK, pen_tx));
            Move(rp, x0, (WORD)(y + 1)); Draw(rp, x1, (WORD)(y + 1)); Draw(rp, x1, (WORD)(y + rh - 3)); Draw(rp, x0, (WORD)(y + rh - 3)); Draw(rp, x0, (WORD)(y + 1));
            if (blks[k].name[0] && x1 - x0 > 16) {
                char bn[44];
                int maxc = (x1 - x0 - 4) / (rp->TxWidth ? rp->TxWidth : 8);
                tm_convert((const uint8_t *)blks[k].name, strlen(blks[k].name), bn, sizeof bn, a->letters);
                if ((int)strlen(bn) > maxc) bn[maxc > 0 ? maxc : 0] = 0;
                SetAPen(rp, CP(C_WHITE, pen_sh));
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
    cursor_xor(a);
}

/* the play cursor: an XOR line over the lanes (drawn twice it is gone) */
static void cursor_xor(App *a)
{
    struct RastPort *rp;
    WORD x;
    if (!vw || cur_tick == 0xFFFFFFFFUL) return;
    x = xt(a, cur_tick);
    if (x < gx0 || x > gx1) return;
    rp = vw->RPort;
    SetDrMd(rp, COMPLEMENT);
    Move(rp, x, tb); Draw(rp, x, gy1);
    SetDrMd(rp, JAM1);
}

void tv_position(App *a, uint32_t pos_us, int playing)
{
    uint32_t t, bar;
    static char last_info[120];
    static uint32_t last_l = 0xFFFFFFFFUL, last_r;
    static int last_loop = -1;
    if (vw && !drag && a->cur >= 0 && (a->loc_l != last_l || a->loc_r != last_r || a->loop_on != last_loop)) {
        last_l = a->loc_l; last_r = a->loc_r; last_loop = a->loop_on;
        draw(a);
    }
    /* the program's messages on the info line (the main window's) */
    if (vw && strcmp(last_info, a->info)) {
        snprintf(last_info, sizeof last_info, "%s", a->info);
        info(a->info);
    }
    if (!vw || a->cur < 0 || drag) return;
    t = xg_us_tick(&a->song, pos_us);
    if (t == cur_tick) return;
    cursor_xor(a);
    cur_tick = t;
    bar = bar_ticks(a);
    if (playing && (xt(a, t) > gx1 || t < htop)) {
        /* the next page */
        htop = t / bar * bar;
        cur_tick = 0xFFFFFFFFUL;
        draw(a);
        cur_tick = t;
    }
    cursor_xor(a);
}

static void drag_xor(App *a)
{
    struct RastPort *rp = vw->RPort;
    uint32_t bar = bar_ticks(a);
    long d = ((long)tx(a, dx1) - (long)tx(a, dx0)) / (long)bar * (long)bar;
    int k;
    SetDrMd(rp, COMPLEMENT);
    for (k = 0; k < nsel; k++) {
        static EdBlock blks[256];
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
    if (vw) { WindowToFront(vw); ActivateWindow(vw); draw(a); draw_toolbar(); return 1; }
    if (!(scr = LockPubScreen((CONST_STRPTR)scr_pubname()))) return 0;
    if ((dri = GetScreenDrawInfo(scr))) {
        pen_bg = dri->dri_Pens[BACKGROUNDPEN]; pen_tx = dri->dri_Pens[SHADOWPEN];
        pen_sh = dri->dri_Pens[SHINEPEN]; pen_fill = dri->dri_Pens[FILLPEN];
        FreeScreenDrawInfo(scr, dri);
    }
    pal_open(scr);
    vvi = GetVisualInfo(scr, TAG_DONE);
    fh = scr->Font->ta_YSize;
    {
        struct TextAttr ta;
        ta.ta_Name = (STRPTR)"topaz.font"; ta.ta_YSize = 8; ta.ta_Style = 0; ta.ta_Flags = 0;
        if (!vmono) vmono = OpenFont(&ta);
    }
    /* the toolbar band, then the gadget rows */
    tbx0 = (WORD)(scr->WBorLeft + 2);
    tby0 = (WORD)(scr->WBorTop + fh + 1 + 1);
    tbh = (WORD)(3 + 14 + 2 + 8 + 4);
    y = (WORD)(tby0 + tbh + 2);
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
    x += 110 + 8;
    ng.ng_Flags = PLACETEXT_IN; ng.ng_LeftEdge = x; ng.ng_Width = 76; ng.ng_GadgetText = (UBYTE *)"Sound..."; ng.ng_GadgetID = V_SOUND;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    y += fh + 7;
    {
        static const char *const bt[11] = {"Copy", "Cut", "Paste", "Delete", "-", "+", "Undo", "Roll", "List", "Demix Ch...", "Demix Note..."};
        static const int ids[11] = {V_COPY, V_CUT, V_PASTE, V_DEL, V_OUT, V_IN, V_UNDO, V_ROLL, V_LIST, V_DMCH, V_DMNOTE};
        static const WORD bw[11] = {44, 36, 48, 54, 20, 20, 44, 40, 40, 92, 104};
        int k;
        x = (WORD)(scr->WBorLeft + 4);
        ng.ng_Flags = PLACETEXT_IN; ng.ng_TopEdge = y;
        for (k = 0; k < 11; k++) {
            ng.ng_LeftEdge = x; ng.ng_Width = bw[k]; ng.ng_GadgetText = (UBYTE *)bt[k]; ng.ng_GadgetID = ids[k];
            g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
            x += bw[k] + (k == 6 || k == 8 ? 10 : 3);
        }
        /* the messages on a row of their own */
        y += fh + 7;
        ng.ng_LeftEdge = (WORD)(scr->WBorLeft + 4); ng.ng_TopEdge = y; ng.ng_Width = 580; ng.ng_GadgetText = NULL; ng.ng_GadgetID = V_INFO;
        g = g_info = CreateGadget(TEXT_KIND, g, &ng, GTTX_Text, (ULONG)"Click a track's Sound to choose its program / bank. Demix: click a track, Demix Ch... / Note...", GTTX_Border, TRUE, TAG_DONE);
    }
    tb = (WORD)(y + fh + 7);
    if (!g) { FreeGadgets(vgl); vgl = NULL; FreeVisualInfo(vvi); UnlockPubScreen(NULL, scr); pal_close(); return 0; }
    /* the main window: the screen's width (up to 720), down to the Transport */
    vw = OpenWindowTags(NULL, WA_Title, (ULONG)"XGWorks Lite - Track View", WA_Left, 0, WA_Top, scr->BarHeight + 1,
        WA_Width, scr->Width > 720 ? 720 : scr->Width,
        WA_Height, scr->Height - scr->BarHeight - 1 - (scr->Height > 300 ? 120 : 0) > 200 ? scr->Height - scr->BarHeight - 1 - (scr->Height > 300 ? 120 : 0) : scr->Height - scr->BarHeight - 1,
        WA_MinWidth, 600, WA_MinHeight, 140, WA_MaxWidth, ~0, WA_MaxHeight, ~0,
        WA_Gadgets, (ULONG)vgl, WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_SizeGadget, TRUE,
        WA_SizeBBottom, TRUE, WA_SizeBRight, TRUE, WA_Activate, TRUE, WA_SimpleRefresh, TRUE, WA_NewLookMenus, TRUE, WA_PubScreen, (ULONG)scr,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_NEWSIZE | IDCMP_REFRESHWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_RAWKEY |
                  IDCMP_IDCMPUPDATE | IDCMP_GADGETUP | IDCMP_MENUPICK | INTEGERIDCMP | STRINGIDCMP | BUTTONIDCMP, TAG_DONE);
    UnlockPubScreen(NULL, scr);
    if (!vw) { FreeGadgets(vgl); vgl = NULL; FreeVisualInfo(vvi); pal_close(); return 0; }
    /* the Track menu on the right mouse button */
    if ((vmenu = CreateMenus(nm, TAG_DONE))) {
        if (LayoutMenus(vmenu, vvi, GTMN_NewLookMenus, TRUE, TAG_DONE)) SetMenuStrip(vw, vmenu);
        else { FreeMenus(vmenu); vmenu = NULL; }
    }
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
    vtrk = a->cur >= 0 ? edw_selected_track() : -1;
    draw(a);
    draw_toolbar();
    info(a->info[0] ? a->info : "Open a song (Open / Songs). Click a track's Sound for its program / bank.");
    return 1;
}

void tv_close(void)
{
    int k;
    if (!vw) return;
    if (dmw) { CloseWindow(dmw); dmw = NULL; FreeGadgets(dmgl); dmgl = NULL; FreeVisualInfo(dmvi); dmvi = NULL; }
    if (vmenu) { ClearMenuStrip(vw); FreeMenus(vmenu); vmenu = NULL; }
    if (hprop) RemoveGadget(vw, hprop);
    if (vprop) RemoveGadget(vw, vprop);
    CloseWindow(vw);
    vw = NULL;
    if (vmono) CloseFont(vmono);
    vmono = NULL;
    pal_close();
    cur_tick = 0xFFFFFFFFUL;
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
    if (a->cur < 0) { nsel = 0; vtrk = -1; draw(a); draw_toolbar(); return; }
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

/* ---- the Demix dialog (as the web / PC version's) ---- */

static const char *demix_label(int ch, int key)
{
    App *a = dm_app;
    const uint8_t *v = a ? a->mx.val[ch & 15] : NULL;
    if (v && (v[MX_MSB] >= 126 || ch == 9)) return vx_drum(v[MX_MSB] >= 126 ? v[MX_MSB] : 127, v[MX_PROG], key);
    return NULL;
}

static const char *const SCOPES[] = {"The selected track", "All tracks", NULL};

static void demix_open(App *a, int by_note)
{
    struct Screen *scr;
    struct NewGadget ng;
    struct Gadget *g;
    WORD fh, x0, y;
    int c, ch;
    if (dmw) { WindowToFront(dmw); return; }
    if (!(scr = LockPubScreen((CONST_STRPTR)scr_pubname()))) return;
    dm_by_note = by_note;
    dm_app = a;
    dmvi = GetVisualInfo(scr, TAG_DONE);
    fh = scr->Font->ta_YSize;
    x0 = (WORD)(scr->WBorLeft + 8);
    y = (WORD)(scr->WBorTop + fh + 1 + 4);
    memset(&ng, 0, sizeof ng);
    ng.ng_TextAttr = scr->Font;
    ng.ng_VisualInfo = dmvi;
    g = CreateContext(&dmgl);
    ng.ng_LeftEdge = x0; ng.ng_TopEdge = y; ng.ng_Width = 200; ng.ng_Height = fh + 4; ng.ng_GadgetID = 1;
    g = dm_scope = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)SCOPES, GTCY_Active, dm_all, TAG_DONE);
    y += fh + 10;
    ch = vtrk >= 0 ? ed_channel(&a->ed, (uint32_t)vtrk) : 9;
    if (ch < 0) ch = 9;
    if (by_note) {
        /* the channels whose notes go one track per key */
        ng.ng_Flags = PLACETEXT_RIGHT; ng.ng_Width = 26; ng.ng_Height = fh + 3;
        for (c = 0; c < 16; c++) {
            static char lbl[16][4];
            sprintf(lbl[c], "%d", c + 1);
            ng.ng_LeftEdge = (WORD)(x0 + (c % 8) * 52); ng.ng_TopEdge = (WORD)(y + (c / 8) * (fh + 6));
            ng.ng_GadgetText = (UBYTE *)lbl[c]; ng.ng_GadgetID = 10 + c;
            g = dm_ch[c] = CreateGadget(CHECKBOX_KIND, g, &ng, GTCB_Checked, c == ch, GTCB_Scaled, TRUE, TAG_DONE);
        }
        y += 2 * (fh + 6) + 4;
    }
    ng.ng_Flags = PLACETEXT_IN; ng.ng_TopEdge = y; ng.ng_Height = fh + 6; ng.ng_Width = 90;
    ng.ng_LeftEdge = x0; ng.ng_GadgetText = (UBYTE *)"Demix"; ng.ng_GadgetID = 2;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    ng.ng_LeftEdge = x0 + 96; ng.ng_GadgetText = (UBYTE *)"Cancel"; ng.ng_GadgetID = 3;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    y += fh + 12;
    if (!g) { FreeGadgets(dmgl); dmgl = NULL; FreeVisualInfo(dmvi); UnlockPubScreen(NULL, scr); return; }
    dmw = OpenWindowTags(NULL, WA_Title, (ULONG)(by_note ? "Demix by Note" : "Demix by Channel"), WA_Left, 120, WA_Top, 60,
        WA_InnerWidth, by_note ? 430 : 220, WA_InnerHeight, y - scr->WBorTop - fh - 1,
        WA_Gadgets, (ULONG)dmgl, WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE,
        WA_PubScreen, (ULONG)scr, WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | CYCLEIDCMP | CHECKBOXIDCMP | BUTTONIDCMP, TAG_DONE);
    UnlockPubScreen(NULL, scr);
    if (!dmw) { FreeGadgets(dmgl); dmgl = NULL; FreeVisualInfo(dmvi); return; }
    GT_RefreshWindow(dmw, NULL);
}

static void demix_close(void)
{
    if (!dmw) return;
    CloseWindow(dmw);
    dmw = NULL;
    FreeGadgets(dmgl); dmgl = NULL;
    FreeVisualInfo(dmvi); dmvi = NULL;
}

/* Demix: the tracks from the last one (the new tracks go right after theirs) */
static int demix_run(App *a)
{
    int t, n = 0, from, to, c;
    uint16_t chans = 0;
    char msg[80];
    if (dm_by_note) {
        for (c = 0; c < 16; c++)
        {
            ULONG on = 0;
            GT_GetGadgetAttrs(dm_ch[c], dmw, NULL, GTCB_Checked, (ULONG)&on, TAG_DONE);
            if (on) chans |= (uint16_t)(1u << c);
        }
        if (!chans) { info("Demix by Note: tick the channels."); return 0; }
    }
    if (dm_all) { from = 0; to = (int)a->ed.ntrk - 1; }
    else {
        if (vtrk < 0) { info("Select a track first."); return 0; }
        from = to = vtrk;
    }
    if (!a->ed.ntrk) return 0;
    app_ed_begin(a);
    for (t = to; t >= from; t--)
        n += dm_by_note ? jb_demix_notes(&a->ed, (uint32_t)t, chans, demix_label) : jb_demix_channels(&a->ed, (uint32_t)t);
    app_ed_commit(a);
    if (!n) app_undo(a);   /* nothing changed: no empty undo step */
    snprintf(msg, sizeof msg, n ? "Demix: %d new tracks (Undo takes them back)." : "Demix: nothing to split.", n);
    info(msg);
    return 1;
}

static int demix_handle(App *a)
{
    struct IntuiMessage *m;
    int changed = 0, close = 0;
    while (dmw && (m = GT_GetIMsg(dmw->UserPort))) {
        ULONG cls = m->Class;
        UWORD code = m->Code;
        struct Gadget *src = (struct Gadget *)m->IAddress;
        GT_ReplyIMsg(m);
        if (cls == IDCMP_CLOSEWINDOW) { close = 1; break; }
        if (cls == IDCMP_REFRESHWINDOW) { GT_BeginRefresh(dmw); GT_EndRefresh(dmw, TRUE); continue; }
        if (cls != IDCMP_GADGETUP) continue;
        if (src->GadgetID == 1) dm_all = code;
        else if (src->GadgetID == 2) { changed = demix_run(a); close = 1; }
        else if (src->GadgetID == 3) close = 1;
    }
    if (close) demix_close();
    return changed;
}

/* a pick of the Track menu */
static int menu_pick(App *a, int id)
{
    if (vtrk >= 0) edw_select_track(a, vtrk);
    switch (id) {
    case M_ROLL: pr_open(a); break;
    case M_LIST: if (vtrk >= 0) edw_events_open(a, vtrk); else info("Select a track first."); break;
    case M_STAFF: sw_open(a); break;
    case M_DRUMS: dw_open(a); break;
    case M_LYRICS: kw_open(a); break;
    case M_MIXER: a->want_mixer = 1; break;
    case M_XGM: {
        int ch = vtrk >= 0 ? ed_channel(&a->ed, (uint32_t)vtrk) : -1;
        if (ch >= 0) xw_open_part(a, ch); else xw_open(a);
        break;
    }
    case M_DEMIXCH: demix_open(a, 0); break;
    case M_DEMIXNOTE: demix_open(a, 1); break;
    case M_TRANSPOSE: jw_open_job(a, 0); break;
    case M_JOBS: jw_open(a); break;
    case M_SETL: case M_SETR: case M_GOL: case M_GOR: case M_LOOP: {
        uint32_t t = a->cur >= 0 ? xg_us_tick(&a->song, a->pos_us) : 0;
        if (a->cur < 0) break;
        if (id == M_SETL) a->loc_l = t;
        else if (id == M_SETR) a->loc_r = t;
        else if (id == M_GOL || id == M_GOR) a->locate_us = (long)xg_tick_us(&a->song, id == M_GOL ? a->loc_l : a->loc_r);
        else a->loop_on = !a->loop_on;
        tr_refresh(a);
        draw(a);
        break;
    }
    }
    return 0;
}

int tv_handle(App *a)
{
    struct IntuiMessage *m;
    int changed = 0, close = 0;
    if (dmw) changed |= demix_handle(a);
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
        case IDCMP_CLOSEWINDOW: a->want_cmd = APC_QUIT; break;
        case IDCMP_REFRESHWINDOW: GT_BeginRefresh(vw); GT_EndRefresh(vw, TRUE); draw(a); draw_toolbar(); break;
        case IDCMP_NEWSIZE: draw(a); draw_toolbar(); break;
        case IDCMP_MENUPICK: {
            UWORD c = code;
            while (c != MENUNULL && vmenu) {
                struct MenuItem *it = ItemAddress(vmenu, c);
                if (!it) break;
                menu_pick(a, (int)(LONG)GTMENUITEM_USERDATA(it));
                c = it->NextSelect;
            }
            break;
        }
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
                    static EdBlock blks[256];
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
            case V_SOUND:
                if (vtrk < 0) { info("Click a track first."); break; }
                menu_pick(a, M_XGM);
                break;
            case V_ROLL: case V_LIST: case V_DMCH: case V_DMNOTE:
                if (vtrk < 0) { info("Click a track first."); break; }
                menu_pick(a, src->GadgetID == V_ROLL ? M_ROLL : src->GadgetID == V_LIST ? M_LIST : src->GadgetID == V_DMCH ? M_DEMIXCH : M_DEMIXNOTE);
                break;
            }
            draw(a);
            break;
        case IDCMP_MOUSEBUTTONS:
            if (code == SELECTDOWN && my >= tby0 && my < tby0 + tbh) {
                int ib = toolbar_hit(mx);
                if (ib >= 0) {
                    info(ITIP[ib]);
                    if (vtrk >= 0 && a->cur >= 0) edw_select_track(a, vtrk);
                    switch (ib) {
                    case I_NEW: a->want_cmd = APC_NEW; break;
                    case I_OPEN: a->want_cmd = APC_OPEN; break;
                    case I_SAVE: a->want_cmd = APC_SAVE; break;
                    case I_SAVEAS: a->want_cmd = APC_SAVEAS; break;
                    case I_SONGS: a->want_cmd = APC_SONGS; break;
                    case I_UNDO: if (a->cur >= 0) { app_undo(a); info(a->info); nsel = 0; changed = 1; } break;
                    case I_LIST: if (vtrk >= 0) menu_pick(a, M_LIST); else edw_tracks_open(a); break;
                    case I_ROLL: menu_pick(a, M_ROLL); break;
                    case I_STAFF: menu_pick(a, M_STAFF); break;
                    case I_DRUM: menu_pick(a, M_DRUMS); break;
                    case I_MIXER: a->want_mixer = 1; break;
                    case I_XGM: menu_pick(a, M_XGM); break;
                    case I_LYRICS: menu_pick(a, M_LYRICS); break;
                    case I_LYRED: yw_open(a); break;
                    case I_TEXTS: lw_open(a); break;
                    case I_JOBS: menu_pick(a, M_JOBS); break;
                    case I_TRANSPORT: if (tr_is_open()) tr_close(); else tr_open(a); break;
                    }
                    if (a->cur < 0 && ib >= I_UNDO && ib != I_TRANSPORT && ib != I_MIXER) info("Load a song first: Open or Songs.");
                }
                break;
            }
            if (code == SELECTDOWN && my >= tb && my < gy0 && mx >= gx0 && mx <= gx1 && a->cur >= 0) {
                /* the ruler: to the beat there (Shift: L there, Alt: R there) */
                uint32_t t = tx(a, mx), bar, beat, tk;
                char m[80];
                ed_mbt(&a->ed, t, &bar, &beat, &tk);
                t = ed_tick_of(&a->ed, bar, beat, 0);
                if (qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) { a->loc_l = t; snprintf(m, sizeof m, "L = %lu:%lu", (unsigned long)bar, (unsigned long)beat); }
                else if (qual & (IEQUALIFIER_LALT | IEQUALIFIER_RALT)) { a->loc_r = t; snprintf(m, sizeof m, "R = %lu:%lu", (unsigned long)bar, (unsigned long)beat); }
                else {
                    a->locate_us = (long)xg_tick_us(&a->song, t);
                    a->pos_us = (uint32_t)a->locate_us;
                    snprintf(m, sizeof m, "Position %lu:%lu  (Shift-click: L, Alt-click: R)", (unsigned long)bar, (unsigned long)beat);
                }
                info(m);
                tr_refresh(a);
                draw(a);
                break;
            }
            if (code == SELECTDOWN && my >= gy0 && my <= gy1) {
                int r = (my - gy0) / rh + ttop, nb, k, hit = -1;
                static EdBlock blks[256];
                uint32_t t;
                if (r >= (int)a->ed.ntrk) break;
                vtrk = r;
                edw_select_track(a, r);
                if (mx < gx0) {
                    /* M / S in the track column */
                    WORD col = (WORD)((mx - cx0 - 2) / (vmono ? vmono->tf_XSize : 8));
                    if (col == 20) { a->ed.trk[r].solo = (uint8_t)!a->ed.trk[r].solo; app_track_mute(a); }
                    else if (col == 22) { a->ed.trk[r].mute = (uint8_t)!a->ed.trk[r].mute; app_track_mute(a); }
                    else if (col >= 24) {
                        /* the Sound: choose the program / bank of the track's channel */
                        int ch = ed_channel(&a->ed, (uint32_t)r);
                        if (ch >= 0) xw_open_part(a, ch);
                        else info("The track has no channel messages.");
                    }
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
                cur_tick = 0xFFFFFFFFUL;
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
            }
            break;
        }
    }
    if (close) tv_close();
    return changed;
}
