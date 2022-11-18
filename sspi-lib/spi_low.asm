;  SPI library for the Simple SPI controller
;
;  Written by Dennis van Weeren
;  orginally based upon code by Niklas Ekstrom
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

        XDEF        _spi_read_fast
        XDEF        _spi_write_fast
        CODE


					; a0 = UBYTE *buf
					; a1 = pointer to I/O port
               ; d0 = UWORD size

_spi_write_fast:
					move.l	d1,-(a7)						;push on stack
					bra		.write_byte_start
					
.write_byte_loop:	
					move.b	(a0)+,d1						;get byte from buffer
										
					move.b	d1,(a1)						;shift out 8 bits
					add.w		d1,d1
					move.b	d1,(a1)
					add.w		d1,d1
					move.b	d1,(a1)
					add.w		d1,d1
					move.b	d1,(a1)
					add.w		d1,d1
					move.b	d1,(a1)
					add.w		d1,d1
					move.b	d1,(a1)
					add.w		d1,d1
					move.b	d1,(a1)
					add.w		d1,d1
					move.b	d1,(a1)				

.write_byte_start:
					dbra		d0,.write_byte_loop
					
 					move.l  	(a7)+,d1						;pop from stack
               rts                 
                
               
               
               
               
               
               

					; a0 = UBYTE *buf
					; a1 = pointer to I/O port
					; d0 = UWORD size

_spi_read_fast:
					move.l	d1,-(a7)						;push on stack

					move.w	a0,d1							;branch if address odd
					andi.w	#1,d1
					bne      .read_byte
					
.read_word:		
					move.w	d0,d1							;word loop counter = size/2
					lsr.w		#1,d1
					beq		.read_byte
										
					move.l	d2,-(a7)						;push on stack
					move.l	d3,-(a7)
					
					subq		#1,d1							;correct word counter for dbra

.read_word_loop:	
					move.b	(a1),d3						;get first byte in d3[15:8]
					add.w		d3,d3
					move.b	(a1),d3						
					add.w		d3,d3
					move.b	(a1),d3						
					add.w		d3,d3
					move.b	(a1),d3						
					add.w		d3,d3
					move.b	(a1),d3						
					add.w		d3,d3
					move.b	(a1),d3						
					add.w		d3,d3
					move.b	(a1),d3						
					add.w		d3,d3
					move.b	(a1),d3						
					add.w		d3,d3							
									
										
					move.b	(a1),d2						;get second byte in d2[7:0]
					add.w		d2,d2
					move.b	(a1),d2						
					add.w		d2,d2
					move.b	(a1),d2						
					add.w		d2,d2
					move.b	(a1),d2						
					add.w		d2,d2
					move.b	(a1),d2						
					add.w		d2,d2
					move.b	(a1),d2						
					add.w		d2,d2
					move.b	(a1),d2						
					add.w		d2,d2
					move.b	(a1),d2						
					lsr.w		#7,d2							
									
					move.b	d2,d3							;combine bytes into word
					move.w	d3,(a0)+						;write word to buffer
										
					dbra		d1,.read_word_loop
					
					andi.w	#1,d0							;size = size - (number of words * 2)
					
					move.l  	(a7)+,d3
 					move.l  	(a7)+,d2
				
.read_byte:	
					tst.w		d0								;branch if size=0					
					beq		.read_done
										
					move.b	(a1),d1						;shift in 8 bits
					add.w		d1,d1
					move.b	(a1),d1
					add.w		d1,d1
					move.b	(a1),d1
					add.w		d1,d1
					move.b	(a1),d1
					add.w		d1,d1
					move.b	(a1),d1
					add.w		d1,d1
					move.b	(a1),d1
					add.w		d1,d1
					move.b	(a1),d1
					add.w		d1,d1
					move.b	(a1),d1					
					lsr.w		#7,d1							;byte is now in d1[7:0]
					
               move.b	d1,(a0)+						;write byte to buffer
                          
					subq		#1,d0							;size--
					
					bne		.read_word
					
.read_done:					
 					move.l  	(a7)+,d1
               rts 