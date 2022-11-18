;  Vertical Blank Interrupt server
;
;  Written by Dennis van Weeren
;
;  This program is free software: you can redistribute it and/or modify
;  it under the terms of the GNU General Public License as published by
;  the Free Software Foundation, either version 3 of the License, or
;  (at your option) any later version.
;
;  This program is distributed in the hope that it will be useful,
;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;  GNU General Public License for more details.
;
;  You should have received a copy of the GNU General Public License
;  along with this program.  If not, see <https://www.gnu.org/licenses/>.

        XDEF        _vb_interrupt_server
        CODE

               ; a1 = points to our data struct 
               ; d0 = scratch
               ; a6 = scratch

_vb_interrupt_server:
					move.l		4,a6				; Sysbase
					move.l		0(a1),d0			; signal mask
					move.l		4(a1),a1			; task handle
					jsr			-324(a6)			; call exec Signal()
					moveq.l		#0,d0				; set Z flag to indicate IRQ should be propagated further
					rts
						
               