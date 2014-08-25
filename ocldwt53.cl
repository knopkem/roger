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

#define BOUNDARY_X 2

#define VERTICAL_STRIDE 64  // WIN_SIZE_X/2 


// two vertical neighbours: pointer diff:
#define BUFFER_SIZE            512	// VERTICAL_STRIDE * WIN_SIZE_Y


#define CHANNEL_BUFFER_SIZE     1024            // BUFFER_SIZE + BUFFER_SIZE

#define CHANNEL_BUFFER_SIZE_X2  2048   
#define CHANNEL_BUFFER_SIZE_X3  3072   

#define PIXEL_BUFFER_SIZE   4096

#define VERTICAL_STRIDE_LOW_TO_HIGH   (BUFFER_SIZE)   // BUFFER_SIZE 




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
	int2 posOut = {getGlobalId(0) - 2 * BOUNDARY_X * getGroupId(0) - BOUNDARY_X, firstY>>1};
	if (posOut.x < 0)
	   return;
	for (int j = 0; j < WIN_SIZE_Y; j+=2) {
	
	    // even
	    if (posOut.y >= halfHeight)
			break;

		write_imagei(odata, posOut,readPixel(currentScratch));

		// odd
		currentScratch += VERTICAL_STRIDE ;
		posOut.y+= halfHeight + 1;
		if (posOut.y >= height)
			break;

		write_imagei(odata, posOut,readPixel(currentScratch));

		currentScratch += VERTICAL_STRIDE;
		posOut.y -= halfHeight;
	}
}

// offset when transforming columns
inline int getScratchColumnOffset(){
   return (getLocalId(0)>> 1) + (getLocalId(0)&1) * VERTICAL_STRIDE_LOW_TO_HIGH;
}

// assumptions: width and height are both even
// (we will probably have to relax these assumptions in the future)
void KERNEL run(__read_only image2d_t idata, __write_only image2d_t odata,   
                       const unsigned int  width, const unsigned int  height, const unsigned int steps) {

	int inputX = getGlobalId(0) - 2 * BOUNDARY_X * getGroupId(0) - BOUNDARY_X;
	if (inputX >= width)
	   return;

	const int halfWinSizeX = WIN_SIZE_X >> 1;
    const unsigned int halfHeight = height >> 1;
	LOCAL int scratch[PIXEL_BUFFER_SIZE];
	const float yDelta = 1.0/height;
	int firstY = getGlobalId(1) * (steps * WIN_SIZE_Y);
	
	//0. Initialize: fetch first pixel (and 2 top boundary pixels)

	// read -1 point
	float2 posIn = (float2)(inputX, firstY - 1) /  (float2)(width, height);	
	int4 minusOne = read_imagei(idata, sampler, posIn);

	// read 0 point
	posIn.y += yDelta;
	int4 current = read_imagei(idata, sampler, posIn);

	// transform -1 point (no need to write to local memory)
	minusOne -= ( read_imagei(idata, sampler, (float2)(posIn.x, (firstY - 2)*yDelta)) + current) >> 1;   
	bool doWrite = (getLocalId(0) >= BOUNDARY_X) && ( getLocalId(0) < WIN_SIZE_X - BOUNDARY_X) ;
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
			currentScratch += VERTICAL_STRIDE;
			writePixel(currentPlusOne, currentScratch);

			//advance scratch pointer
			currentScratch += VERTICAL_STRIDE;

			//update registers
			minusOne = currentPlusOne;
			current = currentPlusTwo;
		}

		//4. transform rows
		if (doWrite) {

			//5. write local buffer to destination image
			writeColumnToOutput(scratch + getScratchColumnOffset(), odata, firstY, height, halfHeight);

		}
		// move to next step 
		firstY += WIN_SIZE_Y;
	}
}

