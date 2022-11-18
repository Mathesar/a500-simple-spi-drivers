/*
 *  Written by Niklas Ekström in July 2021.
 *
 *  adapted for use with A500 Simple SPI hardware by Dennis van Weeren (2022) 
 *
 *  Previous version of this file (device.c) used code written by
 *  Mike Sterling in 2018 for the SPI SD device driver for K1208/Amiga 1200.
 * 
 *  In order to handle sd card removal this file was rewritten almost entirely.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <proto/exec.h>
#include <clib/alib_protos.h>
#include <exec/types.h>
#include <exec/devices.h>
#include <exec/errors.h>
#include <exec/execbase.h>
#include <exec/interrupts.h>
#include <exec/ports.h>
#include <exec/tasks.h>
#include <libraries/dos.h>
#include <devices/timer.h>
#include <devices/trackdisk.h>
//#include <proto/exec.h>


#include "sd.h"
#include "spi.h"

/* START of name/id/version/revision
 * remember to also change VERSION constant in romtag.asm
 */ 
#define DEVICE_NAME		 		"sspisd.device"
#define DEVICE_ID_STRING		"sspisd 1.0 (7 July 2022)"
#define DEVICE_VERSION			1 /* also see romtag.asm */
#define DEVICE_REVISION			0
/* END of name/id/version/revision */


#define TASK_STACK_SIZE 		2048
#define TASK_PRIORITY 			11

#define DEBOUNCE_TIMEOUT_US 	100000

#define SIGB_CARD_CHANGE 		30
#define SIGB_OP_REQUEST 		29
#define SIGB_TIMER 				28

#define SIGF_CARD_CHANGE 		(1 << SIGB_CARD_CHANGE)
#define SIGF_OP_REQUEST 		(1 << SIGB_OP_REQUEST)
#define SIGF_OP_TIMER 			(1 << SIGB_TIMER)

#ifndef TD_GETGEOMETRY
// Needed to compile with AmigaOS 1.3 headers.
#define TD_GETGEOMETRY    (CMD_NONSTD+13)

struct DriveGeometry
{
    ULONG    dg_SectorSize;
    ULONG    dg_TotalSectors;
    ULONG    dg_Cylinders;
    ULONG    dg_CylSectors;
    ULONG    dg_Heads;
    ULONG    dg_TrackSectors;
    ULONG    dg_BufMemType;
    UBYTE    dg_DeviceType;
    UBYTE    dg_Flags;
    UWORD    dg_Reserved;
};
#endif

struct ExecBase *SysBase;
static BPTR saved_seg_list;
static struct timerequest tr;
static struct Task *task;
static struct MsgPort mp;
static struct MsgPort timer_mp;
static volatile BOOL card_present;
static volatile BOOL card_opened;
static volatile ULONG card_change_num;

static struct Interrupt *remove_int;
static struct IOStdReq *change_int;

char device_name[] = DEVICE_NAME;
char id_string[] = DEVICE_ID_STRING;

static uint32_t device_get_geometry(struct IOStdReq *ior)
{
    struct DriveGeometry *geom = (struct DriveGeometry*)ior->io_Data;
    const sd_card_info_t *ci = sd_get_card_info();

    if (ci->type == sdCardType_None)
        return TDERR_DiskChanged;

    geom->dg_SectorSize = 1 << ci->block_size;
    geom->dg_TotalSectors = ci->total_sectors; //ci->capacity >> ci->block_size;
    geom->dg_Cylinders = geom->dg_TotalSectors;
    geom->dg_CylSectors = 1;
    geom->dg_Heads = 1;
    geom->dg_TrackSectors = 1;
    geom->dg_BufMemType = MEMF_PUBLIC;
    geom->dg_DeviceType = 0; //DG_DIRECT_ACCESS;
    geom->dg_Flags = 1; //DGF_REMOVABLE;
    return 0;
}

