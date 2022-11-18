/*
 *  SPI NET ENC28J60 device driver for K1208/Amiga 1200
 *
 *  Copyright (C) 2018 Mike Stirling
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

#ifndef COMMON_COMMON_H_
#define COMMON_COMMON_H_


#include <stdint.h>
#include <stdbool.h>

#if DEBUG
#include <stdio.h>
#endif

#if DEBUG > 0
#define ERROR(...)		{ printf(__VA_ARGS__); }
#else
#define ERROR(...)
#endif

#if DEBUG > 1
#define INFO(...)			{ printf(__VA_ARGS__); }
#else
#define INFO(...)
#endif

#if DEBUG > 2
#define TRACE(...)		{ printf(__VA_ARGS__); }
#else
#define TRACE(...)
#endif

#define FUNCTION_TRACE		TRACE("%s\n", __FUNCTION__)


/* Various utility macros */

/*! Returns the minimum of two values */
#define MIN(a,b)			((a) < (b) ? (a) : (b))

/*! Returns the maximum of two values */
#define MAX(a,b)			((a) > (b) ? (a) : (b))

/*! Returns the number of elements in a pre-declared array */
#define ARRAY_SIZE(n)				(sizeof(n) / sizeof(n[0]))

/*! Swap the byte order of a 16-bit word */
#define SWAP16(a)			(((a) >> 8) | ((a) << 8))

/*! Swap the byte order of a 32-bit word */
#define SWAP32(a)			( (((a) >> 24) & 0x000000fful) | \
							  (((a) >> 8) & 0x0000ff00ul) | \
							  (((a) << 8) & 0x00ff0000ul) | \
							  (((a) << 24) & 0xff000000ul) )



#endif /* COMMON_COMMON_H_ */

