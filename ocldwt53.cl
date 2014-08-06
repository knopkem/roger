#include "ocl_platform.cl"


/// Returns index ranging from 0 to num threads, such that first half
/// of threads get even indices and others get odd indices. Each thread
/// gets different index.
/// Example: (for 8 threads)   threadIdx.x:   0  1  2  3  4  5  6  7
///                              parityIdx:   0  2  4  6  1  3  5  7
/// @param THREADS  total count of participating threads
/// @return parity-separated index of thread
inline int parityIdx(int THREADS) {
	return (getLocalId(0) * 2) - (THREADS - 1) * (getLocalId(0) / (THREADS / 2));
}


// number of banks in local memory
#define LDS_BANKS 32

//////////////////////////
// dimensions of window
#define WIN_SIZE_X		128
#define WIN_SIZE_Y		8
///////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////
// Transform Buffer (in local memory of GPU) where block of input image is stored.

/// Odd and even columns are separated. (Generates less bank conflicts when using lifting scheme.)
/// All even columns are stored first, then all odd columns.
/// All operations expect BUF_SIZE_X threads.

/// BUF_SIZE_X       width of the buffer excluding two boundaries 
///					(Also equal to the number of threads participating on all operations)
///					Must be divisible by 4.
/// BUF_SIZE_Y      height of buffer (total number of lines)
/// BOUNDARY_X		number of extra pixels at the left and right side
///					boundary is expected to be smaller than half BUF_SIZE_X
///					Must be divisible by 2, because half of the boundary lies on the left of the image,
///                 half lies on the right
 
#define BUF_SIZE_X      (WIN_SIZE_X)

#define BUF_SIZE_Y		11			 // WIN_SIZE_Y + 3

// x boundary
#define BOUNDARY_X		2

// two vertical neighbours: pointer diff:
#define VERTICAL_STRIDE 65			// BOUNDARY_X + (BUF_SIZE_X / 2)

// size of one of two buffers (odd or even)
#define BUFFER_SIZE     715          // VERTICAL_STRIDE * BUF_SIZE_Y

/// padding between even and odd buffers: used to reduce bank conflicts
#define PADDING			5           // LDS_BANKS - ((BUFFER_SIZE + LDS_BANKS / 2) % LDS_BANKS);

// offset of the odd columns buffer from the beginning of data buffer
#define ODD_OFFSET 720              // BUFFER_SIZE + PADDING

// size of buffer for both even and odd columns
#define TOTAL_BUFFER_SIZE  1445      // 2 * BUFFER_SIZE + PADDING;
///////////////////////////////////////////////////////////////////////


CONSTANT sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_MIRRORED_REPEAT  | CLK_FILTER_NEAREST;

void KERNEL run(__read_only image2d_t idata, __write_only image2d_t odata,   
                       const unsigned int  width, const unsigned int  height, const unsigned int steps) {
	LOCAL int scratch[TOTAL_BUFFER_SIZE*4];
	for (int j = 0; j < WIN_SIZE_Y * steps; ++j) {
		const int2 pos = {get_global_id(0), j + get_global_id(1) *steps*WIN_SIZE_Y};
		if (pos.x < width && pos.y < height) {
			const float2 posNormal = {pos.x/(float)width, pos.y/(float)height};
			int4 pix =  read_imagei(idata, sampler, posNormal);
			vstore4(pix, 0, scratch);
			write_imagei(odata, pos,pix);
		}
	}
	
}