static void handle_changed()
{
    // Wait to debounce the card detect switch.
    tr.tr_node.io_Command = TR_ADDREQUEST;
    tr.tr_time.tv_secs = 0;
    tr.tr_time.tv_micro = DEBOUNCE_TIMEOUT_US;
    DoIO((struct IORequest *)&tr);

    int res = 1;

    if (res == 1 && sd_open() == 0)
        card_opened = TRUE;
    else
        card_opened = FALSE;

    Forbid();
    card_present = res == 1;
    card_change_num++;
    Permit();

    if (remove_int)
        Cause(remove_int);

    if (change_int)
        Cause((struct Interrupt *)change_int->io_Data);
}

static void process_request(struct IOStdReq *ior)
{
    if (!card_present)
        ior->io_Error = TDERR_DiskChanged;
    else if (!card_opened)
        ior->io_Error = TDERR_NotSpecified;
    else
    {
        switch (ior->io_Command)
        {
        case TD_GETGEOMETRY:
            ior->io_Error = device_get_geometry(ior);
            break;

        case TD_FORMAT:
        case CMD_WRITE:
            if (sd_write((uint8_t *)ior->io_Data, ior->io_Offset >> SD_SECTOR_SHIFT, ior->io_Length >> SD_SECTOR_SHIFT) == 0)
                ior->io_Actual = ior->io_Length;
            else
                ior->io_Error = TDERR_NotSpecified;
            break;

        case CMD_READ:
            if (sd_read((uint8_t *)ior->io_Data, ior->io_Offset >> SD_SECTOR_SHIFT, ior->io_Length >> SD_SECTOR_SHIFT) == 0)
                ior->io_Actual = ior->io_Length;
            else
                ior->io_Error = TDERR_NotSpecified;
            break;
        }
    }

    ReplyMsg(&ior->io_Message);
}

static void task_run()
{
    if (card_present && sd_open() == 0)
        card_opened = TRUE;
     
    while (1)
    {
        ULONG sigs = Wait(SIGF_CARD_CHANGE | SIGF_OP_REQUEST);

        if (sigs & SIGF_CARD_CHANGE)
            handle_changed();

        if (sigs & SIGF_OP_REQUEST)
        {
            BOOL first = TRUE;

            struct IOStdReq *ior;
            while ((ior = (struct IOStdReq *)GetMsg(&mp)))
            {
                if (!first && (SetSignal(0, SIGF_CARD_CHANGE) & SIGF_CARD_CHANGE))
                    handle_changed();

                process_request(ior);
                first = FALSE;
            }
        }
    }
}

static void change_isr()
{
    Signal(task, SIGF_CARD_CHANGE);
}

static void begin_io(__reg("a6") struct Library *dev, __reg("a1") struct IOStdReq *ior)
{
    if (!ior)
        return;

    ior->io_Error = 0;
    ior->io_Actual = 0;

    switch (ior->io_Command)
    {
    case CMD_RESET:
    case CMD_CLEAR:
    case CMD_UPDATE:
    case TD_MOTOR:
    case TD_PROTSTATUS:
        break;

    case TD_CHANGESTATE:
        ior->io_Actual = card_present ? 0 : 1;
        break;

    case TD_CHANGENUM:
        ior->io_Actual = card_change_num;
        break;

    case TD_GETDRIVETYPE:
        ior->io_Actual = 0; //DG_DIRECT_ACCESS;
        break;

    case TD_REMOVE:
        remove_int = (struct Interrupt *)ior->io_Data;
        break;

    case TD_ADDCHANGEINT:
        if (change_int)
            ior->io_Error = IOERR_ABORTED;
        else
        {
            change_int = ior;
            ior->io_Flags &= ~IOF_QUICK;
            ior = NULL;
        }
        break;

    case TD_REMCHANGEINT:
        if (change_int == ior)
            change_int = NULL;
        break;

    case TD_GETGEOMETRY:
    case TD_FORMAT:
    case CMD_WRITE:
    case CMD_READ:
        PutMsg(&mp, (struct Message *)&ior->io_Message);
        ior->io_Flags &= ~IOF_QUICK;
        ior = NULL;
        break;

    default:
        ior->io_Error = IOERR_NOCMD;
    }

    if (ior && !(ior->io_Flags & IOF_QUICK))
        ReplyMsg(&ior->io_Message);
}

