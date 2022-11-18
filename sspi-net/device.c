/*
 *  SPI NET ENC28J60 device driver for Amiga 500
 *
 *  original code by Mike Stirling (2018)
 *
 *  adapted for use with A500 Simple SPI hardware by Dennis van Weeren (2022) 
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

#include <string.h>

#include <exec/devices.h>
#include <exec/errors.h>
#include <exec/interrupts.h>

#include <dos/dos.h>
#include <dos/dostags.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <proto/alib.h>

#include <hardware/custom.h>
#include <hardware/intbits.h>

#include "nic.h"
#include "spi.h"
#include "timer.h"
#include "sana2.h"
#include "common.h"
#include "vb_interrupt.h"

/* START of name/id/version/revision
 * remember to also change VERSION constant in romtag.asm
 */ 
#define DEVICE_NAME		 		"sspinet.device"
#define DEVICE_ID_STRING		"sspinet 0.3 (8 November 2022)"
#define DEVICE_VERSION			0 /* also see romtag.asm */
#define DEVICE_REVISION			3
/* END of name/id/version/revision */


#define ETHERSPI_TASK_NAME        "sspinet"
#define ETHERSPI_TASK_PRIO        12
#define ETHERSPI_STACK_SIZE       2048


typedef BOOL (*etherspi_bmfunc_t)(__reg("a0") APTR dst, __reg("a1") APTR src, __reg("d0") LONG size);

typedef struct 
{
    struct MinNode        node;
    etherspi_bmfunc_t    copyfrom;
    etherspi_bmfunc_t    copyto;
} etherspi_buffer_funcs_t;

//make sure all members are aligned at longword boundaries
//for fast access on all cpu's
#pragma pack(push,4)
typedef struct 
{
	BPTR								saved_seg_list;
    struct Task							*handler_task;
    ULONG								tx_signal_mask;
    LONG								tx_signal;
    ULONG								rx_signal_mask;
    LONG								rx_signal;
    
    struct Device						*device;
    struct vb_interrupt_data_TYPE	 	*interrupt;

    volatile struct List				read_list;
    volatile struct List				write_list;

    struct SignalSemaphore				read_list_sem;
    struct SignalSemaphore				write_list_sem;

    etherspi_buffer_funcs_t				*bf;
    unsigned char						frame[NIC_MTU + sizeof(nic_eth_hdr_t)];
} etherspi_ctx_t;
#pragma pack(pop)

/*! Global device context allocated on device init */
static etherspi_ctx_t *ctx;

/* Library bases */
struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct Library *UtilityBase; 

/* Some text strings */
char device_name[] = DEVICE_NAME;
char id_string[] = DEVICE_ID_STRING;

static BOOL device_node_is_in_list(struct Node *node, struct List *list)
{
    struct Node *n;

    for (n = list->lh_Head; n; n = n->ln_Succ) {
        if (n == node) {
            return TRUE;
        }
    }
    return FALSE;
}

static void device_query(struct IOSana2Req *req)
{
    struct Sana2DeviceQuery *query;

    /* Answer device query message */
    query = req->ios2_StatData;
    query->DevQueryFormat = 0;
    query->DeviceLevel = 0;

    /* Some fields are not always present */
    if (query->SizeAvailable >= 18) {
        query->AddrFieldSize = NIC_MACADDR_SIZE * 8;
    }
    if (query->SizeAvailable >= 22) {
        query->MTU = NIC_MTU;
    }
    if (query->SizeAvailable >= 26) {
        query->BPS = NIC_BPS;
    }
    if (query->SizeAvailable >= 30) {
        query->HardwareType = S2WireType_Ethernet;
    }

    query->SizeSupplied = MIN(query->SizeAvailable, 30);
}

