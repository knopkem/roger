/*  Copyright 2014 Aaron Boxer

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. */


#include "ocl_platform.cl"

/*

Bit Plane Coder
===============

A. Scan Pattern

Xx4 stripes

The stripes are scanned from top to bottom, and the columns within a
stripe are scanned from left to right.

B. Serial State Variables

Pixels are stored in Sign + Magnitude format

sigma[y][x]			equal to 1 if at least one non-zero bit has been coded, otherwise zero

sigma_prime[y][x]   equal to 1 if magnitude refinement coding has been applied to this pixel, otherwise zero

eta[y][x]		    equal to 1 if zero coding (ZC) has been applied in SPP for this bit plane, otherwise zero

C. Concepts

Preferred Neighbourhood: at least one of the eight neighbours has sigma[y][x] == 1

Sign Coding (SC)
Zero Coding (ZC)
Magnitude Refinement Coding (MRC)
Run Length Encoding (RLC) (applied only to first pixel in stripe column)
	- if we are at the first pixel in the column, and all four pixels in column have sigma == 0 and eta == 0,
      then apply RLC to current pixel
	- occurrence of first 1 bit in column determines how RLC is coded

C. Serial Algorithm

Three pass per bit plane : Significance Propagation Pass (SPP)
                           Magnitude Refinement Pass	 (MRP)
						   Clean Up Pass				 (CUP)    

Only the CUP is performed on the most significant bit plane (MSB)

i. most significant bit plane is calculated

ii.  SPP
     // not significant, and in preferred nbhd
     if in preferred nbhd and sigma[y][x] == 0 then
	     ZC
		 eta[y][x] = 1
		 if (encoded bit == 1) then
			SC
			sigma[y][x] = 1
		 end if
	end if


iii. MRP
     // significant, but ZC has not been performed in SPP
	`if (sigma[y][x] == 1 and eta[y][x] == 0 then
		MRC
		sigma_prime = 1
	end if

iv. CUP
    // not significant, and not in preferred nbhd
	if sigma[y][x] == 0 and eta[y][x] == 0 then
		if pixel meets RLC conditions then
		   RLC
		else
		   ZC
		endif
*/


#define CODEBLOCKY_QUARTER 8

#define BOUNDARY 1
#define BOUNDARY_X2 2

#define CODEBLOCKY_X4 128


//////////////////////////////////////////////////////
// State Buffer

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


////////////////////////////////////////////////////
// State Variables 

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

// bit flags
#define SIGMA_NEW_F			 0x1		  //position 0
#define SIGMA_OLD_F			 0x10		  //position  1
#define NBH_F				 0x20		  //position  2
#define RLC_F				 0x80		  //position  4
#define RLC_D_POSITION_F	 0x380        //positions 6-9
#define PIXEL_F				 0x7FFF0000   //positions 10-24
#define SIGN_F				 0x2000000    //position  25

#define NOT_SIGMA_NEW_F      0xFFFFFFFE   // ~SIGMA_NEW
#define SIGMA_OLD_AND_NEW_F  0x11

#define INPUT_TO_SIGN_SHIFT 10

#define BIT(pix) (((pix)>>(bp))&1)
#define NBH(pix) ((pix) & NBH_F)
#define SIGMA_OLD(pix) ((pix) & SIGMA_OLD_F)
#define SIGMA_NEW(pix) ((pix) & SIGMA_NEW_F)
#define SIGMA_OLD_AND_NEW(pix) ((pix) & SIGMA_OLD_AND_NEW_F)
#define SIGN(pix) ((pix) & SIGN_F)

#define SET_SIGMA_NEW(pix) ( (pix) |= SIGMA_NEW_F )
#define CLEAR_SIGMA_NEW(pix) ( (pix) &= NOT_SIGMA_NEW_F )


/////////////////////////////////////
// Context Variables 

/*
0-2		(CX,D) pairs counter
3		SPP flag
4		MRP flag
5		SPP flag
8		D4
9-13	CX4
14		D3
15-19	CX3
20		D2
21-25	CX2
26		D1
27-31	CX1

*/

/////////////////////////////////////////////////////

