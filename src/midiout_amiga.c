/* MIDI output on AmigaOS 3.x: serial.device at 31250 baud (8N1, no
 * handshake, RAD_BOOGIE), timing from timer.device (UNIT_MICROHZ). */
#include <exec/types.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <devices/serial.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <string.h>
#include <stdio.h>
#include "midiout.h"

struct Device *TimerBase;

static struct MsgPort *ser_port, *tim_port;
static struct IOExtSer *ser;
static struct timerequest *tim;
static int ser_open, tim_open;
static struct timeval start;
static UBYTE buf[512];
static ULONG used;

int mo_open(const char *device, long unit, char *msg, int msglen)
{
    if (!(ser_port = CreateMsgPort()) || !(tim_port = CreateMsgPort())) {
        snprintf(msg, msglen, "no memory");
        return 0;
    }
    ser = (struct IOExtSer *)CreateIORequest(ser_port, sizeof(struct IOExtSer));
    tim = (struct timerequest *)CreateIORequest(tim_port, sizeof(struct timerequest));
    if (!ser || !tim) {
        snprintf(msg, msglen, "no memory");
        return 0;
    }
    if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)tim, 0)) {
        snprintf(msg, msglen, "timer.device cannot be opened");
        return 0;
    }
    tim_open = 1;
    TimerBase = tim->tr_node.io_Device;
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

void mo_put(const uint8_t *b, uint32_t n)
{
    while (n) {
        ULONG k = n < sizeof buf - used ? n : sizeof buf - used;
        memcpy(buf + used, b, k);
        used += k; b += k; n -= k;
        if (used == sizeof buf) mo_flush();
    }
}

void mo_close(void)
{
    if (ser_open) {
        mo_flush();
        /* let the last bytes leave before the port closes */
        ser->IOSer.io_Command = CMD_FLUSH;
        DoIO((struct IORequest *)ser);
        CloseDevice((struct IORequest *)ser);
    }
    if (tim_open) CloseDevice((struct IORequest *)tim);
    if (ser) DeleteIORequest((struct IORequest *)ser);
    if (tim) DeleteIORequest((struct IORequest *)tim);
    if (ser_port) DeleteMsgPort(ser_port);
    if (tim_port) DeleteMsgPort(tim_port);
    ser_open = tim_open = 0;
    ser = NULL; tim = NULL; ser_port = tim_port = NULL;
}

void mo_clock_start(void)
{
    GetSysTime(&start);
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
        return !(SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C);
    }
    SubTime(&target, &now);
    tim->tr_node.io_Command = TR_ADDREQUEST;
    tim->tr_time = target;
    SendIO((struct IORequest *)tim);
    sigs = Wait((1UL << tim_port->mp_SigBit) | SIGBREAKF_CTRL_C);
    if (sigs & SIGBREAKF_CTRL_C) {
        if (!CheckIO((struct IORequest *)tim)) AbortIO((struct IORequest *)tim);
        WaitIO((struct IORequest *)tim);
        return 0;
    }
    WaitIO((struct IORequest *)tim);
    return 1;
}