void __saveds device_task(void)
{
    UBYTE nic_keep_alive_interval = 0;
    
    while (1) 
	{
    	struct IOSana2Req *ios2;
        struct IORequest *ioreq;
        nic_eth_hdr_t *hdr = (nic_eth_hdr_t*)ctx->frame;
        etherspi_buffer_funcs_t *bf;
        int len;
        ULONG sigs;
        
        /* Wait for signals from driver and vertical blank interrupt server */
        sigs = Wait(ctx->rx_signal_mask | ctx->tx_signal_mask);
        
		if (sigs & ctx->tx_signal_mask) 
		{
            /* Signal from driver, send packets from write queue */
            do 
			{
                ObtainSemaphore(&ctx->write_list_sem);
                ios2 = (struct IOSana2Req*)RemHead((struct List*)&ctx->write_list);
                ioreq = (struct IORequest*)ios2;
                ReleaseSemaphore(&ctx->write_list_sem);

                if (ios2) 
				{
                    /* Assemble packet in buffer */
                    bf = (etherspi_buffer_funcs_t*)ios2->ios2_BufferManagement;

                    if (ioreq->io_Flags & SANA2IOF_RAW) 
                    {
                        /* Verbatim */
                        bf->copyfrom(ctx->frame, ios2->ios2_Data, ios2->ios2_DataLength);
                    } 
                    else 
                    {
                        /* Build header */
                        hdr->type = ios2->ios2_PacketType;
                        nic_get_mac_address(hdr->src);
                        memcpy(hdr->dest, ios2->ios2_DstAddr, NIC_MACADDR_SIZE);
                        bf->copyfrom(&hdr[1], ios2->ios2_Data, ios2->ios2_DataLength);
                    }

                    /* Write */
                    Forbid();
                    nic_send(ctx->frame, ios2->ios2_DataLength + ((ioreq->io_Flags & SANA2IOF_RAW) ? 0 : sizeof(nic_eth_hdr_t)));
                    Permit();

                    /* Reply to message */
                    ioreq->io_Error = 0;
                    ReplyMsg(&ioreq->io_Message);
                }
            } 
            while (ios2);
        }

		//keep the NIC alive
		if (sigs & ctx->rx_signal_mask) 
		{
			nic_keep_alive_interval++;
			nic_keep_alive_interval &= 0x3f;
			if(nic_keep_alive_interval == 0)
				nic_keep_alive();		  	
		}
        
        //check periodically for new packets
        if (sigs & ctx->rx_signal_mask) 
		{
			/* Signal from periodic vertical blank interrupt, poll for RX data */
            do 
            {
                Forbid();
                len = nic_recv(ctx->frame, NIC_MTU);
                Permit();

                if (len >= 0) 
                {
                    /* Packet received - search read list for read request of correct type */
                    ObtainSemaphore(&ctx->read_list_sem);
                    for (ios2 = (struct IOSana2Req*)ctx->read_list.lh_Head; ios2; ios2 = (struct IOSana2Req*)ios2->ios2_Req.io_Message.mn_Node.ln_Succ) 
                    {
                        if (ios2->ios2_PacketType == hdr->type) 
                        {
                            Remove((struct Node*)ios2);
                            break;
                        }
                    }
                    ReleaseSemaphore(&ctx->read_list_sem);
                    ioreq = (struct IORequest*)ios2;

                    if (ios2) 
                    {
                        int n;

                        /* Copy packet to io request buffer */
                        bf = (etherspi_buffer_funcs_t*)ios2->ios2_BufferManagement;
                        if (ioreq->io_Flags & SANA2IOF_RAW) 
                        {
                            /* Verbatim */
                            ios2->ios2_DataLength = len;
                            bf->copyto(ios2->ios2_Data, ctx->frame, ios2->ios2_DataLength);
                            ioreq->io_Flags = SANA2IOF_RAW;
                        } 
                        else 
                        {
                            /* Skip header */
                            ios2->ios2_DataLength = len - sizeof(nic_eth_hdr_t);
                            bf->copyto(ios2->ios2_Data, &hdr[1], ios2->ios2_DataLength);
                            ioreq->io_Flags = 0;
                        }

                        /* Extract ethernet header data */
                        memcpy(ios2->ios2_SrcAddr, hdr->src, NIC_MACADDR_SIZE);
                        memcpy(ios2->ios2_DstAddr, hdr->dest, NIC_MACADDR_SIZE);
                        ioreq->io_Flags |= SANA2IOF_BCAST;
                        for (n = 0; n < NIC_MACADDR_SIZE; n++) 
                        {
                            if (hdr->dest[n] != 0xff) 
                            {
                                ioreq->io_Flags &= ~SANA2IOF_BCAST;
                                break;
                            }
                        }
                        ios2->ios2_PacketType = hdr->type;
                        ioreq->io_Error = 0;

                        ReplyMsg(&ioreq->io_Message);
                    } 
                    else 
                    {
                        /* FIXME: No matching read request - place packet in orphan queue */
                    }
                }
            } 
            while (len >= 0);
        }
    }
}

