/*
 *  SPI SD device driver for K1208/Amiga 1200
 *
 *  Copyright (C) 2018 Mike Stirling
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

#include <stdio.h>

#include "sd.h"
#include "spi.h"


static void hexdump(const uint8_t *buf, unsigned int size)
{
	unsigned int n;

	for (n = 0; n < size; n++) {
		printf("%02X ", (unsigned int)buf[n]);
		if ((n & 31) == 31) {
			printf("\n");
		}
	}
	printf("\n");
}

int main(void)
{
	static uint8_t buf[SD_SECTOR_SIZE * 2];

	
	spi_initialize(SPI_CHANNEL_1);
	sd_open();

	printf("Read single sector\n");
	printf("%d\n", sd_read(buf, 0, 1));
	hexdump(buf, SD_SECTOR_SIZE);


	return 0;
}
