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

#include "OCLDWTRev.h"
#include "OCLMemoryManager.h"
#include <stdint.h>
#include "OCLDWT.cpp"



template<typename T> OCLDWTRev<T>::OCLDWTRev(KernelInitInfoBase initInfo, OCLMemoryManager<T>* memMgr) : OCLDWT<T>(initInfo, memMgr),
					reverse53(new OCLKernel( KernelInitInfo(initInfo, "ocldwt53rev.cl", "run") )),
					reverse97(new OCLKernel( KernelInitInfo(initInfo, "ocldwt97rev.cl", "run") ))
				
											
{
}


template<typename T> OCLDWTRev<T>::~OCLDWTRev(void)
{
	if (reverse53)
		delete reverse53;
	if (reverse97)
		delete reverse97;
}


template<typename T> void OCLDWTRev<T>::run(bool lossy, int w,	int h, int windowX, int windowY) {

	OCLKernel* targetKernel = lossy?reverse97:reverse53;
	const int steps = divRndUp(h, 15 * windowX);
	setKernelArgs(targetKernel,w,h,steps);
	size_t local_work_size[3] = {1,windowY,1};

	if (lossy) {

		size_t global_offset[3] = {0,-4,0};   //top boundary

		//add one extra windowX to make up for group overlap due to boundary
	    size_t global_work_size[3] = {divRndUp(w, windowX * steps),(divRndUp(h, windowY) + 1)* windowY,1};
	   
	    targetKernel->enqueue(2,global_offset, global_work_size, local_work_size);
	} else {
		size_t global_offset[3] = {0,-2,0};   //top boundary

		//add one extra windowX to make up for group overlap due to boundary
	   size_t global_work_size[3] = {divRndUp(w, windowX * steps),(divRndUp(h, windowY) + 1)* windowY,1};

	    targetKernel->enqueue(2,global_offset, global_work_size, local_work_size);
	}
}