static struct Device *init_device(__reg("a6") struct ExecBase *sys_base, __reg("a0") BPTR seg_list, __reg("d0") struct Device *device)
{
    /* Open Exec library */
    SysBase = *(struct ExecBase**)4l;

    device->dd_Library.lib_Node.ln_Type = NT_DEVICE;
    device->dd_Library.lib_Node.ln_Name = device_name;
    device->dd_Library.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    device->dd_Library.lib_Version = DEVICE_VERSION;
    device->dd_Library.lib_Revision = DEVICE_REVISION;
    device->dd_Library.lib_IdString = (APTR)id_string;
 
	/* Open other libraries */
    DOSBase = (void*)OpenLibrary("dos.library", 37);
    if (DOSBase == NULL) 
    {
        ERROR("dos.library not found\n");
        goto error;
    }
    
	UtilityBase = (void*)OpenLibrary("utility.library", 37);
    if (UtilityBase == NULL) 
    {
        ERROR("utility.library not found\n");
        goto error;
    }

    /* Allocate driver context */
    ctx = AllocMem(sizeof(etherspi_ctx_t), MEMF_PUBLIC | MEMF_CLEAR);
    if (ctx == NULL) 
    {
        ERROR("Memory allocation failed\n");
        goto error;
    }
    ctx->device = device;
    ctx->saved_seg_list = seg_list;

	/* Allocate signal bits */
	ctx->tx_signal = AllocSignal(-1);
	if (ctx->tx_signal < 0) 
	{
		ERROR("Failed to allocate TX signal bit\n");
		goto error;
	}
    ctx->tx_signal_mask = (1ul << ctx->tx_signal);
	
	ctx->rx_signal = AllocSignal(-1);
	if (ctx->rx_signal < 0) 
	{
		ERROR("Failed to allocate RX signal bit\n");
		goto error;
	}
    ctx->rx_signal_mask = (1ul << ctx->rx_signal);
	
    /* Initialise message lists and mutexes */
    NewList((struct List*)&ctx->read_list);
    NewList((struct List*)&ctx->write_list);
    InitSemaphore(&ctx->read_list_sem);
    InitSemaphore(&ctx->write_list_sem);

    /* Initialise hardware, select port 2, fast speed */
    spi_initialize(SPI_CHANNEL_2);
    spi_set_speed(SPI_SPEED_FAST);
    nic_init();

    /* Start receiver task */
	ctx->handler_task = CreateTask((char *)ETHERSPI_TASK_NAME, ETHERSPI_TASK_PRIO, (char *)device_task, ETHERSPI_STACK_SIZE);

	/* Register VB interrupt server */
	ctx->interrupt = Start_Vb_Interrupt(ctx->handler_task, ctx->rx_signal);	 
	if(ctx->interrupt == NULL)
	{
		ERROR("Failed to install interrupt server\n");
		goto error;
	}

    /* Return success */
    return device;

error:
    /* Clean up after failed open */
    if (UtilityBase) 
    {
        CloseLibrary((struct Library*)UtilityBase);
    }
    if (DOSBase) 
    {
        CloseLibrary((struct Library*)DOSBase);
    }
    return NULL;
}

