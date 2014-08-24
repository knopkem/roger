#include "ocl_platform.cl"

// number of banks in local memory
// LDS_BANKS

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


 **/

 #define BOUNDARY_X_LEFT 1

#define VERTICAL_STRIDE_EVEN 66  // WIN_SIZE_X/2 + 2 boundary columns
#define VERTICAL_STRIDE_ODD 65   // WIN_SIZE_X/2 + 1 boundary column

// two vertical neighbours: pointer diff:
#define BUFFER_SIZE_EVEN            528	// VERTICAL_STRIDE_EVEN * WIN_SIZE_Y
#define BUFFER_SIZE_ODD             520	// VERTICAL_STRIDE_ODD * WIN_SIZE_Y

#define EVEN_ODD_PADDING                     16    // LDS_BANKS - (BUFFER_SIZE_EVEN % LDS_BANKS)

#define CHANNEL_BUFFER_SIZE_UNPADDED    1064    // BUFFER_SIZE_EVEN + EVEN_ODD_PADDING +  BUFFER_SIZE_ODD
#define CHANNEL_BUFFER_PADDING  24              // LDS_BANKS - (CHANNEL_BUFFER_SIZE_UNPADDED % LDS_BANKS) 
#define CHANNEL_BUFFER_SIZE     1088            // BUFFER_SIZE_EVEN + EVEN_ODD_PADDING +  BUFFER_SIZE_ODD

#define CHANNEL_BUFFER_SIZE_X2  2176   
#define CHANNEL_BUFFER_SIZE_X3  3264   

#define PIXEL_BUFFER_SIZE   4328

#define VERTICAL_STRIDE_LOW_TO_HIGH   544   // BUFFER_SIZE_EVEN + EVEN_ODD_PADDING
#define VERTICAL_STRIDE_HIGH_TO_LOW  -478   // VERTICAL_STRIDE_EVEN - STRIDE_HIGH_TO_LOW




///////////////////////////////////////////////////////////////////////

CONSTANT sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_MIRRORED_REPEAT  | CLK_FILTER_NEAREST;

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


void writeColumnToOutput(LOCAL int* restrict currentScratch, __write_only image2d_t odata, int firstY, int height, int halfHeight){
	// write points to destination
	int2 posOut = {getGlobalId(0), firstY>>1};
	for (int j = 0; j < WIN_SIZE_Y; j+=2) {
	
	    // even
	    if (posOut.y >= halfHeight)
			break;

		write_imagei(odata, posOut,readPixel(currentScratch));

		// odd
		currentScratch += VERTICAL_STRIDE_LOW_TO_HIGH ;
		posOut.y+= halfHeight + 1;
		if (posOut.y >= height)
			break;

		write_imagei(odata, posOut,readPixel(currentScratch));

		currentScratch += VERTICAL_STRIDE_HIGH_TO_LOW;
		posOut.y -= halfHeight;
	}
}

// offset when transforming columns
inline int getScratchColumnOffset(){
   return BOUNDARY_X_LEFT + (getLocalId(0) >> 1);
}

