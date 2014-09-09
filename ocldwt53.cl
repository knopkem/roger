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



/*


Lossless forward 5/3 discrete wavelet transform

Assumptions:

1) assume WIN_SIZE_X equals the number of work items in the work group
2) width and height are both even (will need to relax this assumption in the future)
3) data precision is 14 bits or less

*/

#include "ocl_platform.cl"

//////////////////////////
// dimensions of window
// WIN_SIZE_X	
// WIN_SIZE_Y  assume this equals number of work items in work group
///////////////////////////


//  scratch buffer (in local memory of GPU) where block of input image is stored.
//  All operations expect WIN_SIZE_Y threads.

 /**

Layout for scratch buffer

Odd and even rows are separated. (Generates less bank conflicts when using lifting scheme.)
All even rows are stored first, then all odd rows.

Left (even) boundary row
Even rows
Right (even) boundary row
Left (odd) boundary row
Odd rows
Right (odd) boundary row

 **/

#define BOUNDARY_Y 2

#define HORIZONTAL_STRIDE 64  // WIN_SIZE_Y/2 


// two vertical neighbours: pointer diff:
#define BUFFER_SIZE            512	// HORIZONTAL_STRIDE * WIN_SIZE_X


#define CHANNEL_BUFFER_SIZE     1024            // BUFFER_SIZE + BUFFER_SIZE

#define CHANNEL_BUFFER_SIZE_X2  2048   
#define CHANNEL_BUFFER_SIZE_X3  3072   

#define PIXEL_BUFFER_SIZE   4096

#define VERTICAL_ODD_TO_PREVIOUS_EVEN -512
#define VERTICAL_ODD_TO_NEXT_EVEN     -511


#define VERTICAL_EVEN_TO_PREVIOUS_ODD  511
#define VERTICAL_EVEN_TO_NEXT_ODD      512



///////////////////////////////////////////////////////////////////////

CONSTANT sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_MIRRORED_REPEAT  | CLK_FILTER_NEAREST;

inline int getCorrectedGlobalIdY() {
      return getGlobalId(1) - 2 * BOUNDARY_Y * getGroupId(1);
}


// read pixel from local buffer
int4 readPixel( LOCAL short*  restrict  src) {
	return (int4)(*src, *(src+CHANNEL_BUFFER_SIZE),  *(src+CHANNEL_BUFFER_SIZE_X2),  *(src+CHANNEL_BUFFER_SIZE_X3)) ;
}

//write pixel to column
inline void writePixel(int4 pix, LOCAL short*  restrict  dest) {
	*dest = pix.x;
	dest += CHANNEL_BUFFER_SIZE;
	*dest = pix.y;
	dest += CHANNEL_BUFFER_SIZE;
	*dest = pix.z;
	dest += CHANNEL_BUFFER_SIZE;
	*dest = pix.w;
}

// write row to destination
void writeRowToOutput(LOCAL short* restrict currentScratch, __write_only image2d_t odata,  __write_only image2d_t odataLL, int firstX, int outputY, int width, int halfWidth){

	int2 posOut = {firstX>>1, outputY};
	for (int j = 0; j < WIN_SIZE_X; j+=2) {
	
	    // even row
		
		//only need to check evens, since even point will be the first out of bound point
	    if (posOut.x >= halfWidth)
			break;

		write_imagei(odataLL, posOut,readPixel(currentScratch));

		// odd row
		currentScratch += HORIZONTAL_STRIDE ;
		posOut.x+= halfWidth;

		write_imagei(odata, posOut, readPixel(currentScratch));

		currentScratch += HORIZONTAL_STRIDE;
		posOut.x -= (halfWidth - 1);
	}
}


// initial scratch offset when transforming vertically
inline int getScratchOffset(){
   return (getLocalId(1)>> 1) + (getLocalId(1)&1) * BUFFER_SIZE;
}

