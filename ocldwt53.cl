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

//////////////////////////
// dimensions of window
// WIN_SIZE_X	assume this equals number of work items in work group
// WIN_SIZE_Y
///////////////////////////


//  scratch buffer (in local memory of GPU) where block of input image is stored.
//  All operations expect WIN_SIZE_X threads.

 /**

Layout for scratch buffer

Odd and even columns are separated. (Generates less bank conflicts when using lifting scheme.)
All even columns are stored first, then all odd columns.

Left (even) boundary column
Even Columns
Right (even) boundary column
Left (odd) boundary column
Odd Columns
Right (odd) boundary column

 **/

#define BOUNDARY_X 2

#define VERTICAL_STRIDE 64  // WIN_SIZE_X/2 


// two vertical neighbours: pointer diff:
#define BUFFER_SIZE            512	// VERTICAL_STRIDE * WIN_SIZE_Y


#define CHANNEL_BUFFER_SIZE     1024            // BUFFER_SIZE + BUFFER_SIZE

#define CHANNEL_BUFFER_SIZE_X2  2048   
#define CHANNEL_BUFFER_SIZE_X3  3072   

#define PIXEL_BUFFER_SIZE   4096

#define HORIZONTAL_ODD_TO_PREVIOUS_EVEN -512
#define HORIZONTAL_ODD_TO_NEXT_EVEN     -511


#define HORIZONTAL_EVEN_TO_PREVIOUS_ODD  511
#define HORIZONTAL_EVEN_TO_NEXT_ODD      512



///////////////////////////////////////////////////////////////////////

CONSTANT sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_MIRRORED_REPEAT  | CLK_FILTER_NEAREST;

inline int getCorrectedGlobalIdX() {
      return getGlobalId(0) - 2 * BOUNDARY_X * getGroupId(0);
}


// read pixel from local buffer
int4 readPixel( LOCAL int*  restrict  src) {
	return (int4)(*src, *(src+CHANNEL_BUFFER_SIZE),  *(src+CHANNEL_BUFFER_SIZE_X2),  *(src+CHANNEL_BUFFER_SIZE_X3)) ;
}

//write pixel to column
inline void writePixel(int4 pix, LOCAL int*  restrict  dest) {
	*dest = pix.x;
	dest += CHANNEL_BUFFER_SIZE;
	*dest = pix.y;
	dest += CHANNEL_BUFFER_SIZE;
	*dest = pix.z;
	dest += CHANNEL_BUFFER_SIZE;
	*dest = pix.w;
}

// write column to destination
void writeColumnToOutput(LOCAL int* restrict currentScratch, __write_only image2d_t odata, int firstY, int inputX, int height, int halfHeight){

	int2 posOut = {inputX, firstY>>1};
	for (int j = 0; j < WIN_SIZE_Y; j+=2) {
	
	    // even row
		
		//only need to check evens, since even point will be the first out of bound point
	    if (posOut.y >= halfHeight)
			break;

		write_imagei(odata, posOut,readPixel(currentScratch));

		// odd row
		currentScratch += VERTICAL_STRIDE ;
		posOut.y+= halfHeight;

		write_imagei(odata, posOut,readPixel(currentScratch));

		currentScratch += VERTICAL_STRIDE;
		posOut.y -= (halfHeight - 1);
	}
}

// initial scratch offset when transforming vertically
inline int getScratchOffset(){
   return (getLocalId(0)>> 1) + (getLocalId(0)&1) * BUFFER_SIZE;
}