static BPTR expunge(__reg("a6") struct Library *dev)
{
    BPTR seg_list;
    
    if (ctx) 
    {
        seg_list = ctx->saved_seg_list;
        
        /* stop VB interrupt server */
        if(ctx->interrupt)
       	   Stop_Vb_Interrupt(ctx->interrupt);
                
        /* stop handler task */
		if(ctx->handler_task)
    		DeleteTask(ctx->handler_task);		

        /* Free context memory */
        FreeMem(ctx, sizeof(etherspi_ctx_t));
        ctx = NULL;
    }
    
    /* stop spi */
    spi_shutdown();

    /* Clean up libs */
    if (UtilityBase) 
    {
        CloseLibrary((struct Library*)UtilityBase);
    }
    
    if (DOSBase) 
    {
        CloseLibrary((struct Library*)DOSBase);
    }
    
    return seg_list;
}

static void open(__reg("a6") struct Library *dev, __reg("a1") struct IORequest *ioreq, __reg("d0") ULONG unit, __reg("d1") ULONG flags)
{
    struct IOSana2Req *ios2 = (struct IOSana2Req*)ioreq;
    etherspi_buffer_funcs_t *bf;
    int err = IOERR_OPENFAIL;

    /* Only unit 0 supported */
    /* FIXME: This doesn't allow for multiple opens */
    if (unit == 0) 
    {
        /* Allocate buffer function pointers */
        bf = AllocVec(sizeof(etherspi_buffer_funcs_t), MEMF_CLEAR | MEMF_PUBLIC);
        if (bf) 
        {
            bf->copyfrom = (etherspi_bmfunc_t)GetTagData(S2_CopyFromBuff, 0UL, (struct TagItem*)ios2->ios2_BufferManagement);
            bf->copyto = (etherspi_bmfunc_t)GetTagData(S2_CopyToBuff, 0UL, (struct TagItem*)ios2->ios2_BufferManagement);
            ctx->bf = bf;
			dev->lib_OpenCnt++;

            /* Success */
            ios2->ios2_BufferManagement = bf;
            ioreq->io_Error = 0;
            ioreq->io_Unit = (struct Unit*)unit;
            ioreq->io_Device = ctx->device;
            err = 0;
        }
    }

    if (err) 
    {
        ioreq->io_Error = err;
        ioreq->io_Unit = NULL;
        ioreq->io_Device = NULL;
    }
    
    ioreq->io_Message.mn_Node.ln_Type = NT_REPLYMSG;
}

static BPTR close(__reg("a6") struct Library *dev, __reg("a1") struct IORequest *ioreq)
{
    ioreq->io_Unit = (struct Unit*)-1;
    ioreq->io_Device = (struct Device*)-1;

    if (ctx->bf) 
    {
        FreeVec(ctx->bf);
    }
    ctx->bf = NULL;
    
    dev->lib_OpenCnt--;
    
    return 0;
}

