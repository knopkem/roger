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

#define BOUNDARY_X 4

#define VERTICAL_STRIDE 64  // WIN_SIZE_X/2 


// two vertical neighbours: pointer diff:
#define BUFFER_SIZE            512	// VERTICAL_STRIDE * WIN_SIZE_Y


#define CHANNEL_BUFFER_SIZE     1024            // BUFFER_SIZE + BUFFER_SIZE

#define CHANNEL_BUFFER_SIZE_X2  2048   
#define CHANNEL_BUFFER_SIZE_X3  3072   

#define PIXEL_BUFFER_SIZE   4096

#define HORIZONTAL_EVEN_TO_PREVIOUS_ODD  511
#define HORIZONTAL_EVEN_TO_NEXT_ODD      512

#define HORIZONTAL_ODD_TO_PREVIOUS_EVEN -512
#define HORIZONTAL_ODD_TO_NEXT_EVEN     -511

CONSTANT float P1 = -1.586134342;   ///< forward 9/7 predict 1
CONSTANT float U1 = -0.05298011854;  ///< forward 9/7 update 1
CONSTANT float P2 = 0.8829110762;   ///< forward 9/7 predict 2
CONSTANT float U2 = 0.4435068522;    ///< forward 9/7 update 2
CONSTANT float U1P1 = 0.08403358545952490068;

CONSTANT float scale97Mul = 1.23017410491400f;
CONSTANT float scale97Div = 1.0 / 1.23017410491400f;

/*

Lifting scheme consists of four steps: Predict1 Update1 Predict2 Update2
followed by scaling.

If the odd predict2 pixels are calculated first, then the even update2 pixels
can be easily calculated.

Predict Calculation:

For S odd, we have:


plusOne_P1 = plusOne + P1*(current + plusTwo)

plusOne_P2 = plusOne_P1 + 
             P2*(current_U1 + plusTwo_U1)
           =   plusOne + P1*(current + plusTwo) +
		         P2*(current + U1*(minusOne + plusOne) + U1P1*(minusTwo + 2*current + plusTwo) + 
				     plusTwo + U1*(plusOne + plusThree) + U1P1*(current + 2*plusTwo + plusFour)  )



Update Calculation

For S even, we have

current_U1 = current + U1*(minusOne_P1 + plusOne_P1)
           = current + U1*(minusOne + P1*(minusTwo + current) + plusOne + P1*(current + plusTwo))
		   = current + U1*(minusOne + plusOne) + U1P1*(minusTwo + 2*current + plusTwo)

current_U2 = current_U1 + U2*(minusOne_P2 + plusOne_P2)

*/
  

///////////////////////////////////////////////////////////////////////

CONSTANT sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_MIRRORED_REPEAT  | CLK_FILTER_NEAREST;

inline int getCorrectedGlobalIdX() {
      return getGlobalId(0) - 2 * BOUNDARY_X * getGroupId(0);
}


// read pixel from local buffer
float4 readPixel( LOCAL float*  restrict  src) {
	return (float4)(*src, *(src+CHANNEL_BUFFER_SIZE),  *(src+CHANNEL_BUFFER_SIZE_X2),  *(src+CHANNEL_BUFFER_SIZE_X3)) ;
}

//write pixel to column
inline void writePixel(float4 pix, LOCAL float*  restrict  dest) {
	*dest = pix.x;
	dest += CHANNEL_BUFFER_SIZE;
	*dest = pix.y;
	dest += CHANNEL_BUFFER_SIZE;
	*dest = pix.z;
	dest += CHANNEL_BUFFER_SIZE;
	*dest = pix.w;
}

