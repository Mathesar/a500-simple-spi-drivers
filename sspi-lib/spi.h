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
 
#ifndef SPI_H_INCLUDED
#define SPI_H_INCLUDED

#include <exec/exec.h>

#define SSPI_BASE_ADDRESS	0x00EC0000

#define SPI_SPEED_SLOW 		0
#define SPI_SPEED_FAST 		1

#define SPI_CHANNEL_1		0x01
#define SPI_CHANNEL_2		0x02

#define SSPI_RESOURCE_NAME	"sspi"

struct sspi_resource_TYPE
{
	struct Node	node;
	UBYTE   pad1;
   UBYTE   pad2;
   UWORD   pad3;	    			
   UWORD   pad4;	    			
   UWORD   Version;	    	
   UWORD   Revision;	 
   struct SignalSemaphore semaphore;
	char name[sizeof(SSPI_RESOURCE_NAME)];
};

int spi_initialize(unsigned char channel);
void spi_shutdown();
void spi_set_speed(long speed);
void spi_obtain();
void spi_release();
void spi_select();
void spi_deselect();
void spi_read(__reg("a0") unsigned char *buf, __reg("d0") UWORD size);
void spi_write(__reg("a0") const unsigned char *buf, __reg("d0") UWORD size);

#endif