static void begin_io(__reg("a6") struct Library *dev, __reg("a1") struct IOStdReq *ioreq)
{
    struct IOSana2Req *ios2 = (struct IOSana2Req*)ioreq;

    if (ctx == NULL || ioreq == NULL) 
    {
        /* Driver not initialised */
        return;
    }

    ioreq->io_Error = S2ERR_NO_ERROR;
    ios2->ios2_WireError = S2WERR_GENERIC_ERROR;

    TRACE("ioreq=%lu\n", ioreq->io_Command);

    /* Do IO */
    switch (ioreq->io_Command) 
    {
	    case CMD_READ:
	        if (ios2->ios2_BufferManagement == NULL) 
	        {
	            ioreq->io_Error = S2ERR_BAD_ARGUMENT;
	            ios2->ios2_WireError = S2WERR_BUFF_ERROR;
	            break;
	        }

	        /* Enqueue read buffer (defer reply to task) */
	        ObtainSemaphore(&ctx->read_list_sem);
	        AddTail((struct List*)&ctx->read_list, (struct Node*)ios2);
	        ReleaseSemaphore(&ctx->read_list_sem);
	        ioreq->io_Flags &= ~SANA2IOF_QUICK;
	        ios2 = NULL;
	        break;

    	case S2_BROADCAST:
        	/* Update destination address for broadcast */
        	memset(ios2->ios2_DstAddr, 0xff, NIC_MACADDR_SIZE);
        	/* Fall through */
    	
    	case CMD_WRITE:
        	if (ios2->ios2_DataLength > NIC_MTU) 
        	{
            	ioreq->io_Error = S2ERR_MTU_EXCEEDED;
            	break;
        	}
        	if (ios2->ios2_BufferManagement == NULL) 
        	{
            	ioreq->io_Error = S2ERR_BAD_ARGUMENT;
            	ios2->ios2_WireError = S2WERR_BUFF_ERROR;
            	break;
        	}

        	/* Enqueue write buffer (defer reply to task) */
        	ObtainSemaphore(&ctx->write_list_sem);
        	AddTail((struct List*)&ctx->write_list, (struct Node*)ios2);
        	ReleaseSemaphore(&ctx->write_list_sem);
        	ioreq->io_Flags &= ~SANA2IOF_QUICK;
        	ios2 = NULL;

        	/* Signal handler task */
        	Signal((struct Task*)ctx->handler_task, ctx->tx_signal_mask);
        	break;

    	case S2_ONLINE:
    	case S2_OFFLINE:
    	case S2_CONFIGINTERFACE:
        	break;
    
    	case S2_GETSTATIONADDRESS:
        	nic_get_mac_address(ios2->ios2_SrcAddr);
        	nic_get_mac_address(ios2->ios2_DstAddr);
        	break;
        	
    	case S2_DEVICEQUERY:
        	device_query(ios2);
        	break;

    	case S2_ONEVENT:
    	case S2_TRACKTYPE:
    	case S2_UNTRACKTYPE:
    	case S2_GETTYPESTATS:
    	case S2_READORPHAN:
    	case S2_GETGLOBALSTATS:
    	case S2_GETSPECIALSTATS:
        	break;

    	/* All other commands are treated as unsupported SANA2 commands */
    	default:
        	ioreq->io_Error = IOERR_NOCMD;
        	ios2->ios2_WireError = S2WERR_GENERIC_ERROR;
        	break;
    }

    if (ios2) 
    {
        /* If request wasn't deferred to a task then reply now */
        if (ioreq->io_Flags & SANA2IOF_QUICK) 
        {
            ioreq->io_Message.mn_Node.ln_Type = NT_MESSAGE;
        } 
        else 
        {
            ReplyMsg(&ioreq->io_Message);
        }
    }
}

static ULONG abort_io(__reg("a6") struct Library *dev, __reg("a1") struct IORequest *ioreq)
{
    struct IOSana2Req *ios2 = (struct IOSana2Req *)ioreq;

    if (ioreq == NULL) 
    {
        return IOERR_NOCMD;
    }

    /* Remove this IO request from any lists to which it is attached */
    ObtainSemaphore(&ctx->read_list_sem);
    if (device_node_is_in_list((struct Node*)ioreq, (struct List*)&ctx->read_list)) 
    {
        Remove((struct Node*)ioreq);
    }
    ReleaseSemaphore(&ctx->read_list_sem);

    ObtainSemaphore(&ctx->write_list_sem);
    if (device_node_is_in_list((struct Node*)ioreq, (struct List*)&ctx->write_list)) 
    {
        Remove((struct Node*)ioreq);
    }
    ReleaseSemaphore(&ctx->write_list_sem);

    /* Clean up */
    ioreq->io_Error = IOERR_ABORTED;
    ios2->ios2_WireError = 0;
    ReplyMsg((struct Message*)ioreq);
    
    return ioreq->io_Error;
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
