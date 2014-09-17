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

/*

BPC Scan Pattern

Xx4 stripes

The stripes are scanned from top to bottom, and the columns within a
stripe are scanned from left to right.

*/


#define BOUNDARY 1


CONSTANT sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE  | CLK_FILTER_NEAREST;


void KERNEL run(read_only image2d_t idata, const unsigned int  width, const unsigned int height) {

    //find max bit plane number
	LOCAL char msb;
	LOCAL char msbScratch[CODEBLOCKX];

	msb = 0;							// between one and 32 - zero value indicates that this code block is identically zero
	msbScratch[getLocalId(0)] = 0;

	int2 posIn = (int2)(getLocalId(0) + getGlobalId(0)*CODEBLOCKX,  getGlobalId(1)*CODEBLOCKY);
	int4 max = -2147483647-1;
	for (int i = 0; i < CODEBLOCKX; ++i) {
		int4 temp = read_imagei(idata, sampler, posIn);	    
	}



	/*

	int4 pixelMsb = 32 - clz(val);  // between one and 32 - zero value indicates that this pixel has zero magnitude
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
	if (!msb)
		return;
*/

	

}



