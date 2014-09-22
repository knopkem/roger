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
#define STATE_BUFFER_SIZE 1088
#define STATE_BUFFER_STRIDE 34
#define PIXEL_START_BIT 10  //zero based index
#define PIXEL_END_BIT   24  //zero based index

CONSTANT sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE  | CLK_FILTER_NEAREST;


void KERNEL run(read_only image2d_t R,
						 read_only image2d_t G, 
						 read_only image2d_t B,
						 read_only image2d_t A , const unsigned int  width, const unsigned int height) {

	// Red channel 

	// between one and 32 - zero value indicates that this code block is identically zero
	LOCAL int msbScratch[CODEBLOCKX];

	// state buffer
	// 
	LOCAL int state[STATE_BUFFER_SIZE];



	int2 posIn = (int2)(getLocalId(0) + getGlobalId(0)*CODEBLOCKX,  getGlobalId(1)*CODEBLOCKY);
	int maxVal = -2147483647-1;
	int index = BOUNDARY + getLocalId(0);

	//initialize pixels, and calculate column max
	for (int i = 0; i < CODEBLOCKY; ++i) {
	    int pixel = read_imagei(R, sampler, posIn).x;
		state[index] = pixel << PIXEL_START_BIT;
		maxVal = max(maxVal, pixel);
		index += STATE_BUFFER_STRIDE;	
		posIn.y++; 
	}
	//initialize boundary columns
	if (getLocalId(0) == 0 || getLocalId(0) == CODEBLOCKX-1) {
	    int delta = -1 + (getLocalId(0)/(CODEBLOCKX-1))*2; // -1 or +1
		int index = BOUNDARY + getLocalId(0) + delta;
		 for (int i = 0; i < CODEBLOCKY; ++i) {
		     state[index] = 0;
			 index += STATE_BUFFER_STRIDE;
		 }
	}

	int msbWI = 32 - clz(maxVal);
	msbScratch[getLocalId(0)] =msbWI;
	localMemoryFence();
	
	
	if (getLocalId(0) == 0) {
	    int4 mx = (int4)(msbWI);
		/*
		for(int i=0; i < CODEBLOCKX; i+=4) {
		    int4 temp = mx;
			mx = max(temp,(int4)(msbScratch[i],msbScratch[i+1],msbScratch[i+2],msbScratch[i+3]));
		}
		msbWI = mx.x;
		msbWI = max(msbWI, mx.y);
		msbWI = max(msbWI, mx.z);
		msbWI = max(msbWI, mx.w);
		*/
		msbScratch[0] = 8;
	}

	localMemoryFence();
}



