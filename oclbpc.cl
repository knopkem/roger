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

Bit Plane Coder
===============

A. Scan Pattern

Xx4 stripes

The stripes are scanned from top to bottom, and the columns within a
stripe are scanned from left to right.

*/


#define CODEBLOCKY_QUARTER 8

#define BOUNDARY 1
#define BOUNDARY_X2 2

#define CODEBLOCKY_X4 128


#define STATE_BUFFER_SIZE 1156
#define STATE_BOTTOM_BOUNDARY_OFFSET 1124

#define STATE_BUFFER_STRIDE 34
#define STATE_BUFFER_STRIDE_X2 68
#define STATE_BUFFER_STRIDE_X3 102
#define STATE_BUFFER_STRIDE_X4 136

#define LEFT_TOP -35
#define TOP  -34
#define RIGHT_TOP -33

#define LEFT   -1
#define RIGHT   1

#define LEFT_BOTTOM  33
#define BOTTOM  34
#define RIGHT_BOTTOM  35


// bit positions (0 based indices)
#define INPUT_SIGN_BITPOS  15

#define SIGMA_NEW_BITPOS   0x1 
#define SIGMA_OLD_BITPOS   0x1
#define NBH_BITPOS         0x2
#define SIGMA_OLD_TO_NBH_SHIFT 0x1
#define RLC_BITPOS         0x4
#define PIXEL_START_BITPOS 0xA 
#define PIXEL_END_BITPOS   0x18 
#define SIGN_BITPOS        0x19 

#define SIGMA_NEW			 0x1		  //position 0
#define NOT_SIGMA_NEW        0xFFFFFFFE   // ~SIGMA_NEW
#define SIGMA_OLD			 0x10		  //position  1
#define NBH					 0x20		  //position  2
#define RLC					 0x80		  //position  4
#define RLC_D_POSITION		 0x380        //positions 6-9
#define PIXEL				 0x7FFF0000   //positions 10-24
#define SIGN				 0x2000000    //position  25

#define INPUT_TO_SIGN_SHIFT 10

CONSTANT sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE  | CLK_FILTER_NEAREST;

#define BIT(pix) (((pix)>>(bp))&1)

/**





**/

