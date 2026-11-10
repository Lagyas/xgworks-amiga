/* XGWorks Lite: the MUI mixer window - 16 parts: sound name, program,
 * bank MSB / LSB, volume, pan, reverb, chorus, mute, solo; Save and Undo. */
#include <exec/types.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <string.h>
#include "mixwin.h"

#ifndef MAKE_ID
#define MAKE_ID(a, b, c, d) ((ULONG)(a) << 24 | (ULONG)(b) << 16 | (ULONG)(c) << 8 | (ULONG)(d))
#endif

/* IDs: MIXID_BASE + ch * 16 + column; columns as below */
enum { C_PROG, C_MSB, C_LSB, C_VOL, C_PAN, C_REV, C_CHO, C_MUTE, C_SOLO, C_N };
#define ID_SAVE (MIXID_BASE + 16 * 16)
#define ID_UNDO (ID_SAVE + 1)

static Object *mwin, *name[16], *ctl[16][C_N], *head;
static const int PARAM[C_N] = {MX_PROG, MX_MSB, MX_LSB, MX_VOL, MX_PAN, MX_REV, MX_CHO, -1, -1};

static Object *slider(int min, int max, int val)
{
    return SliderObject, MUIA_Numeric_Min, min, MUIA_Numeric_Max, max, MUIA_Numeric_Value, val, MUIA_Weight, 60, End;
}

static Object *check(void)
{
    return ImageObject, ImageButtonFrame, MUIA_InputMode, MUIV_InputMode_Toggle, MUIA_Image_Spec, MUII_CheckMark,
        MUIA_Image_FreeVert, TRUE, MUIA_Background, MUII_ButtonBack, MUIA_ShowSelState, FALSE, End;
}

void *mixmui_create(App *a, void *mui_app)
{
    Object *grid, *bsave, *bundo;
    static char nums[16][4];
    int c, k;
    (void)a;
    grid = ColGroup(C_N + 2),
        Child, Label("Part"), Child, Label("Sound"), Child, Label("Prog"), Child, Label("MSB"), Child, Label("LSB"),
        Child, Label("Volume"), Child, Label("Pan"), Child, Label("Reverb"), Child, Label("Chorus"), Child, Label("M"), Child, Label("S"),
    End;
    if (!grid) return NULL;
    for (c = 0; c < 16; c++) {
        Object *o;
        sprintf(nums[c], "%d", c + 1);
        DoMethod(grid, OM_ADDMEMBER, (ULONG)(o = TextObject, MUIA_Text_Contents, (ULONG)nums[c], MUIA_Text_PreParse, (ULONG)"\33r", End));
        DoMethod(grid, OM_ADDMEMBER, (ULONG)(name[c] = TextObject, TextFrame, MUIA_Background, MUII_TextBack, MUIA_Text_Contents, (ULONG)"", MUIA_Weight, 120, End));
        ctl[c][C_PROG] = slider(1, 128, 1);
        ctl[c][C_MSB] = slider(0, 127, 0);
        ctl[c][C_LSB] = slider(0, 127, 0);
        ctl[c][C_VOL] = slider(0, 127, 100);
        ctl[c][C_PAN] = slider(-64, 63, 0);
        ctl[c][C_REV] = slider(0, 127, 40);
        ctl[c][C_CHO] = slider(0, 127, 0);
        ctl[c][C_MUTE] = check();
        ctl[c][C_SOLO] = check();
        for (k = 0; k < C_N; k++) DoMethod(grid, OM_ADDMEMBER, (ULONG)ctl[c][k]);
    }
    mwin = WindowObject,
        MUIA_Window_Title, (ULONG)"XGWorks Lite - Mixer",
        MUIA_Window_ID, MAKE_ID('X', 'G', 'M', 'X'),
        WindowContents, VGroup,
            Child, head = TextObject, MUIA_Text_Contents, (ULONG)"", End,
            Child, ScrollgroupObject, MUIA_Scrollgroup_FreeHoriz, FALSE, MUIA_Scrollgroup_Contents, VirtgroupObject, Child, grid, End, End,
            Child, HGroup,
                Child, bsave = SimpleButton("_Save into the song"),
                Child, bundo = SimpleButton("_Undo: the song's values"),
            End,
        End,
    End;
    if (!mwin) return NULL;
    DoMethod((Object *)mui_app, OM_ADDMEMBER, (ULONG)mwin);
    DoMethod(mwin, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)mwin, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    DoMethod(bsave, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)mui_app, 2, MUIM_Application_ReturnID, ID_SAVE);
    DoMethod(bundo, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)mui_app, 2, MUIM_Application_ReturnID, ID_UNDO);
    for (c = 0; c < 16; c++)
        for (k = 0; k < C_N; k++)
            DoMethod(ctl[c][k], MUIM_Notify, k >= C_MUTE ? MUIA_Selected : MUIA_Numeric_Value, MUIV_EveryTime,
                     (ULONG)mui_app, 2, MUIM_Application_ReturnID, MIXID_BASE + c * 16 + k);
    return mwin;
}

