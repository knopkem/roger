
/*  Copyright 2014 Aaron Boxer (boxerab@gmail.com)

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

CONSTANT sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE  | CLK_FILTER_NEAREST;


void KERNEL run(read_only image2d_t idata, const unsigned int  width, const unsigned int height, 
						write_only image2d_t R,
						 write_only image2d_t G, 
						 write_only image2d_t B,
						 write_only image2d_t A ) {

        int x = getLocalId(0) + getGroupId(0) * WIN_SIZE_X;
		int y = getLocalId(1) + getGroupId(1) * WIN_SIZE_Y;
		if (x >=width || y >= height)
			return;
		int2 pos = (int2)(x,y);
		int4 pix = read_imagei(idata, sampler, pos);
		write_imagei(R,pos, (int4)(pix.x,0,0,0));
		write_imagei(G,pos, (int4)(pix.y,0,0,0));
		write_imagei(B,pos, (int4)(pix.z,0,0,0));
		write_imagei(A,pos, (int4)(pix.w,0,0,0));


}