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
 

#include "vb_interrupt.h"
#include <exec/interrupts.h>
#include <proto/exec.h>
#include <hardware/intbits.h>

#include <string.h>

//assembly functions
extern void vb_interrupt_server(struct Task *task);

//Start a vertical blank interrupt server
//signals <task> with signal <sigbit> every VB interval
//if <task> == NULL, current task is used to signal
//if <sigbit> < 0, a signal is allocated
struct vb_interrupt_data_TYPE *Start_Vb_Interrupt(struct Task *task, LONG sigbit)
{	
	struct vb_interrupt_data_TYPE *vb_interrupt_data; 
		
	//allocate signal if not provided
	if(sigbit < 0)
	{
		sigbit = AllocSignal(-1);
   	if (sigbit < 0) 
   		return NULL;
   }
   
	//allocate memory for interrupt context
	vb_interrupt_data = AllocMem(sizeof(struct vb_interrupt_data_TYPE), MEMF_PUBLIC|MEMF_CLEAR);
	if(vb_interrupt_data == NULL)
		return NULL;
	   	   	
 	vb_interrupt_data->signal_mask = 1UL << sigbit;
 	
 	if(task==NULL)
 		vb_interrupt_data->task = FindTask(NULL);//task not defined, use current task
 	else
 		vb_interrupt_data->task = task;
  	memcpy(vb_interrupt_data->name, SSPI_NET_INTERRUPT_NAME, sizeof(SSPI_NET_INTERRUPT_NAME));
  
  	//install interrupt server   
  	vb_interrupt_data->interrupt.is_Node.ln_Type = NT_INTERRUPT;
  	vb_interrupt_data->interrupt.is_Node.ln_Pri = -10;
  	vb_interrupt_data->interrupt.is_Node.ln_Name = vb_interrupt_data->name;    
	vb_interrupt_data->interrupt.is_Data = (APTR)vb_interrupt_data;
  	vb_interrupt_data->interrupt.is_Code = vb_interrupt_server;
  	AddIntServer(INTB_VERTB, &vb_interrupt_data->interrupt);

	//success
	return vb_interrupt_data;
}

//Stop a vertical blank interrupt server
void Stop_Vb_Interrupt(struct vb_interrupt_data_TYPE *vb_interrupt_data)
{
	RemIntServer(INTB_VERTB, &vb_interrupt_data->interrupt);
	FreeMem(vb_interrupt_data,sizeof(struct vb_interrupt_data_TYPE));
}
