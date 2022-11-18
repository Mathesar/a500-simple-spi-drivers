/*  SPI library for the Simple SPI controller
 *
 *  Written by Dennis van Weeren
 *  orginally based upon code by Mike Stirling
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
 

#include "spi.h"
#include <proto/exec.h>
#include <string.h>

//assembly functions
extern void spi_read_fast(__reg("a0") UBYTE *buf, __reg("d0") UWORD size, __reg("a1") UBYTE *port);
extern void spi_write_fast(__reg("a0") const UBYTE *buf, __reg("d0") UWORD size, __reg("a1") UBYTE *port);

//current speed setting
static long current_speed = SPI_SPEED_SLOW;

//SPI channel (chip_select) to use
static long current_channel = 0;

//pointer to CIAA register
static volatile UBYTE *cia_b_pra = (volatile UBYTE *)0xbfd000;

//pointer to the SSPI resource
struct sspi_resource_TYPE *sspi;

//bus status
UBYTE bus_taken;

//obtain the bus
void spi_obtain()
{
	if(!bus_taken)
	{
		ObtainSemaphore(&sspi->semaphore);	
		bus_taken = 1;
	}
}

//release the bus
void spi_release()
{
	if(bus_taken)
	{
		ReleaseSemaphore(&sspi->semaphore);
		bus_taken = 0;
	}
}

//select the channel (assert chip_select)
void spi_select()
{
	UBYTE *csport = (UBYTE *)SSPI_BASE_ADDRESS;
		
	//assert chipselect
	*csport = current_channel;	
}

//deselect the channel (de-assert chip_select)
void spi_deselect()
{
	UBYTE *csport = (UBYTE *)SSPI_BASE_ADDRESS;
		
	//de-assert chipselect
	*csport = 0;
}

//sets the speed of the SPI bus
void spi_set_speed(long speed)
{
	current_speed = speed;
}


// A slow SPI transfer takes 32 us (8 bits times 4us (250kHz))
// An E-cycle is 1.4 us.
static void wait_40_us()
{
	UBYTE tmp;
	for (int i = 0; i < 32; i++)
		tmp = *cia_b_pra;
}

//slowed down write
static void spi_write_slow(__reg("a0") const UBYTE *buf, __reg("d0") UWORD size)
{
	for (ULONG i = 0; i < size; i++)
	{
		spi_write_fast(buf++, 1, (UBYTE *)(SSPI_BASE_ADDRESS+1));
	}
}

//slowed down read
static void spi_read_slow(__reg("a0") UBYTE *buf, __reg("d0") UWORD size)
{	
	for (ULONG i = 0; i < size; i++)
	{
		spi_read_fast(buf++, 1, (UBYTE *)(SSPI_BASE_ADDRESS+1));
		wait_40_us();
	}
}

//read <size> bytes from the SPI bus into <buf>
void spi_read(__reg("a0") UBYTE *buf, __reg("d0") UWORD size)
{
	if (current_speed == SPI_SPEED_FAST)
		spi_read_fast(buf, size, (UBYTE *)(SSPI_BASE_ADDRESS+1));
	else
		spi_read_slow(buf, size);
}

//write <size> bytes from <buf> to the SPI bus
void spi_write(__reg("a0") const UBYTE *buf, __reg("d0") UWORD size)
{
	if (current_speed == SPI_SPEED_FAST)
		spi_write_fast(buf, size, (UBYTE *)(SSPI_BASE_ADDRESS+1));
	else
		spi_write_slow(buf, size);
}

//initialize SPI hardware, <channel> sets chipselect to use
int spi_initialize(unsigned char channel)
{
	//assert channel
	if(channel!=SPI_CHANNEL_1 && channel!=SPI_CHANNEL_2)
		return -1;
	
	//open sspi resource
	sspi = OpenResource(SSPI_RESOURCE_NAME);
		
	//create resource if it does not exist yet
	if(sspi == NULL)
	{			
		//allocate memory for resource
		sspi = AllocMem(sizeof(struct sspi_resource_TYPE), MEMF_PUBLIC|MEMF_CLEAR);
		if(sspi == NULL)
			return -1;
	
		//build resource
		memcpy(sspi->name,SSPI_RESOURCE_NAME,sizeof(SSPI_RESOURCE_NAME));
		InitSemaphore(&sspi->semaphore);
		sspi->node.ln_Type = NT_RESOURCE;
		sspi->node.ln_Pri = 0;
		sspi->node.ln_Name = sspi->name;
		sspi->Version = 1;
		sspi->Revision = 0;
	
		//add resource to the system
		AddResource(sspi); 	
	}		
	
	//set channel to use
	current_channel = channel;
	
	//we do not have the bus
	bus_taken = 0;
	
	//initial speed is slow
	current_speed = SPI_SPEED_SLOW;
			
	return 1;
}

//shutdown SPI bus
void spi_shutdown()
{
	//make sure we release the bus
	spi_deselect();
	spi_release();
}

