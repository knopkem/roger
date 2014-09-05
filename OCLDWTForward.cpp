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
					forward97(new OCLKernel( KernelInitInfo(initInfo, "ocldwt97.cl", "run") ))
				
											
{
}


template<typename T> OCLDWTForward<T>::~OCLDWTForward(void)
{
	if (forward53)
		delete forward53;
	if (forward97)
		delete forward97;
}

template<typename T> void OCLDWTForward<T>::doRun(bool lossy, int w, int h, int windowX, int windowY, int level) {

	OCLKernel* targetKernel = lossy?forward97:forward53;
	const int steps = divRndUp(h, 15 * windowY);
	setKernelArgs(targetKernel,w,h,steps,level);
    size_t local_work_size[3] = {windowX,1,1};
	if (lossy) {
		size_t global_offset[3] = {-4,0,0};   //left boundary

		//add one extra windowX to make up for group overlap due to boundary
	   size_t global_work_size[3] = {(divRndUp(w, windowX) + 1)* windowX, divRndUp(h, windowY * steps),1};
	   targetKernel->enqueue(2,global_offset, global_work_size, local_work_size);

	} else {
		size_t global_offset[3] = {-2,0,0};   //left boundary

		//add one extra windowX to make up for group overlap due to boundary
	   size_t global_work_size[3] = {(divRndUp(w, windowX) + 1)* windowX, divRndUp(h, windowY * steps),1};
	   targetKernel->enqueue(2,global_offset, global_work_size, local_work_size);
	}

}

template<typename T> void OCLDWTForward<T>::run(bool lossy, int w,	int h, int windowX, int windowY, int level) {

	doRun(lossy, w,h,windowX, windowY,level);
	if(level > 1) {
      // copy output's LL band back into input buffer
      const int llSizeX = divRndUp(w, 2);
      const int llSizeY = divRndUp(h, 2);

	  level--;
	  
	  tDeviceRC err = memoryManager->copyLLBandToSrc(level, llSizeX, llSizeY);
	  if (err != DeviceSuccess)
		  return;  
      
      // run remaining levels of FDWT
      run(lossy, llSizeX, llSizeY, windowX, windowY, level);
    }
}