// TABLES
/*

// ZERO CODING /////////////////////////////////////

// LL and LH sub-bands
values					Context
sumH	sumV	sumD	CX
2		x		x		8
1 		1		x		7
1		0 		1		6
1		0		0		5
0		2		x		4	
0		1		x		3
0		0 		2		2
0		0		1		1
0		0		0		0


//HH sub-band

values					Context
sum(H + V )		sumD	CX
2 				3		8
1				2		7
1				2		6
1				1		5
0				1		4
0				1		3
0				0		2
0				0		1
0				0		0

// HL sub-band

 values Context
sumH	sumV	sumD	CX
x		2		x		8
1		1		x		7
0		1 		1		6
0		1		0		5
2		0		x		4
1		0		x		3
0		0 		2		2
0		0		1		1
0		0		0		0


// SIGN CODING ///////////////////////

H	V	X^	 CX
1	1	0	 13
1	0	0	 12
1	-1	0	 11
0	1	0	 10
0	0	0	 9
0	-1	1	 10
-1	1	1	 11
-1	0	1	 12
-1 -1	1	 13


// Magnitude Refinement //////////////////


sigm_prime [y; x]	(H + V + D)	CX
1					x			16
0 					1			15
0					0			14


*/

CONSTANT sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE  | CLK_FILTER_NEAREST;

