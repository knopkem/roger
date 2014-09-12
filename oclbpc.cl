/*  Copyright 2014 Aaron Boxer

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. */


#include "ocl_platform.cl"

// simulate OpenCL 2.0 _any functionality
/*
void KERNEL test(void) {
  LOCAL char ControlBit;
  localMemoryFence();
   if(true)
      ControlBit |= 0x1;
   /// ...
   localMemoryFence();
   /// ...
   if(ControlBit) {

	} else {
		
	}
}
*/

void KERNEL run(read_only image2d_t idata, const unsigned int  width, const unsigned int height, const unsigned int  codeblockX, const unsigned int codeblockY) {





}