// assumptions: width and height are both even
// (we will probably have to relax these assumptions in the future)
void KERNEL run(__read_only image2d_t idata, __write_only image2d_t odata,   
                       const unsigned int  width, const unsigned int  height, const unsigned int steps) {

	int inputX = getCorrectedGlobalIdX();

	int outputX = inputX;
	outputX = (outputX >> 1) + (outputX & 1)*( width >> 1);

	const int halfWinSizeX = WIN_SIZE_X >> 1;
    const unsigned int halfHeight = height >> 1;
	LOCAL int scratch[PIXEL_BUFFER_SIZE];
	const float yDelta = 1.0/(height-1);
	int firstY = getGlobalId(1) * (steps * WIN_SIZE_Y);
	
	//0. Initialize: fetch first pixel (and 2 top boundary pixels)

	// read -1 point
	float2 posIn = (float2)(inputX, firstY - 1) /  (float2)(width-1, height-1);	
	int4 minusOne = read_imagei(idata, sampler, posIn);

	// read 0 point
	posIn.y += yDelta;
	int4 current = read_imagei(idata, sampler, posIn);

	// transform -1 point (no need to write to local memory)
	minusOne -= ( read_imagei(idata, sampler, (float2)(posIn.x, (firstY - 2)*yDelta)) + current) >> 1;   
	for (int i = 0; i < steps; ++i) {

		// 1. read from source image, transform columns, and store in local scratch
		LOCAL int* currentScratch = scratch + getScratchOffset();
		for (int j = 0; j < WIN_SIZE_Y; j+=2) {
	   
			///////////////////////////////////////////////////////////////////////////////////////////
			// fetch next two pixels, then transform and write current (odd) and last (even)  
			
			// read current plus one (odd) point
			posIn.y += yDelta;
			if (posIn.y > 1 + yDelta)
				break;
			int4 currentPlusOne = read_imagei(idata, sampler, posIn);
	
			// read current plus two (even) point
			posIn.y += yDelta;
			int4 currentPlusTwo = read_imagei(idata, sampler, posIn);

			// transform current plus one (odd) point
			currentPlusOne -= (current + currentPlusTwo) >> 1;  // F.4, page 118, ITU-T Rec. T.800 final draft
	
			// transform current (even) point
			current += (minusOne + currentPlusOne + 2) >> 2; // F.3, page 118, ITU-T Rec. T.800 final draft
	

			//write current (even)
			writePixel(current, currentScratch);

			//write odd
			currentScratch += VERTICAL_STRIDE;
			writePixel(currentPlusOne, currentScratch);

			//advance scratch pointer
			currentScratch += VERTICAL_STRIDE;

			//update registers
			minusOne = currentPlusOne;
			current = currentPlusTwo;
		}

		
		//4. transform horizontally
		currentScratch = scratch + getScratchOffset();	

		
		localMemoryFence();
		//odd columns (skip right odd boundary column)
		if ( (getLocalId(0)&1) && (getLocalId(0) != WIN_SIZE_X-1) ) {
			for (int j = 0; j < WIN_SIZE_Y; j++) {
				int4 currentOdd = readPixel(currentScratch);
				int4 prevEven = readPixel(currentScratch + HORIZONTAL_ODD_TO_PREVIOUS_EVEN);
				int4 nextEven = readPixel(currentScratch + HORIZONTAL_ODD_TO_NEXT_EVEN); 
				currentOdd -= ((prevEven + nextEven) >> 1);
				writePixel( currentOdd, currentScratch);
				currentScratch += VERTICAL_STRIDE;
			}
		}
		
		currentScratch = scratch + getScratchOffset();	
		localMemoryFence();
		//even columns (skip left and right even boundary columns)
		if ( !(getLocalId(0)&1) && (getLocalId(0) != 0) && (getLocalId(0) != WIN_SIZE_X-2)  ) {
			for (int j = 0; j < WIN_SIZE_Y; j++) {
				int4 currentEven = readPixel(currentScratch);
				int4 prevOdd = readPixel(currentScratch + HORIZONTAL_EVEN_TO_PREVIOUS_ODD);
				int4 nextOdd = readPixel(currentScratch + HORIZONTAL_EVEN_TO_NEXT_ODD); 
				currentEven += (prevOdd + nextOdd + 2) >> 2; 
				writePixel( currentEven, currentScratch);
				currentScratch += VERTICAL_STRIDE;
			}
		}
		localMemoryFence();

		//5. write local buffer column to destination image
		// (only write non-boundary columns that are within the image bounds)
		if ((getLocalId(0) >= BOUNDARY_X) && ( getLocalId(0) < WIN_SIZE_X - BOUNDARY_X) && (inputX < width) && inputX >= 0) {
			writeColumnToOutput(scratch + getScratchOffset(), odata, firstY, outputX, height, halfHeight);

		}
		// move to next step 
		firstY += WIN_SIZE_Y;
	}
}