// write column to destination
void writeColumnToOutput(LOCAL float* restrict currentScratch, __write_only image2d_t odata, int firstY, int inputX, int height, int halfHeight){

	int2 posOut = {inputX, firstY>>1};
	for (int j = 0; j < WIN_SIZE_Y; j+=2) {
	
	    // even row
		
		//only need to check evens, since even point will be the first out of bound point
	    if (posOut.y >= halfHeight)
			break;

		write_imagef(odata, posOut,scale97Div * readPixel(currentScratch));

		// odd row
		currentScratch += VERTICAL_STRIDE ;
		posOut.y+= halfHeight;

		write_imagef(odata, posOut,scale97Mul * readPixel(currentScratch));

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
	LOCAL float scratch[PIXEL_BUFFER_SIZE];
	const float yDelta = 1.0/(height-1);
	int firstY = getGlobalId(1) * (steps * WIN_SIZE_Y);
	
	//0. Initialize: fetch first pixel (and 2 top boundary pixels)

	// read -4 point
	float2 posIn = (float2)(inputX, firstY - 4) /  (float2)(width-1, height-1);	
	float4 minusFour = read_imagef(idata, sampler, posIn);

	posIn.y += yDelta;
	float4 minusThree = read_imagef(idata, sampler, posIn);

	// read -2 point
	posIn.y += yDelta;
	float4 minusTwo = read_imagef(idata, sampler, posIn);

	// read -1 point
	posIn.y += yDelta;
	float4 minusOne = read_imagef(idata, sampler, posIn);

	// read 0 point
	posIn.y += yDelta;
	float4 current = read_imagef(idata, sampler, posIn);

	// +1 point
	posIn.y += yDelta;
	float4 plusOne = read_imagef(idata, sampler, posIn);

	// +2 point
	posIn.y += yDelta;
	float4 plusTwo = read_imagef(idata, sampler, posIn);

	float4 minusThree_P1 = minusThree + P1*(minusFour + minusTwo);
	float4 minusOne_P1   = minusOne   + P1*(minusTwo + current);
	float4 plusOne_P1    = plusOne    + P1*(current + plusTwo);

	float4 minusTwo_U1 = minusTwo + U1*(minusThree_P1 + minusOne_P1);
	float4 current_U1  = current + U1*(minusOne_P1 + plusOne_P1);
	float4 minusOne_P2 = minusOne_P1 + P2*(minusTwo_U1 + current_U1);
		
	for (int i = 0; i < steps; ++i) {

		// 1. read from source image, transform columns, and store in local scratch
		LOCAL float* currentScratch = scratch + getScratchOffset();
		for (int j = 0; j < WIN_SIZE_Y; j+=2) {

	        //read next two points

			// +3 point
			posIn.y += yDelta;
			float4 plusThree = read_imagef(idata, sampler, posIn);
	   
	   		// +4 point
			posIn.y += yDelta;
	   		if (posIn.y > 1 + 3*yDelta)
				break;
			float4 plusFour = read_imagef(idata, sampler, posIn);

			float4 plusThree_P1    = plusThree  + P1*(plusTwo + plusFour);
			float4 plusTwo_U1      = plusTwo + U1*(plusOne_P1 + plusThree_P1);
			float4 plusOne_P2      = plusOne_P1 + P2*(current_U1 + plusTwo_U1);
								 
					  
			//write current U2 (even)
			writePixel(scale97Div * (current_U1 +  U2 * (minusOne_P2 + plusOne_P2)), currentScratch);

			//advance scratch pointer
			currentScratch += VERTICAL_STRIDE;

			//write current P2 (odd)
			writePixel(scale97Mul* plusOne_P2 , currentScratch);

			//advance scratch pointer
			currentScratch += VERTICAL_STRIDE;

			// shift registers up by two
			minusFour = minusTwo;
			minusThree = minusOne;
			minusTwo = current;
			minusOne = plusOne;
			current = plusTwo;
			plusOne = plusThree;
			plusTwo = plusFour;
			//update P1s
			minusThree_P1 = minusOne_P1;
			minusOne_P1 = plusOne_P1;
			plusOne_P1 = plusThree_P1;

			//update U1s
			minusTwo_U1 = current_U1;
			current_U1 = plusTwo_U1;

			//update P2s
			minusOne_P2 = plusOne_P2;
		}

		
		//4. transform horizontally
		currentScratch = scratch + getScratchOffset();	

		
		localMemoryFence();
		// P2 - odd columns (skip left three boundary columns and all right boundary columns)
		if ( (getLocalId(0)&1) && (getLocalId(0) >= BOUNDARY_X-1) && (getLocalId(0) < WIN_SIZE_X-BOUNDARY_X) ) {
			for (int j = 0; j < WIN_SIZE_Y; j++) {
				float4 current = readPixel(currentScratch);
				float4 minusOne = readPixel(currentScratch + HORIZONTAL_ODD_TO_PREVIOUS_EVEN);
				float4 minusTwo = readPixel(currentScratch -1);
				float4 minusThree = readPixel(currentScratch + HORIZONTAL_ODD_TO_PREVIOUS_EVEN -1);
				float4 plusOne = readPixel(currentScratch + HORIZONTAL_ODD_TO_NEXT_EVEN);
				float4 plusTwo = readPixel(currentScratch + 1); 
				float4 plusThree = readPixel(currentScratch + HORIZONTAL_ODD_TO_NEXT_EVEN+1);

				float4 current_P2 =  current + P1*(minusOne + plusOne) +
		         P2*( minusOne + U1*(minusTwo + current) + U1P1*(minusThree + 2*minusOne + plusOne) + 
				      plusOne + U1*(current + plusTwo) + U1P1*(minusOne + 2*plusOne + plusThree)  );
				writePixel(current_P2, currentScratch);
				currentScratch += VERTICAL_STRIDE;
			}
		}
		

		currentScratch = scratch + getScratchOffset();	
		localMemoryFence();
		//U2 - even columns (skip left and right boundary columns)
		if ( !(getLocalId(0)&1) && (getLocalId(0) >= BOUNDARY_X) && (getLocalId(0) < WIN_SIZE_X-BOUNDARY_X)  ) {
			for (int j = 0; j < WIN_SIZE_Y; j++) {

				float4 current = readPixel(currentScratch);
				float4 minusOne = readPixel(currentScratch + HORIZONTAL_EVEN_TO_PREVIOUS_ODD);
				float4 minusTwo = readPixel(currentScratch -1);
				float4 plusOne = readPixel(currentScratch + HORIZONTAL_EVEN_TO_NEXT_ODD);
				float4 plusTwo = readPixel(currentScratch + 1); 
				float4 prevOdd = readPixel(currentScratch + HORIZONTAL_EVEN_TO_PREVIOUS_ODD);
				float4 nextOdd = readPixel(currentScratch + HORIZONTAL_EVEN_TO_NEXT_ODD); 
				writePixel(current + U1*(minusOne + plusOne) + U1P1*(minusTwo + 2*current + plusTwo) +
				                                                   U2*(prevOdd + nextOdd), currentScratch);
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

