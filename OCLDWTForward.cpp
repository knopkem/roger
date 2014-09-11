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

#pragma once

#include "OCLDWTForward.h"
#include "OCLMemoryManager.h"
#include <stdint.h>
#include "OCLDWT.cpp"


template<typename T> OCLDWTForward<T>::OCLDWTForward(KernelInitInfoBase initInfo, OCLMemoryManager<T>* memMgr) : OCLDWT<T>(initInfo, memMgr),
	               	forward53(new OCLKernel( KernelInitInfo(initInfo, "ocldwt53.cl", "run") )),
					forward97(new OCLKernel( KernelInitInfo(initInfo, "ocldwt97.cl", "runWithQuantization") ))
				
											
{
}


template<typename T> OCLDWTForward<T>::~OCLDWTForward(void)
{
	if (forward53)
		delete forward53;
	if (forward97)
		delete forward97;
}

template<typename T> void OCLDWTForward<T>::doRun(bool lossy, size_t w, size_t h, size_t windowX, size_t windowY, size_t level, size_t levels, float quantLL, float quantLH, float quantHH) {

	OCLKernel* targetKernel = lossy?forward97:forward53;
	const size_t steps = divRndUp(w, 15 * windowX);
	if (setKernelArgs(targetKernel,w,h,steps,level,levels) != DeviceSuccess)
		return;
	if (lossy) {
		if (setKernelArgsQuant(targetKernel,level, levels, quantLL, quantLH, quantHH) != DeviceSuccess)
			return;

	}
    size_t local_work_size[3] = {1,windowY,1};
	if (lossy) {
		size_t global_offset[3] = {0,-4,0};   //left boundary

		//add one extra windowY to make up for group overlap due to boundary
	   size_t global_work_size[3] = {divRndUp(w, windowX * steps), (divRndUp(h, windowY) + 1)* windowY,1};
	   targetKernel->enqueue(2,global_offset, global_work_size, local_work_size);

	} else {
		size_t global_offset[3] = {0,-2,0};   //left boundary

		//add one extra windowY to make up for group overlap due to boundary
	   size_t global_work_size[3] = {divRndUp(w, windowX * steps), (divRndUp(h, windowY) + 1)* windowY,1};
	   targetKernel->enqueue(2,global_offset, global_work_size, local_work_size);
	}

}

template<typename T> void OCLDWTForward<T>::run(bool lossy, size_t w,	size_t h, size_t windowX, size_t windowY, size_t level, size_t levels, float quantLL, float quantLH, float quantHH) {

	doRun(lossy, w,h,windowX, windowY,level,levels, quantLL, quantLH, quantHH);
	if(level < levels-1) {
      // copy output's LL band back into input buffer
      const size_t llSizeX = divRndUp(w, 2);
      const size_t llSizeY = divRndUp(h, 2);

	  level++;
	  
      // run remaining levels of FDWT
      run(lossy, llSizeX, llSizeY, windowX, windowY, level,levels, quantLL, quantLH, quantHH);
    }
}