void KERNEL run(read_only image2d_t channel) {

	// state buffer
	LOCAL uint state[STATE_BUFFER_SIZE];
	LOCAL uint cxd[STATE_BUFFER_SIZE];


	///////////////////////////////////////////////////////////////////////////////////
	//1. Calculate MSB
	
	LOCAL int msbScratch[CODEBLOCKX]; // between -1 and 31 - negative one value indicates that this code block is identically zero

	int maxSigBit;
	if (getLocalId(1) == 0) {

		uint maxVal = 0;
		state[getLocalId(0)] = 0;   //top boundary
		LOCAL uint* statePtr = state + (BOUNDARY + getLocalId(0));
		int2 posIn = (int2)(getGlobalId(0),  (getGlobalId(1) >> 3)*CODEBLOCKY);

		for (int i = 0; i < CODEBLOCKY; ++i) {
			int pixel = read_imagei(channel, sampler, posIn).x;
			uint absPixel = abs(pixel);
			maxVal = max(maxVal, absPixel);
			pixel = (absPixel << PIXEL_START_BITPOS) | SIGN((pixel << INPUT_TO_SIGN_SHIFT));
			statePtr[0] = pixel;

			posIn.y++;
			statePtr += STATE_BUFFER_STRIDE;	
		}
		state[ getLocalId(0) + STATE_BOTTOM_BOUNDARY_OFFSET] = 0;		//bottom boundary

		//initialize full left and right boundary columns
		if (getLocalId(0) == 0 || getLocalId(0) == CODEBLOCKX-1) {
			int delta = -1 + ((getLocalId(0)/(CODEBLOCKX-1)) << 1); // -1 or +1
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

	////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. CUP on MSB

	uint bp = msbScratch[0] + PIXEL_START_BITPOS;

	// i) set sigma_new for stripe column (sigma_old is zero)
	LOCAL uint* statePtr = state + startIndex;
	for (int i = 0; i < 4; ++i) {
		int current = statePtr[0];
		current |= BIT(current);	// set sigma_new
		*statePtr = current;	
		statePtr += STATE_BUFFER_STRIDE;

	}
	localMemoryFence();
		
	// ii) set nbh for stripe column, and do CUP
	statePtr = state + startIndex;
	uint current			= statePtr[0];
	uint top				= statePtr[TOP];
	uint leftTop			= statePtr[LEFT_TOP];
	uint left				= statePtr[LEFT];
	uint leftBottom			= statePtr[LEFT_BOTTOM];

	int rlcCount = 0;               

	// first pixel in stripe column
	uint nbh =  SIGMA_NEW(top) | SIGMA_NEW(leftTop) | SIGMA_NEW(left) | SIGMA_NEW(leftBottom);
	bool doRLC = !nbh && !SIGMA_NEW(current);

	// next two pixels in stripe column
	for (int i = 0; i < 2; ++i) {

		statePtr += STATE_BUFFER_STRIDE;
		top = current;
		current		= statePtr[0];
		leftTop		= left;
		left		= leftBottom;
		leftBottom	= statePtr[LEFT_BOTTOM];

		nbh =  SIGMA_NEW(top) | SIGMA_NEW(leftTop) | SIGMA_NEW(left) | SIGMA_NEW(leftBottom);
		doRLC = doRLC && (!nbh && !SIGMA_NEW(current));
	}

	// last pixel in stripe column -
	// ignore leftBottom pixel, because it is in the next
	// stripe and it hasn't been processed yet
	statePtr += STATE_BUFFER_STRIDE;
	top			= current;
	current		= statePtr[0];
	leftTop		= left;
	left		= leftBottom;

	nbh =  SIGMA_NEW(top) | SIGMA_NEW(leftTop) | SIGMA_NEW(left);
	doRLC = doRLC && (!nbh && !SIGMA_NEW(current));

	if (doRLC) {
	    //RLC on first pixel
	}


	localMemoryFence();

	
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// 3. Processs rest of bit planes
	bp--;
	LOCAL char blockVote;

	while (bp >= PIXEL_START_BITPOS) {
		blockVote = 0;

		/////////////////////////////
		// 4. pre-process bit plane

		// i) migrate sigma_new to sigma_old, and clear sigma new bit
		statePtr = state + startIndex;
		for (int i = 0; i < 4; ++i) {
		    int current = *statePtr;
			current |=  SIGMA_NEW(current) << SIGMA_OLD_BITPOS;  
			CLEAR_SIGMA_NEW(current); 

			*statePtr = current;
			statePtr += STATE_BUFFER_STRIDE;	

		}
		localMemoryFence();
		

		// ii) update nbh
		statePtr = state + startIndex;

		uint top			= statePtr[TOP];
		uint leftTop		= statePtr[LEFT_TOP];
		uint rightTop	= statePtr[RIGHT_TOP];
		uint left		= statePtr[LEFT];
		uint current		= statePtr[0];
		uint right		= statePtr[RIGHT];
		uint leftBottom  = statePtr[LEFT_BOTTOM];
		uint bottom		= statePtr[BOTTOM];
		uint rightBottom = statePtr[RIGHT_BOTTOM];

		current |=  (   SIGMA_OLD(leftTop) |
						SIGMA_OLD( top) |
						SIGMA_OLD( rightTop) |
						SIGMA_OLD( left) | 
						SIGMA_OLD( right) | 
						SIGMA_OLD( leftBottom) |
						SIGMA_OLD( bottom) |
						SIGMA_OLD( rightBottom)  ) << SIGMA_OLD_TO_NBH_SHIFT; 

		if ( BIT(current) && NBH(current) && !SIGMA_OLD(current) ) {
			SET_SIGMA_NEW(current);
			blockVote = 1;

		}
		*statePtr = current;

		for (int i = 0; i < 3; ++i) {
			statePtr += STATE_BUFFER_STRIDE;
				
			top			 = current;
			leftTop		 = left;
			rightTop	 = right;
			left		 = leftBottom;
			current		 = statePtr[0];
			right		 = rightBottom;
			leftBottom   = statePtr[LEFT_BOTTOM];
			bottom		 = statePtr[BOTTOM];
			rightBottom  = statePtr[RIGHT_BOTTOM];

			current |=	   ( SIGMA_OLD(leftTop) |
							 SIGMA_OLD( top) |
							 SIGMA_OLD( rightTop) |
							 SIGMA_OLD( left) | 
							 SIGMA_OLD( right) | 
							 SIGMA_OLD( leftBottom) |
							 SIGMA_OLD( bottom) |
							 SIGMA_OLD( rightBottom)  ) << SIGMA_OLD_TO_NBH_SHIFT; 

			if ( BIT(current) && NBH(current) && !SIGMA_OLD(current) ) {
			    SET_SIGMA_NEW(current);
				blockVote = 1;
			}
			*statePtr = current;

		}
		localMemoryFence();

		
		// iii) block vote on sigma new
		while (blockVote) 
		{
		    blockVote = 0;
			localMemoryFence();

			statePtr = state + startIndex;

			//first pixel in stripe column
			uint top			= statePtr[TOP];
			uint leftTop		= statePtr[LEFT_TOP];
			uint left			= statePtr[LEFT];
			uint current		= statePtr[0];
			uint leftBottom		= statePtr[LEFT_BOTTOM];

			current  |= (   SIGMA_NEW(leftTop) |
							SIGMA_NEW( top) |
							SIGMA_NEW( left) | 
							SIGMA_NEW( leftBottom) ) << NBH_BITPOS; 

			if (  BIT(current) && !SIGMA_OLD(current) &&  NBH(current)  &&  !SIGMA_NEW(current)) {
				SET_SIGMA_NEW(current);
				blockVote = 1;

			}
			statePtr[0] = current;

			// next two pixels in stripe column
			for (int i = 0; i < 2; ++i) {
				statePtr += STATE_BUFFER_STRIDE;

				top			= current;
				current		= statePtr[0];
				leftTop		= left;
				left		= leftBottom;
				leftBottom  = statePtr[LEFT_BOTTOM];

				current  |= (   SIGMA_NEW(leftTop) |
								SIGMA_NEW( top) |
								SIGMA_NEW( left) | 
								SIGMA_NEW( leftBottom) ) << NBH_BITPOS; 

				if (  BIT(current) && !SIGMA_OLD(current) && NBH(current)  && !SIGMA_NEW(current)) {
					SET_SIGMA_NEW(current);
					blockVote = 1;

				}
				statePtr[0] = current;
			}

			// last pixel in stripe column -
			// ignore leftBottom pixel, because it is in the next
			// stripe and it hasn't been processed yet
			statePtr += STATE_BUFFER_STRIDE;

			top			= current;
			current		= statePtr[0];
			leftTop		= left;
			left		= leftBottom;

			current |= ( SIGMA_NEW(leftTop) |
						 SIGMA_NEW( top) |
						 SIGMA_NEW( left) ) << NBH_BITPOS;
			if (  BIT(current) && !SIGMA_OLD(current) && NBH(current)  && !SIGMA_NEW(current)) {
				SET_SIGMA_NEW(current);
				blockVote = 1;
			}
			statePtr[0] = current;

			localMemoryFence();

		}
		
		/////////////////////////
		// 5. bit plane processing 
		statePtr = state + startIndex;

		top = statePtr[TOP];
		leftTop			= statePtr[LEFT_TOP];
		rightTop		= statePtr[RIGHT_TOP];
		left			= statePtr[LEFT];
		current			= statePtr[0];
		right			= statePtr[RIGHT];
		leftBottom		= statePtr[LEFT_BOTTOM];
		bottom			= statePtr[BOTTOM];
		rightBottom		= statePtr[RIGHT_BOTTOM];

		doRLC = false;
		rlcCount = 0;
		if (!doRLC && SIGMA_OLD(current)) {
			// MRC
		} else {
			int nbh = ( SIGMA_OLD_AND_NEW(leftTop) |
						SIGMA_OLD_AND_NEW( top) |
						SIGMA_OLD_AND_NEW( rightTop) |
						SIGMA_OLD_AND_NEW( left) | 
						SIGMA_OLD_AND_NEW( right) | 
						SIGMA_OLD_AND_NEW( leftBottom) |
						SIGMA_OLD_AND_NEW( bottom) |
						SIGMA_OLD_AND_NEW( rightBottom)  ) ; 

			doRLC = !nbh && !SIGMA_NEW(current);		
		}

		for (int i = 0; i < 3; ++i) {
			statePtr += STATE_BUFFER_STRIDE;	

			top			= current;
			leftTop		= left;
			rightTop	= right;
			left		= leftBottom;
			current		= statePtr[0];
			right		= rightBottom;
			leftBottom  = statePtr[LEFT_BOTTOM];
			bottom		= statePtr[BOTTOM];
			rightBottom = statePtr[RIGHT_BOTTOM];

			if (!doRLC && SIGMA_OLD(current)) {
				// MRC
			} else {
				int nbh = ( SIGMA_OLD_AND_NEW(leftTop) |
							SIGMA_OLD_AND_NEW( top) |
							SIGMA_OLD_AND_NEW( rightTop) |
							SIGMA_OLD_AND_NEW( left) | 
							SIGMA_OLD_AND_NEW( right) | 
							SIGMA_OLD_AND_NEW( leftBottom) |
							SIGMA_OLD_AND_NEW( bottom) |
							SIGMA_OLD_AND_NEW( rightBottom)  ) ; 

				doRLC = doRLC && !nbh && !SIGMA_NEW(current);	
			}
		}

		localMemoryFence();
		bp--;
	}
}