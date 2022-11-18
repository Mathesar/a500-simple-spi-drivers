/*
 *  SPI NET ENC28J60 test program
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

#include <exec/devices.h>
#include <exec/errors.h>
#include <exec/interrupts.h>

#include <dos/dos.h>
#include <dos/dostags.h>

#include <libraries/expansion.h>

#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/dos.h>

#include <hardware/custom.h>
#include <hardware/intbits.h>

#include <stdio.h>
#include <string.h>

#include "nic.h"
#include "spi.h"
#include "timer.h"
#include "vb_interrupt.h"

/*! Returns the minimum of two values */
#define MIN(a,b)            ((a) < (b) ? (a) : (b))






static void dump_packet(const uint8_t *buf, unsigned int length)
{
	unsigned int n, m;
	const uint8_t *ptr;

	for (n = 0; n < length; n+=16) {
		ptr = &buf[n];
		for (m = 0; m < MIN(16, length - n); m++) {
			printf("%02x ", ptr[m]);
		}
		for ( ; m < 16; m++) {
			printf("   ");
		}
		for (m = 0; m < MIN(16, length - n); m++) {
			printf("%c", (ptr[m] > 31) ? ptr[m] : '.');
		}
		printf("\n");
	}
}


int main(void)
{
	struct vb_interrupt_data_TYPE *interrupt;

	printf("ENC28J60 NIC test\n");

	/* Initialise hardware */
	if(spi_initialize(SPI_CHANNEL_2)>0)
		printf("SPI initialized\n");
	else
		printf("SPI initialisation failed\n");
		
	if(!nic_init())
		printf("NIC initialized\n");
	else
		printf("NIC initialisation failed\n");
	
	spi_set_speed(SPI_SPEED_FAST);
	
	//Start a VB interrupt server
	interrupt = Start_Vb_Interrupt(NULL, -1);
	if(interrupt == NULL)
		printf("Failed to install interrupt server\n");
	else
		printf("VB interrupt started\n");

	while (1) 
	{
		int len;
		ULONG sigs;
		static uint8_t rxbuf[1500];

		/* Wait for periodic VB interrupt */
		sigs = Wait(SIGBREAKF_CTRL_C | interrupt->signal_mask);

		if (sigs & SIGBREAKF_CTRL_C) 
		{
			break;
		}

		if(sigs & interrupt->signal_mask)
		{
			nic_keep_alive();

			
			do 
			{
				Forbid();
				len = nic_recv(rxbuf, sizeof(rxbuf));
				Permit();
				if (len >= 0) 
				{
					printf("RX %d bytes:\n", len);
					dump_packet(rxbuf, len);

					rxbuf[0] = rxbuf[6];
					rxbuf[1] = rxbuf[7];
					rxbuf[2] = rxbuf[8];
					rxbuf[3] = rxbuf[9];
					rxbuf[4] = rxbuf[10];
					rxbuf[5] = rxbuf[11];
					nic_get_mac_address(&rxbuf[6]);
					nic_send(rxbuf, len);
				}
			} 
			while (len >= 0);
		}
	}

	printf("Cleaning up\n");
	Stop_Vb_Interrupt(interrupt);


	return 0;
}
