/* XGWorks Lite: all editor windows together for the main windows - their
 * signals, their messages, and refreshing the others after a change. */
#include <exec/types.h>
#include "editors.h"
#include "edwin.h"
#include "pianoroll.h"
#include "drumwin.h"
#include "lyrwin.h"

unsigned long eds_signal(void)
{
    return edw_signal() | pr_signal() | dw_signal() | lw_signal();
}

void eds_refresh(App *a)
{
    edw_refresh(a);
    pr_refresh(a);
    dw_refresh(a);
    lw_refresh(a);
}

int eds_handle(App *a, unsigned long sigs)
{
    int ch = 0;
    if (sigs & edw_signal()) ch |= edw_handle(a);
    if (sigs & pr_signal()) ch |= pr_handle(a);
    if (sigs & dw_signal()) ch |= dw_handle(a);
    if (sigs & lw_signal()) ch |= lw_handle(a);
    if (ch) eds_refresh(a);
    return ch;
}

void eds_close(void)
{
    edw_close_all();
    pr_close();
    dw_close();
    lw_close();
}
