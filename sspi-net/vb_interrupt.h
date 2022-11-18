/*  Vertical Blank Interrupt support code
 *
 *  Written by Dennis van Weeren
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

#ifndef VB_INTERRUPT_H_INCLUDED
#define VB_INTERRUPT_H_INCLUDED

#include <exec/types.h>
#include <exec/interrupts.h>

#define SSPI_NET_INTERRUPT_NAME	"sspi-net"

#pragma pack(push,1)
struct vb_interrupt_data_TYPE
{
	ULONG					signal_mask;
	struct Task 		*task;
	struct Interrupt 	interrupt;
	char   				name[sizeof(SSPI_NET_INTERRUPT_NAME)];
};
#pragma pack(pop)

struct vb_interrupt_data_TYPE *Start_Vb_Interrupt(struct Task *task, LONG sigbit);
void Stop_Vb_Interrupt(struct vb_interrupt_data_TYPE *vb_interrupt_data);

#endif
