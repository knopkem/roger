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


void KERNEL run(write_only image2d_t R,
						 write_only image2d_t G, 
						 write_only image2d_t B,
						 write_only image2d_t A , const unsigned int  width, const unsigned int height) {

	// Red channel 

    //find maximum number of bits in code block
	LOCAL char msbScratch[CODEBLOCKX];

    // between one and 32 - zero value indicates that this code block is identically zero

	int2 posIn = (int2)(getLocalId(0) + getGlobalId(0)*CODEBLOCKX,  getGlobalId(1)*CODEBLOCKY);
	int maxVal = -2147483647-1;
	for (int i = 0; i < CODEBLOCKY; ++i) {
		maxVal = max(maxVal, read_imagei(R, sampler, posIn).x);	
		posIn.y++; 
	}

	char msbWI = 32 - clz(maxVal);
	msbScratch[getLocalId(0)] =msbWI;
	localMemoryFence();
	

	//group by twos
	if ( (getLocalId(0)&1) == 0) {
		msbWI = max(msbWI, msbScratch[getLocalId(0)+1]);
	}
	localMemoryFence();
	
	//group by fours
	if ( (getLocalId(0)&3) == 0) {
		msbWI = max(msbWI, msbScratch[getLocalId(0)+2]);
	}
	localMemoryFence();
	
	
	//group by eights
	if ( (getLocalId(0)&7) == 0) {
		msbWI = max(msbWI, msbScratch[getLocalId(0)+4]);
	}
	localMemoryFence();
	
	//group by 16ths
	if ( (getLocalId(0)&15) == 0) {
		msbWI = max(msbWI, msbScratch[getLocalId(0)+8]);
	}
	localMemoryFence();
	
	
	if (getLocalId(0) == 0) {
		msbScratch[0] = max(msbWI, msbScratch[16]);  //crashes here with access violation while reading location .....
	}
	localMemoryFence();
	

}



