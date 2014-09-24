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


#define CODEBLOCKY_QUARTER 8

#define BOUNDARY 1
#define BOUNDARY_X2 2

#define CODEBLOCKY_X4 128


#define STATE_BUFFER_SIZE 1156
#define STATE_BOUNDARY_OFFSET 1124

#define STATE_BUFFER_STRIDE 34
#define STATE_BUFFER_STRIDE_X4 136

#define LEFT_TOP -35
#define TOP  -34
#define RIGHT_TOP -33

#define LEFT   -1
#define RIGHT   1

#define LEFT_BOTTOM  33
#define BOTTOM  34
#define RIGHT_BOTTOM  35


// bit numbers (0 based indices)
#define INPUT_SIGN_BITPOS  15
 
#define NBH_BITPOS         0x2
#define RLC_BITPOS         0x4
#define PIXEL_START_BITPOS 0xA 
#define PIXEL_END_BITPOS   0x18 
#define SIGN_BITPOS        0x19 

#define SIGMA_NEW			 0x1		  //0
#define SIGMA_OLD			 0x10		  //1
#define NBH					 0x20		  //2
#define RLC					 0x80		  //4
#define RLC_D_POSITION		 0x380        //6-9
#define PIXEL				 0x7FFF0000   //10-24
#define SIGN				 0x2000000    //25

#define INPUT_TO_SIGN_SHIFT 10



#define AMD



CONSTANT sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE  | CLK_FILTER_NEAREST;

#define BIT(pix) (((pix)>>(bp))&1)


