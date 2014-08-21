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
 #define BOUNDARY_X_MIDDLE 2
 


// two vertical neighbours: pointer diff:
#define VERTICAL_STRIDE 131			// BOUNDARY_X_LEFT + BOUNDARY_X_MIDDLE + WIN_SIZE_X 	

#define TOTAL_BUFFER_SIZE     1048          // VERTICAL_STRIDE * WIN_SIZE_Y
#define TOTAL_BUFFER_SIZE_X2  2096   
#define TOTAL_BUFFER_SIZE_X3  3144   


///////////////////////////////////////////////////////////////////////

CONSTANT sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_MIRRORED_REPEAT  | CLK_FILTER_NEAREST;

// read pixel from local buffer
int4 readPixel( LOCAL int*  restrict  src) {
	return (int4)(*src, *(src+TOTAL_BUFFER_SIZE),  *(src+TOTAL_BUFFER_SIZE_X2),  *(src+TOTAL_BUFFER_SIZE_X3)) ;
}

//write pixel to column
inline void writePixel(int4 pix, LOCAL int*  restrict  dest) {
	*dest = pix.x;
	dest += TOTAL_BUFFER_SIZE;
	*dest = pix.y;
	dest += TOTAL_BUFFER_SIZE;
	*dest = pix.z;
	dest += TOTAL_BUFFER_SIZE;
	*dest = pix.w;
}

// write pixel to both designated column and mirrored boundary column
inline void writePixelAndBoundary(int4 pix, LOCAL int*  restrict  dest, int boundaryShift) {
	writePixel(pix, dest);
	writePixel(pix, dest+boundaryShift);
}

void writeColumnToOutput(LOCAL int* restrict currentScratch, __write_only image2d_t odata, int firstY, int height, int halfHeight){
	// write points to destination
	int2 posOut = {getGlobalId(0), firstY>>1};
	for (int j = 0; j < WIN_SIZE_Y; j+=2) {
	
	    // even
	    if (posOut.y >= halfHeight)
			break;

		write_imagei(odata, posOut,readPixel(currentScratch));

		currentScratch += VERTICAL_STRIDE ;
		posOut.y+= halfHeight + 1;

		// odd
		if (posOut.y >= height)
			break;

		write_imagei(odata, posOut,readPixel(currentScratch));

		currentScratch += VERTICAL_STRIDE;
		posOut.y -= halfHeight;
	}
}

// offset when transforming columns
inline int getScratchColumnOffset(){
   return BOUNDARY_X_LEFT + (getLocalId(0) >> 1) + ( (getLocalId(0)&1) * (BOUNDARY_X_MIDDLE + (WIN_SIZE_X>>1))  );
}

// assumptions: width and height are both even
// (we will probably have to relax these assumptions in the future)
void KERNEL run(__read_only image2d_t idata, __write_only image2d_t odata,   
                       const unsigned int  width, const unsigned int  height, const unsigned int steps) {

	if (getGlobalId(0) >= width)
	    return;

	const int halfWinSizeX = WIN_SIZE_X >> 1;

    //check if this column needs to handle a boundary column
	int boundaryShift = 0;
	if (getGlobalId(0) == 1)
		boundaryShift =  halfWinSizeX + 2;
	else if (getGlobalId(0) == 2)
		boundaryShift = -3;
	else if (getGlobalId(0) == width - 2)
	    boundaryShift =  halfWinSizeX + 1;

    const unsigned int halfHeight = height >> 1;
	LOCAL int scratch[TOTAL_BUFFER_SIZE << 2];
	float yDelta = 1.0/height;
	int firstY = getGlobalId(1) * (steps * WIN_SIZE_Y);

	
	//0. Initialize: fetch first pixel (and 2 top boundary pixels)

	// read -1 point
	const float2 posIn = (float2)(getGlobalId(0), firstY - 1) /  (float2)(width, height);	
	int4 minusOne = read_imagei(idata, sampler, posIn);

	// read 0 point
	posIn.y += yDelta;
	int4 current = read_imagei(idata, sampler, posIn);

	// transform -1 point (no need to write to local memory)
	minusOne -= ( read_imagei(idata, sampler, (float2)(posIn.x, (firstY - 2)*yDelta)) + current) >> 1;   
	
	
	{
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

			// transform odd (two rows at a time)
			currentScratch = scratch + getScratchColumnOffset() + (1+ getLocalId(0) % halfWinSizeX ) * VERTICAL_STRIDE;
			for (int j = 1; j < WIN_SIZE_Y; j+=2) {
	            

				currentScratch += VERTICAL_STRIDE;
			}
			
			// transform even (two rows at a time
			for (int j = 0; j < WIN_SIZE_Y; j+=2) {
	   

			}


			//3. write local buffer to destination image
			writeColumnToOutput(scratch + getScratchColumnOffset(), odata, firstY, height, halfHeight);

			// move to next step 
			firstY += WIN_SIZE_Y;
		}
	}
}


////////////////////////////////////////////
/*
/// Returns index ranging from 0 to num work items, such that first half
/// of the work items get even indices and others get odd indices. Each work item
/// gets a different index.
/// Example: (for WIN_SiZE_X == 8)   getLocalId(0):   0  1  2  3  4  5  6  7
///									 parityIdx:		  0  2  4  6  1  3  5  7

/// @return parity-separated index of work item
inline int parityIdx() {
	return (getLocalId(0) << 1) - (WIN_SIZE_X - 1) * (getLocalId(0) / (WIN_SIZE_X >> 1));
}

// two vertical neighbours: pointer diff:
#define VERTICAL_STRIDE_EVEN 66			// 2*BOUNDARY_X + (WIN_SIZE_X / 2)		

#define VERTICAL_STRIDE_ODD  65          //  BOUNDARY_X  + (WIN_SIZE_X / 2)
	
// size of one of two buffers (odd or even)
#define BUFFER_SIZE_EVEN     528          // VERTICAL_STRIDE_EVEN * WIN_SIZE_Y
#define BUFFER_SIZE_ODD     528          // VERTICAL_STRIDE_ODD * WIN_SIZE_Y

/// padding between even and odd buffers: used to reduce bank conflicts
#define PADDING			32           // LDS_BANKS - ((BUFFER_SIZE + LDS_BANKS / 2) % LDS_BANKS);

// offset of the odd columns buffer from the beginning of data buffer
#define ODD_OFFSET 560              // BUFFER_SIZE + PADDING

// size of buffer for both even and odd columns
#define TOTAL_BUFFER_SIZE  1088     // 2 * BUFFER_SIZE + PADDING;
#define TOTAL_BUFFER_SIZE_X2  2176   
#define TOTAL_BUFFER_SIZE_X3  3264   
*/
////////////////////////////////////////////