// assumptions: width and height are both even
// (we will probably have to relax these assumptions in the future)
void KERNEL run(__read_only image2d_t idata, __write_only image2d_t odata,   
                       const unsigned int  width, const unsigned int  height, const unsigned int steps) {

	if (getGlobalId(0) >= width)
	    return;

   // column 0 loads column -2 (even) with x offset -2
   // column 1 loads column -1 (odd)  with x offset -2
   // column WIN_SIZE_X -2 loads column WIN_SIZE_X (even) with x offset +2
   int srcBoundaryShiftX = 0;
   int scratchBoundaryShiftX = 0;
   if (getLocalId(0) == 0 || getLocalId(0) == 1) {
       srcBoundaryShiftX = -2;
	   scratchBoundaryShiftX = -1;
   }
   else if (getLocalId(0) == WIN_SIZE_X -2) {
       srcBoundaryShiftX = -2;
	   scratchBoundaryShiftX = 1;
   }

	const int halfWinSizeX = WIN_SIZE_X >> 1;

    const unsigned int halfHeight = height >> 1;
	LOCAL int scratch[PIXEL_BUFFER_SIZE];
	float yDelta = 1.0/height;
	int firstY = getGlobalId(1) * (steps * WIN_SIZE_Y);

	
	//0. Initialize: fetch first pixel (and 2 top boundary pixels)
	int4 minusOneBdy;
	int4 currentBdy;

	// read -1 point
	const float2 posIn = (float2)(getGlobalId(0), firstY - 1) /  (float2)(width, height);	
	float2 posInBdy; 

	int4 minusOne = read_imagei(idata, sampler, posIn);

	// read 0 point
	posIn.y += yDelta;
	int4 current = read_imagei(idata, sampler, posIn);

	// transform -1 point (no need to write to local memory)
	minusOne -= ( read_imagei(idata, sampler, (float2)(posIn.x, (firstY - 2)*yDelta)) + current) >> 1;   

	if (srcBoundaryShiftX) {
	   posInBdy = posIn;	
	   posInBdy.y -=  2*yDelta;
	   minusOneBdy = read_imagei(idata, sampler, posInBdy);

	  // read 0 point
  	  posInBdy.y += yDelta;
	  currentBdy = read_imagei(idata, sampler, posInBdy);

	  // transform -1 point (no need to write to local memory)
	  minusOneBdy -= ( read_imagei(idata, sampler, (float2)(posInBdy.x, (firstY - 2)*yDelta)) + current) >> 1;   
	}
	

	for (int i = 0; i < steps; ++i) {

		// 1. read from source image, transform columns, and store in local scratch
		LOCAL int* currentScratch = scratch + getScratchColumnOffset();
		for (int j = 0; j < WIN_SIZE_Y>>1; ++j) {
	   
			///////////////////////////////////////////////////////////////////////////////////////////
			// fetch next two pixels, then transform and write current (odd) and last (even)  
			
			// read current plus one (odd) point
			posIn.y += yDelta;
			if (posIn.y >= 1)
				break;
			int4 currentPlusOne = read_imagei(idata, sampler, posIn);
	
			// read current plus two (even) point
			posIn.y += yDelta;
			int4 currentPlusTwo = read_imagei(idata, sampler, posIn);
	
			// transform current plus one (odd) point
			currentPlusOne -= (current + currentPlusTwo) >> 1;
	
			// transform current (even) point
			current += (minusOne + currentPlusOne + (int4)(2,2,2,2)) >> 2; 
	
			//write current (even)
			writePixel(current, currentScratch);

			//write odd
			currentScratch += VERTICAL_STRIDE_LOW_TO_HIGH;
			writePixel(currentPlusOne, currentScratch);

			//advance scratch pointer
			currentScratch += VERTICAL_STRIDE_HIGH_TO_LOW;

			//update registers
			minusOne = currentPlusOne;
			current = currentPlusTwo;
		}
		//read boundary column if necessary
		if (scratchBoundaryShiftX) {
			currentScratch = scratch + getScratchColumnOffset() + scratchBoundaryShiftX;
			for (int j = 0; j < WIN_SIZE_Y>>1; ++j) {
	   
				///////////////////////////////////////////////////////////////////////////////////////////
				// fetch next two pixels, then transform and write current (odd) and last (even)  
			
				// read current plus one (odd) point
				posInBdy.y += yDelta;
				if (posInBdy.y >= 1)
					break;
				int4 currentPlusOneBdy = read_imagei(idata, sampler, posInBdy);
	
				// read current plus two (even) point
				posInBdy.y += yDelta;
				int4 currentPlusTwoBdy = read_imagei(idata, sampler, posInBdy);
	
				// transform current plus one (odd) point
				currentPlusOneBdy -= (currentBdy + currentPlusTwoBdy) >> 1;
	
				// transform current (even) point
				currentBdy += (minusOneBdy + currentPlusOneBdy + (int4)(2,2,2,2)) >> 2; 
	
				//write current (even)
				writePixel(currentBdy, currentScratch);

				//write odd
				currentScratch += VERTICAL_STRIDE_LOW_TO_HIGH;
				writePixel(currentPlusOneBdy, currentScratch);

				//advance scratch pointer
				currentScratch += VERTICAL_STRIDE_HIGH_TO_LOW;

				//update registers
				minusOneBdy = currentPlusOneBdy;
				currentBdy = currentPlusTwoBdy;
			}
		}

		//4. transform rows

		//5. write local buffer to destination image
		writeColumnToOutput(scratch + getScratchColumnOffset(), odata, firstY, height, halfHeight);

		// move to next step 
		firstY += WIN_SIZE_Y;
	}
}