void KERNEL run(read_only image2d_t R,
						 read_only image2d_t G, 
						 read_only image2d_t B,
						 read_only image2d_t A , const unsigned int  width, const unsigned int height) {

	// state buffer
	LOCAL int state[STATE_BUFFER_SIZE];

	///////////////////////////////////////////////////////////////////////////////////////////////////////
	//0. read data into LDS
	int2 posIn = (int2)(getGlobalId(0),  (getGlobalId(1)&7)*CODEBLOCKY + getLocalId(1) );
	LOCAL int* statePtr = state + BOUNDARY + getLocalId(0);
	for (int i = 0; i < 4; ++i) {
		int pixel = read_imagei(R, sampler, posIn).x;
		*statePtr = (abs(pixel) << PIXEL_START_BITPOS) | ((pixel << INPUT_TO_SIGN_SHIFT) & SIGN);
		posIn.y++;
		statePtr += STATE_BUFFER_STRIDE;

	}
	localMemoryFence();

	///////////////////////////////////////////////////////////////////////////////////
	//1. Calculate MSB
	// between one and 32 - zero value indicates that this code block is identically zero
	LOCAL int msbScratch[CODEBLOCKX];

	int maxSigBit;
	if (getLocalId(1) == 0) {
		int maxVal = 0;
		state[getLocalId(0)] = 0;   //boundary
		LOCAL int* statePtr = state + (BOUNDARY + getLocalId(0));
		for (int i = 0; i < CODEBLOCKY; ++i) {
			maxVal = max(maxVal, (*statePtr >> PIXEL_START_BITPOS)&0x7FFF);
			statePtr += STATE_BUFFER_STRIDE;	
		}
		state[ getLocalId(0) + STATE_BOUNDARY_OFFSET] = 0;		//boundary

		//initialize full boundary columns
		if (getLocalId(0) == 0 || getLocalId(0) == CODEBLOCKX-1) {
			int delta = -1 + (getLocalId(0)/(CODEBLOCKX-1))*2; // -1 or +1
			statePtr = state + BOUNDARY + getLocalId(0) + delta;
			 for (int i = 0; i < CODEBLOCKY+ BOUNDARY_X2; ++i) {
				 *statePtr = 0;
				 statePtr += STATE_BUFFER_STRIDE;
			 }
		}

		// calculate column-msb
		maxSigBit = 31 - clz(maxVal);
		msbScratch[getLocalId(0)] =maxSigBit;
	}
	localMemoryFence();
	if (getLocalId(1) == 0) {
		//calculate global msb
		if (getLocalId(0) == 0) {
			int4 mx = (int4)(maxSigBit);

			LOCAL int* scratchPtr = msbScratch;
			for(int i=0; i < CODEBLOCKX; i+=4) {
				mx = max(mx,(int4)(msbScratch[i],msbScratch[i+1],msbScratch[i+2],msbScratch[i+3]));
			}
			maxSigBit = mx.x;
			maxSigBit = max(maxSigBit, mx.y);
			maxSigBit = max(maxSigBit, mx.z);
			maxSigBit = max(maxSigBit, mx.w);
			msbScratch[0] = maxSigBit;
		}
	}
	localMemoryFence();
	if (msbScratch[0] == -1)
		return;



	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. CUP on MSB

	
	//Algorithm 13 Clean-up pass on the MSB
	//1: set sigmanew := bit value
	//2: set rlcNbh :=  SUM(sigmanew) of preceding column and upper position
	//3: if rlcNbh = 0 and y-dimension of current position is a multiple of 4
	//then
	//4: execute RLC operation
	//5: end if
	//6: if rlcNbh = 1 then
	//7: execute ZC operation
	//8: end if
	//9: if sigmanew = 1 then
	//10: execute SC operation
	//11: end if	
	

	int bp = msbScratch[0] + PIXEL_START_BITPOS;

	// sigma_new 
	statePtr = state + (BOUNDARY + getLocalId(0)) + (BOUNDARY + getLocalId(1))*STATE_BUFFER_STRIDE_X4;
	for (int i = 0; i < 4; ++i) {
		int current = *statePtr;
		*statePtr |= BIT(current);	 // set sigma_new
		statePtr += STATE_BUFFER_STRIDE;

	}
	localMemoryFence();

	
	// rlcNbh
	statePtr = state + (BOUNDARY + getLocalId(0)) + (BOUNDARY + getLocalId(1))*STATE_BUFFER_STRIDE_X4;
	int top = statePtr[TOP];
	int leftTop = statePtr[LEFT_TOP];
	int left = statePtr[LEFT];
	int leftBottom = statePtr[LEFT_BOTTOM];

	for (int i = 0; i < 4; ++i) {
		int nbh = BIT(top) + BIT(leftTop) + BIT(left) + BIT(leftBottom);
		nbh = ((nbh | (~nbh + 1)) >> 27) * NBH;  // NBH if non-zero, otherwise zero 
		*statePtr |= nbh;	 // set nbh
		statePtr += STATE_BUFFER_STRIDE;
		top = left;
		left = leftBottom;
		top = statePtr[TOP];
		leftBottom = statePtr[LEFT_BOTTOM];

	}
	localMemoryFence();
	
/*

	// run CUP coding on MSB
	statePtr = state + BOUNDARY + getLocalId(0);
	int y = 0;
    current = statePtr[0];
	int currentPlusOne = statePtr[STATE_BUFFER_STRIDE];
	for (int i = BOUNDARY; i < CODEBLOCKY+BOUNDARY; ++i) {
		 if (!(y&3) ) {
			//RLC


			// get information about both SIGMA_NEW and SIGMA_OLD from
			// left and right neighbours and current bit, 
			// and use this to update the current RLC bit
#ifdef POO
						
			statePtr[0] |= (( left & SIGMA_NEW ) ||
								(left & SIGMA_OLD ) ||
								  ( current & SIGMA_NEW ) ||
								   ( current & SIGMA_OLD ) ||
								    ( right   & SIGMA_NEW ) ||
								      ( right   & SIGMA_OLD )) << RLC_BITPOS ;

			localMemoryFence();

			
			// get information about bit value from the left neighbour and update current RLC bit
			statePtr[0] |= BIT (statePtr[LEFT]) << RLC_BITPOS;

			localMemoryFence();

			// get the information from RLC bits of successors
			atomic_or(statePtr + (y&0xfffffffc - y)*STATE_BUFFER_STRIDE, *statePtr & RLC );
			
			localMemoryFence();

			int nbh = 0;
#endif
		 } else {
			//ZC

		 }
		 if (BIT(current)) {
			//SC

		 }
		 statePtr += STATE_BUFFER_STRIDE;
		 y++;

	}
	localMemoryFence();
#endif
*/
}