// assumptions: width and height are both even
// (we will probably have to relax these assumptions in the future)
void KERNEL run(read_only image2d_t idata, write_only image2d_t odata,  __write_only image2d_t odataLL,
                       const unsigned int  width, const unsigned int  height, const unsigned int steps,
							const unsigned int  level, const unsigned int levels) {

	int inputY = getCorrectedGlobalIdY();
	int outputY = -1;
	if (inputY < height && inputY >= 0)
	    outputY = (inputY >> 1) + (inputY & 1)*( height >> 1);

    const unsigned int halfWidth = width >> 1;
	LOCAL short scratch[PIXEL_BUFFER_SIZE];
	const float xDelta = 1.0/(width-1);
	int firstX = getGlobalId(0) * (steps * WIN_SIZE_X);

	bool doP = false;
	bool doU = false;
	if (getLocalId(1)&1)
		doP = getLocalId(1) != WIN_SIZE_Y-1;
	 else
	    doU = (getLocalId(1) != 0) && (getLocalId(1) != WIN_SIZE_Y-2);
	bool writeRow = (getLocalId(1) >= BOUNDARY_Y) && ( getLocalId(1) < WIN_SIZE_Y - BOUNDARY_Y) && outputY != -1;
	
	//0. Initialize: fetch first pixel (and 2 top boundary pixels)

	// read -1 point
	float2 posIn = (float2)(firstX-1, inputY) /  (float2)(width-1, height-1);	
	int4 previous = read_imagei(idata, sampler, posIn);

	// read 0 point
	posIn.x += xDelta;
	int4 current = read_imagei(idata, sampler, posIn);

	// predict previous (odd)
	previous -= ( read_imagei(idata, sampler, (float2)((firstX - 2)*xDelta,posIn.y )) + current) >> 1;   
	for (int i = 0; i < steps; ++i) {

		// 1. read from source image, transform columns, and store in local scratch
		LOCAL short* currentScratch = scratch + getScratchOffset();
		for (int j = 0; j < WIN_SIZE_X; j+=2) {
	   
			///////////////////////////////////////////////////////////////////////////////////////////
			// fetch next two pixels, then transform and write current (odd) and last (even)  
			
			// read next (odd) point
			posIn.x += xDelta;
			if (posIn.x > 1 + xDelta)
				break;
			int4 next = read_imagei(idata, sampler, posIn);
	
			// read next plus one (even) point
			posIn.x += xDelta;
			int4 nextPlusOne = read_imagei(idata, sampler, posIn);

			// predict next (odd)
			// F.4, page 118, ITU-T Rec. T.800 final draft
			next -= (current + nextPlusOne) >> 1;  
	
			// update current (even)
			// F.3, page 118, ITU-T Rec. T.800 final draft
			// note 8 = 2 << 2, which we need to do because the data is left shifted by 2
			current += (previous + next + 8) >> 2; 

	

			//write current (even)
			writePixel(current, currentScratch);

			//write odd
			currentScratch += HORIZONTAL_STRIDE;
			writePixel(next, currentScratch);

			//advance scratch pointer
			currentScratch += HORIZONTAL_STRIDE;

			//update registers
			previous = next;
			current = nextPlusOne;
		}

		
		//4. transform horizontally
		currentScratch = scratch + getScratchOffset();	

		
		localMemoryFence();
		//odd columns (skip right odd boundary column)
		if ( doP) {
			for (int j = 0; j < WIN_SIZE_X; j++) {
				int4 currentOdd = readPixel(currentScratch);
				int4 prevEven = readPixel(currentScratch + VERTICAL_ODD_TO_PREVIOUS_EVEN);
				int4 nextEven = readPixel(currentScratch + VERTICAL_ODD_TO_NEXT_EVEN); 
				currentOdd -= ((prevEven + nextEven) >> 1);
				writePixel( currentOdd, currentScratch);
				currentScratch += HORIZONTAL_STRIDE;
			}
		}
		
		currentScratch = scratch + getScratchOffset();	
		localMemoryFence();
		//even columns (skip left and right even boundary columns)
		if ( doU  ) {
			for (int j = 0; j < WIN_SIZE_X; j++) {
				int4 currentEven = readPixel(currentScratch);
				int4 prevOdd = readPixel(currentScratch + VERTICAL_EVEN_TO_PREVIOUS_ODD);
				int4 nextOdd = readPixel(currentScratch + VERTICAL_EVEN_TO_NEXT_ODD); 
				currentEven += (prevOdd + nextOdd + 8) >> 2; // note 8 = 2 << 2, which we need to do because the data is left shifted by 2
				writePixel( currentEven, currentScratch);
				currentScratch += HORIZONTAL_STRIDE;
			}
		}
		localMemoryFence();

		//5. write local buffer column to destination image
		// (only write non-boundary columns that are within the image bounds)
		if (writeRow) {
			 if (inputY &1)
			   writeRowToOutput(scratch + getScratchOffset(), odata, odata, firstX, outputY, width, halfWidth);
			else
			   writeRowToOutput(scratch + getScratchOffset(), odata, odataLL, firstX, outputY, width, halfWidth);

		}
		// move to next step 
		firstX += WIN_SIZE_X;
	}
}