void KERNEL run(read_only image2d_t channel) {

	// state buffer
	LOCAL int state[STATE_BUFFER_SIZE];
	LOCAL int cxd[STATE_BUFFER_SIZE];


	///////////////////////////////////////////////////////////////////////////////////
	//1. Calculate MSB
	
	LOCAL int msbScratch[CODEBLOCKX]; // between one and 32 - zero value indicates that this code block is identically zero

	int maxSigBit;
	if (getLocalId(1) == 0) {
		int maxVal = 0;
		state[getLocalId(0)] = 0;   //top boundary
		LOCAL int* statePtr = state + (BOUNDARY + getLocalId(0));
		int2 posIn = (int2)(getGlobalId(0),  (getGlobalId(1) >> 3)*CODEBLOCKY);
		for (int i = 0; i < CODEBLOCKY; ++i) {
			int pixel = read_imagei(channel, sampler, posIn).x;
			pixel = (abs(pixel) << PIXEL_START_BITPOS) | ((pixel << INPUT_TO_SIGN_SHIFT) & SIGN);
			posIn.y++;
			maxVal = max(maxVal, (pixel>> PIXEL_START_BITPOS)&0x7FFF);
			statePtr[0] = pixel;
			statePtr += STATE_BUFFER_STRIDE;	
		}
		state[ getLocalId(0) + STATE_BOTTOM_BOUNDARY_OFFSET] = 0;		//bottom boundary

		//initialize full left and right boundary columns
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

	//calculate global msb
	if (!getLocalId(0) && !getLocalId(1) ) {
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
	localMemoryFence();

	if (msbScratch[0] == -1)
		return;

		

	int startIndex = (BOUNDARY + getLocalId(0)) + (BOUNDARY + getLocalId(1))*STATE_BUFFER_STRIDE_X4;

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

	// i) set sigma_new for strip column (sigma_old is zero)
	LOCAL int* statePtr = state + startIndex;
	for (int i = 0; i < 4; ++i) {
		int current = statePtr[0];
		current |= BIT(current);
		*statePtr = current;	
		statePtr += STATE_BUFFER_STRIDE;

	}
	localMemoryFence();
		
	// ii) set rlc nbh for strip column
	statePtr = state + startIndex;
	int current = statePtr[0];
	int top = statePtr[TOP];
	int leftTop = statePtr[LEFT_TOP];
	int left = statePtr[LEFT];
	int leftBottom = statePtr[LEFT_BOTTOM];

	int rlcCount = 0;               

	current |=  (BIT(top) | BIT(leftTop) | BIT(left) | BIT(leftBottom)) << NBH_BITPOS;
	statePtr[0] = current;

	// set doRLC flag
	bool doRLC = !(current & NBH) && !BIT(current);
			  
	if (doRLC) {
		rlcCount++;
	} else {
		if (current & NBH) {
			//ZC
		}
		if (BIT(current)) {
		    //SC
		}
	}


	for (int i = 0; i < 3; ++i) {

		statePtr += STATE_BUFFER_STRIDE;
		top = current;
		current = statePtr[0];
		leftTop = left;
		left = leftBottom;
		leftBottom = statePtr[LEFT_BOTTOM];

		// toggle doRLC flag
		doRLC = doRLC && !(current & NBH) && !BIT(current);
		if (doRLC) {
      
			rlcCount++;
		} else {
		   if (current & NBH) {
				//ZC
		   }
		   if (BIT(current)) {
		       //SC
		   }
		}


	}
	localMemoryFence();

	
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// 3. Processs rest of bit plans
	bp--;
	LOCAL char blockVote;

	while (bp >= PIXEL_START_BITPOS) {
		blockVote = 0;

		/////////////////////////////
		// 4. pre-process bit plane
		
		//Algorithm 15 Bit-plane Preprocessing
		//1: //First step
		//2: set sigma_old |= sigma_new
		//3: set nbh |=  old_sigma from neighbouring positions
		//4: if sigma_old = 0 AND nbh != 0 AND bit value = 1 then
		//5: set sigma_new := 1
		//6: end if
		//7: if there is some sigma_new = 1 then
		//8: blockVote := true
		//9: end if
		//10: //Second step
		//11: while blockVote = true do
		//12: set blockVote := false //reset the voting variable
		//13: set nbh :=  sigma_new from the four previous positions
		//14: if sigma_old = 0 AND nbh != 0 AND bit value = 1 then
		//15: set sigma_new = 1
		//16: end if
		//17: if there is some sigma_new = 1 found in this iteration then
		//18: set blockVote := true
		//19: end if
		//20: end while
		

		// i) migrate sigma_new to sigma_old, and clear sigma new bit
		statePtr = state + startIndex;
		for (int i = 0; i < 4; ++i) {
		    int current = *statePtr;
			current |= ((current) & SIGMA_NEW) << SIGMA_OLD_BITPOS;  
			current &= NOT_SIGMA_NEW;  // clear sigma new 
			*statePtr = current;
			statePtr += STATE_BUFFER_STRIDE;	

		}
		localMemoryFence();
		

		// ii) update nbh
		statePtr = state + startIndex;

		int top = statePtr[TOP];
		int leftTop = statePtr[LEFT_TOP];
		int rightTop = statePtr[RIGHT_TOP];
		int left = statePtr[LEFT];
		int current = statePtr[0];
		int right = statePtr[RIGHT];
		int leftBottom = statePtr[LEFT_BOTTOM];
		int bottom = statePtr[BOTTOM];
		int rightBottom = statePtr[RIGHT_BOTTOM];

		current |= ((leftTop & SIGMA_OLD) |
							( top & SIGMA_OLD) |
							( rightTop &  SIGMA_OLD) |
							( left & SIGMA_OLD) | 
							( right & SIGMA_OLD) | 
							( leftBottom & SIGMA_OLD) |
							( bottom & SIGMA_OLD) |
							( rightBottom & SIGMA_OLD)  ) << SIGMA_OLD_TO_NBH_SHIFT; 

		if ( BIT(current) && (current & NBH) && !(current & SIGMA_OLD) ) {
			current |= SIGMA_NEW;
			blockVote = 1;

		}
		*statePtr = current;

		for (int i = 0; i < 3; ++i) {
			statePtr += STATE_BUFFER_STRIDE;	
			top = current;
			leftTop = left;
			rightTop = right;
			left = leftBottom;
			current = statePtr[0];
			right = rightBottom;
			leftBottom = statePtr[LEFT_BOTTOM];
			bottom = statePtr[BOTTOM];
			rightBottom = statePtr[RIGHT_BOTTOM];

			current |= ((leftTop & SIGMA_OLD) |
							 ( top & SIGMA_OLD) |
							 ( rightTop &  SIGMA_OLD) |
							 ( left & SIGMA_OLD) | 
							 ( right & SIGMA_OLD) | 
							 ( leftBottom & SIGMA_OLD) |
							 ( bottom & SIGMA_OLD) |
							 ( rightBottom & SIGMA_OLD)  ) << SIGMA_OLD_TO_NBH_SHIFT; 

			if ( BIT(current) && (current & NBH) && !(current & SIGMA_OLD) ) {
			    current |= SIGMA_NEW;
				blockVote = 1;
			}
			*statePtr = current;

		}
		localMemoryFence();

		
		// iii) block vote on sigma new
#ifdef AMD
		while (blockVote) 
#endif
		{
		    blockVote = 0;
			localMemoryFence();

			statePtr = state + startIndex;

			int top = statePtr[TOP];
			int leftTop = statePtr[LEFT_TOP];
			int left = statePtr[LEFT];
			int current = statePtr[0];
			int leftBottom = statePtr[LEFT_BOTTOM];

			for (int i = 0; i < 3; ++i) {

				current  |= ((leftTop & SIGMA_NEW) |
							( top & SIGMA_NEW) |
							( left & SIGMA_NEW) | 
							( leftBottom & SIGMA_NEW) ) << NBH_BITPOS; 

				if (  BIT(current) && !(current & SIGMA_OLD) &&  (current & NBH)  && !(current & SIGMA_NEW)) {
					current |= SIGMA_NEW;
					blockVote = 1;

				}
				statePtr[0] = current;
				statePtr += STATE_BUFFER_STRIDE;
				top = current;
				current = statePtr[0];
				leftTop = left;
				left = leftBottom;
				leftBottom = statePtr[LEFT_BOTTOM];
			}
			current |= ((leftTop & SIGMA_NEW) |
						( top & SIGMA_NEW) |
						( left & SIGMA_NEW) ) << NBH_BITPOS; // ignore leftBottom pixel, because it is in the next
						                                     // stripe and it hasn't been processed yet

			if (  BIT(current) && !(current & SIGMA_OLD) &&  (current & NBH)  && !(current & SIGMA_NEW)) {
				current |= SIGMA_NEW;
				blockVote = 1;

			}
			statePtr[0] = current;
			localMemoryFence();

		}
		
		/////////////////////////
		// 5. Significance 
		statePtr = state + startIndex;

		top = statePtr[TOP];
		leftTop = statePtr[LEFT_TOP];
		rightTop = statePtr[RIGHT_TOP];
		left = statePtr[LEFT];
		current = statePtr[0];
		right = statePtr[RIGHT];
		leftBottom = statePtr[LEFT_BOTTOM];
		bottom = statePtr[BOTTOM];
		rightBottom = statePtr[RIGHT_BOTTOM];
		for (int i = 0; i < 4; ++i) {

			int nbh = ((leftTop & SIGMA_OLD) |
							 ( top & SIGMA_OLD) |
							 ( rightTop &  SIGMA_OLD) |
							 ( left & SIGMA_OLD) | 
							 ( right & SIGMA_OLD) | 
							 ( leftBottom & SIGMA_OLD) |
							 ( bottom & SIGMA_OLD) |
							 ( rightBottom & SIGMA_OLD)  ) << NBH_BITPOS; 

			current |= nbh;
			if ( !(current & NBH) && nbh && BIT(current) ) {
			    current |= SIGMA_NEW;

			}
			*statePtr = current;
			statePtr += STATE_BUFFER_STRIDE;	
			top = current;
			leftTop = left;
			rightTop = right;
			left = leftBottom;
			current = statePtr[0];
			right = rightBottom;
			leftBottom = statePtr[LEFT_BOTTOM];
			bottom = statePtr[BOTTOM];
			rightBottom = statePtr[RIGHT_BOTTOM];
		}
		localMemoryFence();

		//////////////////////////
		// 6. Magnitude Refinement
		statePtr = state + startIndex;

		top = statePtr[TOP];
		leftTop = statePtr[LEFT_TOP];
		rightTop = statePtr[RIGHT_TOP];
		left = statePtr[LEFT];
		current = statePtr[0];
		right = statePtr[RIGHT];
		leftBottom = statePtr[LEFT_BOTTOM];
		bottom = statePtr[BOTTOM];
		rightBottom = statePtr[RIGHT_BOTTOM];
		for (int i = 0; i < 4; ++i) {

			int nbh = ((leftTop & SIGMA_OLD) |
							 ( top & SIGMA_OLD) |
							 ( rightTop &  SIGMA_OLD) |
							 ( left & SIGMA_OLD) | 
							 ( right & SIGMA_OLD) | 
							 ( leftBottom & SIGMA_OLD) |
							 ( bottom & SIGMA_OLD) |
							 ( rightBottom & SIGMA_OLD)  ) << NBH_BITPOS; 

			current |= nbh;
			if ( !(current & NBH) && nbh && BIT(current) ) {
			    current |= SIGMA_NEW;

			}
			*statePtr = current;
			statePtr += STATE_BUFFER_STRIDE;	
			top = current;
			leftTop = left;
			rightTop = right;
			left = leftBottom;
			current = statePtr[0];
			right = rightBottom;
			leftBottom = statePtr[LEFT_BOTTOM];
			bottom = statePtr[BOTTOM];
			rightBottom = statePtr[RIGHT_BOTTOM];
		}
		localMemoryFence();

		///////////////////////
		// 7. cleanup
		statePtr = state + startIndex;

		top = statePtr[TOP];
		leftTop = statePtr[LEFT_TOP];
		rightTop = statePtr[RIGHT_TOP];
		left = statePtr[LEFT];
		current = statePtr[0];
		right = statePtr[RIGHT];
		leftBottom = statePtr[LEFT_BOTTOM];
		bottom = statePtr[BOTTOM];
		rightBottom = statePtr[RIGHT_BOTTOM];
		for (int i = 0; i < 4; ++i) {

			int nbh = ((leftTop & SIGMA_OLD) |
							 ( top & SIGMA_OLD) |
							 ( rightTop &  SIGMA_OLD) |
							 ( left & SIGMA_OLD) | 
							 ( right & SIGMA_OLD) | 
							 ( leftBottom & SIGMA_OLD) |
							 ( bottom & SIGMA_OLD) |
							 ( rightBottom & SIGMA_OLD)  ) << NBH_BITPOS; 

			current |= nbh;
			if ( !(current & NBH) && nbh && BIT(current) ) {
			    current |= SIGMA_NEW;

			}
			*statePtr = current;
			statePtr += STATE_BUFFER_STRIDE;	
			top = current;
			leftTop = left;
			rightTop = right;
			left = leftBottom;
			current = statePtr[0];
			right = rightBottom;
			leftBottom = statePtr[LEFT_BOTTOM];
			bottom = statePtr[BOTTOM];
			rightBottom = statePtr[RIGHT_BOTTOM];
		}
		localMemoryFence();

		bp--;
	}
}



