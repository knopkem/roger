#include "ocl_platform.cl"



// number of banks in local memory
// LDS_BANKS

//////////////////////////
// dimensions of window
// WIN_SIZE_X	assume this equals number of work items in work group
// WIN_SIZE_Y
///////////////////////////



/// Returns index ranging from 0 to num work items, such that first half
/// of the work items get even indices and others get odd indices. Each work item
/// gets a different index.
/// Example: (for WIN_SiZE_X == 8)   getLocalId(0):   0  1  2  3  4  5  6  7
///									 parityIdx:		  0  2  4  6  1  3  5  7
/// @return parity-separated index of work item
inline int parityIdx() {
	return (getLocalId(0) * 2) - (WIN_SIZE_X - 1) * (getLocalId(0) / (WIN_SIZE_X / 2));
}




// scratch buffer (in local memory of GPU) where block of input image is stored.

/// Odd and even columns are separated. (Generates less bank conflicts when using lifting scheme.)
/// All even columns are stored first, then all odd columns.
/// All operations expect WIN_SIZE_X threads.

/// BOUNDARY_X		number of extra pixels at the left and right side
///					boundary is expected to be smaller than half WIN_SIZE_X
///					Must be divisible by 2, because half of the boundary lies on the left of the image,
///                 half lies on the right
 

// two vertical neighbours: pointer diff:
#define VERTICAL_STRIDE 66			// BOUNDARY_X + (WIN_SIZE_X / 2)

// size of one of two buffers (odd or even)
#define BUFFER_SIZE     528          // VERTICAL_STRIDE * WIN_SIZE_Y

/// padding between even and odd buffers: used to reduce bank conflicts
#define PADDING			32           // LDS_BANKS - ((BUFFER_SIZE + LDS_BANKS / 2) % LDS_BANKS);

// offset of the odd columns buffer from the beginning of data buffer
#define ODD_OFFSET 560              // BUFFER_SIZE + PADDING

// size of buffer for both even and odd columns
#define TOTAL_BUFFER_SIZE  1088     // 2 * BUFFER_SIZE + PADDING;
///////////////////////////////////////////////////////////////////////

CONSTANT int4 twoVec = (int4)(2,2,2,2);

CONSTANT sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_MIRRORED_REPEAT  | CLK_FILTER_NEAREST;

void readPixel(int4* const pix, LOCAL int*  restrict  src) {
	pix->x = *src ;
	src += TOTAL_BUFFER_SIZE;
	pix->y = *src;
	src += TOTAL_BUFFER_SIZE;
	pix->z = *src;
	src += TOTAL_BUFFER_SIZE;
	pix->w = *src;
}

void writePixel(int4 pix, LOCAL int*  restrict  dest) {
	*dest = pix.x;
	dest += TOTAL_BUFFER_SIZE;
	*dest = pix.y;
	dest += TOTAL_BUFFER_SIZE;
	*dest = pix.z;
	dest += TOTAL_BUFFER_SIZE;
	*dest = pix.w;
}


//fetch first pixel (and 2 top boundary pixels)
// -2  -1 0 1 2 
void initVertical(__read_only image2d_t idata, float2 posIn, float yDelta, int4*  restrict const buff) {
	///////////////////////////////////////////////
	// read -2 point
	int4 minusTwo = read_imagei(idata, sampler, posIn);
	// read -1 point
	posIn.y += yDelta;
	buff[0] = read_imagei(idata, sampler, posIn);
	// read 0 point
	posIn.y += yDelta;
	buff[1] = read_imagei(idata, sampler, posIn);
	///////////////////////////////////////////////

	// transform -1 point (no need to write to local memory)
	buff[0] -= (minusTwo + buff[1]) >> 1;  
}

// fetch next two pixels, store in circular buffer, and write last (even) and current (odd)
void fetchNextVertical(__read_only image2d_t idata, float2 posIn, float yDelta, int4*  restrict const buff, int buffStart, LOCAL int* restrict dest) {
    // read current (odd) point
	buff[buffStart] = read_imagei(idata, sampler, posIn);
	
	// read next (even) point
	posIn.y += yDelta;
	buff[(buffStart+1)&3] = read_imagei(idata, sampler, posIn);
	
	// transform current (odd) point
	buff[buffStart] -= (buff[(buffStart-1)&3] + buff[(buffStart+1)&3]) >> 1;
	
	// transform previous (even) point
	buff[(buffStart-1)&3] += (buff[(buffStart-2)&3] + buff[buffStart] + twoVec) >> 2; 
	
	//write even
	writePixel(buff[(buffStart-1)&3], dest);

	//write odd
	dest += WIN_SIZE_X;
	writePixel(buff[buffStart], dest);
}


//1. initialize column (and optionally boundary column)
//2. read column(s) into scratch
//3. vertically transform column(s)
//4. horizontally transform all rows corresponding to this column(s)
//5. write into destination
void KERNEL run(__read_only image2d_t idata, __write_only image2d_t odata,   
                       const unsigned int  width, const unsigned int  height, const unsigned int steps) {
	LOCAL int scratch[TOTAL_BUFFER_SIZE*4];
	float yDelta = 1.0/height;
	int firstY = getGlobalId(1) * (steps * WIN_SIZE_Y);
	int4 cache[4];

	const float2 posIn = {getGlobalId(0)/(float)width, (firstY - 2)*yDelta};		    
	initVertical(idata, posIn, yDelta, cache);	 
	int cachePosition = 0;
	
	// adjust input y so that loop starts at pixel 1	
	posIn.y += yDelta;

	for (int i = 0; i < steps; ++i) {

	   LOCAL int* currentScratch = scratch + getLocalId(0);

	   //read pixels two at a time, and store in local buffer
	   for (int j = 0; j < WIN_SIZE_Y/2; ++j) {
	   
	   		// jump two pixels
	   		posIn.y += 2*yDelta;
			if (posIn.y >= 1)
			   break;
			cachePosition = (cachePosition +2) &3;
	        //fetch
			fetchNextVertical(idata,posIn , yDelta, cache, cachePosition, currentScratch);
			//advance scratch pointer
			currentScratch += 2*WIN_SIZE_X;

		}
	
	    localMemoryFence();

		// write even points to destination
		const int2 posOut = {getGlobalId(0), firstY>>1};
		currentScratch = scratch +  getLocalId(0);
		for (int j = 0; j < WIN_SIZE_Y; j+=2) {
	
	        if (posOut.x >= width || posOut.y >= height/2)
				break;

			int4 pix;
			readPixel(&pix, currentScratch);
			write_imagei(odata, posOut,pix);

			currentScratch += 2*WIN_SIZE_X;
			posOut.y++;
		}
		//write odd points to destination
		posOut.y =  ((height + firstY)>>1) + 1;
		currentScratch = scratch +  getLocalId(0) + WIN_SIZE_X;
		for (int j = 0; j < WIN_SIZE_Y; j+=2) {
	
			 if (posOut.x >= width || posOut.y >= height)
				break;

			int4 pix;
			readPixel(&pix, currentScratch);
			write_imagei(odata, posOut,pix);

			currentScratch += 2*WIN_SIZE_X;
			posOut.y++;
		}

		firstY += WIN_SIZE_Y;
		localMemoryFence();
	}
	
}