static ULONG abort_io(__reg("a6") struct Library *dev, __reg("a1") struct IORequest *ior)
{
    return IOERR_NOCMD;
}

static struct Library *init_device(__reg("a6") struct ExecBase *sys_base, __reg("a0") BPTR seg_list, __reg("d0") struct Library *dev)
{
    SysBase = *(struct ExecBase **)4;
    saved_seg_list = seg_list;

    dev->lib_Node.ln_Type = NT_DEVICE;
    dev->lib_Node.ln_Name = device_name;
    dev->lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    dev->lib_Version = DEVICE_VERSION;
    dev->lib_Revision = DEVICE_REVISION;
    dev->lib_IdString = (APTR)id_string;
    
    Forbid();

    tr.tr_node.io_Message.mn_Node.ln_Type = NT_REPLYMSG;
    tr.tr_node.io_Message.mn_ReplyPort = &timer_mp;
    tr.tr_node.io_Message.mn_Length = sizeof(tr);

    if (OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *)&tr, 0))
        goto fail1;

    task = CreateTask(device_name, TASK_PRIORITY, (char *)&task_run, TASK_STACK_SIZE);
    if (!task)
        goto fail2;

    int res = spi_initialize(SPI_CHANNEL_1);
    if (res < 0)
        goto fail3;

    card_present = res == 1;
	 
    mp.mp_Node.ln_Type = NT_MSGPORT;
    mp.mp_Flags = PA_SIGNAL;
    mp.mp_SigBit = SIGB_OP_REQUEST;
    mp.mp_SigTask = task;
    NewList(&mp.mp_MsgList);

    timer_mp.mp_Node.ln_Type = NT_MSGPORT;
    timer_mp.mp_Flags = PA_SIGNAL;
    timer_mp.mp_SigBit = SIGB_TIMER;
    timer_mp.mp_SigTask = task;
    NewList(&timer_mp.mp_MsgList);

    Permit();
    return dev;

fail3:
    DeleteTask(task);

fail2:
    CloseDevice((struct IORequest *)&tr);

fail1:
    Permit();
    FreeMem((char *)dev - dev->lib_NegSize, dev->lib_NegSize + dev->lib_PosSize);
    return NULL;
}

static BPTR expunge(__reg("a6") struct Library *dev)
{
    if (dev->lib_OpenCnt != 0)
    {
        dev->lib_Flags |= LIBF_DELEXP;
        return 0;
    }

    // This could be improved on.
    // There is a risk that the task has an outstanding debounce timer,
    // and deleting the task at that point will probably cause a crash.

    spi_shutdown();

    DeleteTask(task);

    CloseDevice((struct IORequest *)&tr);

    BPTR seg_list = saved_seg_list;
    Remove(&dev->lib_Node);
    FreeMem((char *)dev - dev->lib_NegSize, dev->lib_NegSize + dev->lib_PosSize);
    return seg_list;
}

static void open(__reg("a6") struct Library *dev, __reg("a1") struct IORequest *ior, __reg("d0") ULONG unitnum, __reg("d1") ULONG flags)
{
    ior->io_Error = IOERR_OPENFAIL;
    ior->io_Message.mn_Node.ln_Type = NT_REPLYMSG;

    if (unitnum != 0)
        return;

    dev->lib_OpenCnt++;
    ior->io_Error = 0;
}

static BPTR close(__reg("a6") struct Library *dev, __reg("a1") struct IORequest *ior)
{
    ior->io_Device = NULL;
    ior->io_Unit = NULL;

    dev->lib_OpenCnt--;

    if (dev->lib_OpenCnt == 0 && (dev->lib_Flags & LIBF_DELEXP))
        return expunge(dev);

    return 0;
}

static ULONG device_vectors[] =
{
    (ULONG)open,
    (ULONG)close,
    (ULONG)expunge,
    0,
    (ULONG)begin_io,
    (ULONG)abort_io,
    -1,
};

ULONG auto_init_tables[] =
{
    sizeof(struct Library),
    (ULONG)device_vectors,
    0,
    (ULONG)init_device,
};
