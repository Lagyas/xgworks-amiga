/* XGWorks Lite: all editor windows together for the main windows - their
 * signals, their messages, and refreshing the others after a change. */
#include <exec/types.h>
#include "editors.h"
#include "edwin.h"
#include "pianoroll.h"
#include "drumwin.h"
#include "lyrwin.h"
#include "xgmwin.h"
#include "lyredwin.h"
#include "staffwin.h"
#include "jobwin.h"
#include "trackview.h"

unsigned long eds_signal(void)
{
    return edw_signal() | pr_signal() | dw_signal() | lw_signal() | xw_signal() | yw_signal() | sw_signal() | jw_signal() | tv_signal();
}

void eds_refresh(App *a)
{
    edw_refresh(a);
    pr_refresh(a);
    dw_refresh(a);
    lw_refresh(a);
    xw_refresh(a);
    yw_refresh(a);
    sw_refresh(a);
    tv_refresh(a);
}

void eds_position(App *a, uint32_t pos_us, int playing)
{
    pr_position(a, pos_us, playing);
    sw_position(a, pos_us, playing);
}

int eds_handle(App *a, unsigned long sigs)
{
    int ch = 0;
    if (sigs & edw_signal()) ch |= edw_handle(a);
    if (sigs & pr_signal()) ch |= pr_handle(a);
    if (sigs & dw_signal()) ch |= dw_handle(a);
    if (sigs & lw_signal()) ch |= lw_handle(a);
    if (sigs & xw_signal()) ch |= xw_handle(a);
    if (sigs & yw_signal()) ch |= yw_handle(a);
    if (sigs & sw_signal()) sw_handle(a);
    if (sigs & jw_signal()) ch |= jw_handle(a);
    if (sigs & tv_signal()) ch |= tv_handle(a);
    if (ch) eds_refresh(a);
    return ch;
}

void eds_close(void)
{
    edw_close_all();
    pr_close();
    dw_close();
    lw_close();
    xw_close();
    yw_close();
    sw_close();
    jw_close();
    tv_close();
}