void mixmui_open(int open)
{
    if (mwin) SetAttrs(mwin, MUIA_Window_Open, open, TAG_DONE);
}

static ULONG getv(Object *o, ULONG attr)
{
    ULONG v = 0;
    GetAttr(attr, o, &v);
    return v;
}

void mixmui_refresh(App *a)
{
    static char h[200];
    int c;
    if (!mwin) return;
    snprintf(h, sizeof h, "%s", a->cur >= 0 ? a->names[a->cur] : "No song loaded");
    SetAttrs(head, MUIA_Text_Contents, (ULONG)h, TAG_DONE);
    for (c = 0; c < 16; c++) {
        const uint8_t *v = a->mx.val[c];
        int k;
        SetAttrs(name[c], MUIA_Text_Contents, (ULONG)(c == 9 && v[MX_MSB] == 0 ? "Drums" : v[MX_MSB] == 127 ? "Drum kit" : mx_gm_names[v[MX_PROG] & 127]), TAG_DONE);
        for (k = 0; k < C_MUTE; k++) {
            int val = v[PARAM[k]];
            if (k == C_PROG) val += 1;
            if (k == C_PAN) val -= 64;
            SetAttrs(ctl[c][k], MUIA_NoNotify, TRUE, MUIA_Numeric_Value, val, TAG_DONE);
        }
        SetAttrs(ctl[c][C_MUTE], MUIA_NoNotify, TRUE, MUIA_Selected, a->mx.mute[c], TAG_DONE);
        SetAttrs(ctl[c][C_SOLO], MUIA_NoNotify, TRUE, MUIA_Selected, a->mx.solo[c], TAG_DONE);
        SetAttrs(name[c], MUIA_Disabled, !a->mx.used[c], TAG_DONE);
    }
}

int mixmui_handle(App *a, unsigned long id)
{
    int c, k;
    if (id == ID_SAVE) {
        app_save(a);
        mixmui_refresh(a);
        return 1;
    }
    if (id == ID_UNDO) {
        app_mix_reset(a);
        mixmui_refresh(a);
        return 1;
    }
    if (id < MIXID_BASE || id >= ID_SAVE) return 0;
    c = (int)(id - MIXID_BASE) / 16;
    k = (int)(id - MIXID_BASE) % 16;
    if (k >= C_N) return 1;
    if (k >= C_MUTE) {
        app_mute(a, c, (int)getv(ctl[c][C_MUTE], MUIA_Selected), (int)getv(ctl[c][C_SOLO], MUIA_Selected));
        return 1;
    }
    {
        int v = (int)(LONG)getv(ctl[c][k], MUIA_Numeric_Value);
        if (k == C_PROG) v -= 1;
        if (k == C_PAN) v += 64;
        app_mix(a, c, PARAM[k], v);
        if (k <= C_LSB) SetAttrs(name[c], MUIA_Text_Contents, (ULONG)(a->mx.val[c][MX_MSB] == 127 ? "Drum kit" : mx_gm_names[a->mx.val[c][MX_PROG] & 127]), TAG_DONE);
    }
    return 1;
}
