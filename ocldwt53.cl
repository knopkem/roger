#include "ocl_platform.cl"

CONSTANT sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_MIRRORED_REPEAT  | CLK_FILTER_NEAREST;

void KERNEL run(__read_only image2d_t idata, __write_only image2d_t odata,    const unsigned int  width, const unsigned int  height) {
	const int2 pos = {get_global_id(0), get_global_id(1)};
	const float2 posNormal = {get_global_id(0)/(float)width, get_global_id(1)/(float)height};
    write_imagei(odata, pos, read_imagei(idata, sampler, posNormal));
}

