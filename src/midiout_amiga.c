/* MIDI output on AmigaOS 3.x: serial.device at 31250 baud (8N1, no
 * handshake, RAD_BOOGIE) or camd.library (link "out.<unit>"); timing from
 * timer.device (UNIT_MICROHZ). Must be opened by the task that sends. */
#include <exec/types.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <exec/memory.h>
#include <devices/serial.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <utility/tagitem.h>
#include <midi/camd.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <proto/camd.h>
#include <string.h>
#include <stdio.h>
#include "midiout.h"

struct Device *TimerBase;
struct Library *CamdBase;

static struct MsgPort *ser_port, *tim_port;
static struct IOExtSer *ser;
static struct timerequest *tim;
static int ser_open, tim_open;
static struct MidiNode *mnode;
static struct MidiLink *mlink;
static struct timeval start;
static UBYTE buf[512];
static ULONG used;

static int open_camd(long unit, char *msg, int msglen)
{
    static char loc[16];
    struct TagItem t1[4], t2[3];
    if (!(CamdBase = OpenLibrary((CONST_STRPTR)"camd.library", 0))) {
        snprintf(msg, msglen, "camd.library is not installed");
        return 0;
    }
    snprintf(loc, sizeof loc, "out.%ld", unit);
    t1[0].ti_Tag = MIDI_Name; t1[0].ti_Data = (ULONG)"XGWorks Lite";
    t1[1].ti_Tag = MIDI_MsgQueue; t1[1].ti_Data = 0;
    t1[2].ti_Tag = MIDI_SysExSize; t1[2].ti_Data = 0;
    t1[3].ti_Tag = TAG_DONE;
    if (!(mnode = CreateMidiA(t1))) {
        snprintf(msg, msglen, "CAMD: no MIDI node");
        return 0;
    }
    t2[0].ti_Tag = MLINK_Location; t2[0].ti_Data = (ULONG)loc;
    t2[1].ti_Tag = MLINK_Comment; t2[1].ti_Data = (ULONG)"XGWorks Lite out";
    t2[2].ti_Tag = TAG_DONE;
    if (!(mlink = AddMidiLinkA(mnode, MLTYPE_Sender, t2))) {
        snprintf(msg, msglen, "CAMD: %s cannot be linked", loc);
        return 0;
    }
    return 1;
}

int mo_open(const char *device, long unit, char *msg, int msglen)
{
    if (!(ser_port = CreateMsgPort()) || !(tim_port = CreateMsgPort())) {
        snprintf(msg, msglen, "no memory");
        return 0;
    }
    if (!(tim = (struct timerequest *)CreateIORequest(tim_port, sizeof(struct timerequest)))) {
        snprintf(msg, msglen, "no memory");
        return 0;
    }
    if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)tim, 0)) {
        snprintf(msg, msglen, "timer.device cannot be opened");
        return 0;
    }
    tim_open = 1;
    TimerBase = tim->tr_node.io_Device;
    if (!strcmp(device, "camd") || !strcmp(device, "CAMD")) return open_camd(unit, msg, msglen);
    if (!(ser = (struct IOExtSer *)CreateIORequest(ser_port, sizeof(struct IOExtSer)))) {
        snprintf(msg, msglen, "no memory");
        return 0;
    }
    ser->io_SerFlags = SERF_XDISABLED | SERF_RAD_BOOGIE;
    if (OpenDevice((CONST_STRPTR)device, unit, (struct IORequest *)ser, 0)) {
        snprintf(msg, msglen, "%s unit %ld cannot be opened (in use by another program?)", device, unit);
        return 0;
    }
    ser_open = 1;
    ser->io_Baud = 31250;
    ser->io_ReadLen = 8;
    ser->io_WriteLen = 8;
    ser->io_StopBits = 1;
    ser->io_SerFlags = SERF_XDISABLED | SERF_RAD_BOOGIE;
    ser->IOSer.io_Command = SDCMD_SETPARAMS;
    if (DoIO((struct IORequest *)ser)) {
        snprintf(msg, msglen, "%s does not take 31250 baud", device);
        return 0;
    }
    return 1;
}

