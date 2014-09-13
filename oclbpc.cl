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


CONSTANT sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE  | CLK_FILTER_NEAREST;

void KERNEL run(read_only image2d_t idata, const unsigned int  width, const unsigned int height, const unsigned int  codeblockX, const unsigned int codeblockY, const unsigned int precision) {

    //find max bit plane number
	LOCAL char msb;
	msb = 0;
	int2 posIn = (int2)(getLocalId(0) + getGlobalId(0)*codeblockX, getLocalId(1) + getGlobalId(1)*codeblockY);
	int4 val = read_imagei(idata, sampler, posIn);
	int4 pixelMsb = 32 - clz(val);  // between one and 32
	int currentBit = 32;            // between one and 32 

	// wait until all work items have unset msb
	localMemoryFence();

	while(!msb && currentBit) {  
	     if (pixelMsb.x == currentBit){
		    msb = currentBit;
		}
		// wait to see if any work item has set msb in this iteration
		localMemoryFence();

        currentBit--;
	}
	//now we know the msb for the x channel of this code block
	

}