void mo_flush(void)
{
    if (!used || !ser_open) { used = 0; return; }
    ser->IOSer.io_Command = CMD_WRITE;
    ser->IOSer.io_Data = buf;
    ser->IOSer.io_Length = used;
    DoIO((struct IORequest *)ser);
    used = 0;
}

static void put(const uint8_t *b, uint32_t n)
{
    while (n) {
        ULONG k = n < sizeof buf - used ? n : sizeof buf - used;
        memcpy(buf + used, b, k);
        used += k; b += k; n -= k;
        if (used == sizeof buf) mo_flush();
    }
}

void mo_msg(uint8_t status, const uint8_t *data, uint32_t n)
{
    if (mlink) {
        if (status == 0xF0) {
            /* CAMD takes the whole SysEx, F0 … F7 */
            UBYTE *x = (UBYTE *)AllocVec(n + 2, MEMF_ANY);
            if (x) {
                x[0] = 0xF0;
                memcpy(x + 1, data, n);
                if (!n || data[n - 1] != 0xF7) x[++n] = 0xF7;
                PutSysEx(mlink, x);
                FreeVec(x);
            }
        } else if (status < 0xF0) {
            ULONG m = (ULONG)status << 24;
            if (n > 0) m |= (ULONG)data[0] << 16;
            if (n > 1) m |= (ULONG)data[1] << 8;
            PutMidi(mlink, m);
        }
        return;
    }
    put(&status, 1);
    put(data, n);
}

void mo_close(void)
{
    if (ser_open) {
        mo_flush();
        CloseDevice((struct IORequest *)ser);
    }
    if (mlink) RemoveMidiLink(mlink);
    if (mnode) DeleteMidi(mnode);
    if (CamdBase) CloseLibrary(CamdBase);
    if (tim_open) CloseDevice((struct IORequest *)tim);
    if (ser) DeleteIORequest((struct IORequest *)ser);
    if (tim) DeleteIORequest((struct IORequest *)tim);
    if (ser_port) DeleteMsgPort(ser_port);
    if (tim_port) DeleteMsgPort(tim_port);
    ser_open = tim_open = 0;
    ser = NULL; tim = NULL; ser_port = tim_port = NULL;
    mlink = NULL; mnode = NULL; CamdBase = NULL;
    used = 0;
}

void mo_clock_start(void)
{
    GetSysTime(&start);
}

uint32_t mo_now_us(void)
{
    struct timeval now;
    GetSysTime(&now);
    SubTime(&now, &start);
    return now.tv_secs * 1000000UL + now.tv_micro;
}

int mo_wait_until(uint32_t us)
{
    struct timeval now, target;
    ULONG sigs;
    target.tv_secs = start.tv_secs + us / 1000000;
    target.tv_micro = start.tv_micro + us % 1000000;
    if (target.tv_micro >= 1000000) { target.tv_micro -= 1000000; target.tv_secs++; }
    GetSysTime(&now);
    if (CmpTime(&now, &target) <= 0) {   /* now >= target: no waiting */
        ULONG s = SetSignal(0, SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D);
        return (s & SIGBREAKF_CTRL_C) ? 0 : (s & SIGBREAKF_CTRL_D) ? 2 : 1;
    }
    SubTime(&target, &now);
    tim->tr_node.io_Command = TR_ADDREQUEST;
    tim->tr_time = target;
    SendIO((struct IORequest *)tim);
    sigs = Wait((1UL << tim_port->mp_SigBit) | SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D);
    if (sigs & (SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D)) {
        if (!CheckIO((struct IORequest *)tim)) AbortIO((struct IORequest *)tim);
        WaitIO((struct IORequest *)tim);
        return (sigs & SIGBREAKF_CTRL_C) ? 0 : 2;
    }
    WaitIO((struct IORequest *)tim);
    return 1;
}
