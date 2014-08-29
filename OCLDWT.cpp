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

#include "OCLDWT.h"
#include "OCLMemoryManager.h"
#include <stdint.h>


inline int divRndUp(const int n, const int d) {
	return n/d + !!(n % d); 
  }


template<typename T> OCLDWT<T>::OCLDWT(KernelInitInfoBase initInfo, OCLMemoryManager<T>* memMgr) : 
	                initInfo(initInfo),
					memoryManager(memMgr),
					forward53(new OCLKernel( KernelInitInfo(initInfo, "ocldwt53.cl", "run") )),
					forward97(new OCLKernel( KernelInitInfo(initInfo, "ocldwt97.cl", "run") ))
				
											
{
}


template<typename T> OCLDWT<T>::~OCLDWT(void)
{
	if (forward53)
		delete forward53;
	if (forward97)
		delete forward97;
}

template<typename T> void OCLDWT<T>::encode(bool lossy, std::vector<T*> components,	int w,	int h, int windowX, int windowY) {

	if (components.size() == 0)
		return;
	if (lossy) {
		OCLKernel* targetKernel = forward97;
		memoryManager->init(components,w,h,lossy);
		const int steps = divRndUp(h, 15 * windowY);
		setKernelArgs(targetKernel,steps);

		size_t global_offset[3] = {-2,0,0};   //left boundary

		//add one extra windowX to make up for group overlap due to boundary
	   size_t global_work_size[3] = {(divRndUp(w, windowX) + 1)* windowX, divRndUp(h, windowY * steps),1};
	   size_t local_work_size[3] = {windowX,1,1};

	   targetKernel->enqueue(2,global_offset, global_work_size, local_work_size);

	} else {
		OCLKernel* targetKernel = forward53;
		memoryManager->init(components,w,h,lossy);
		const int steps = divRndUp(h, 15 * windowY);
		setKernelArgs(targetKernel,steps);

		size_t global_offset[3] = {-2,0,0};   //left boundary

		//add one extra windowX to make up for group overlap due to boundary
	   size_t global_work_size[3] = {(divRndUp(w, windowX) + 1)* windowX, divRndUp(h, windowY * steps),1};
	   size_t local_work_size[3] = {windowX,1,1};

	  targetKernel->enqueue(2,global_offset, global_work_size, local_work_size);

	}
}


template<typename T> tDeviceRC OCLDWT<T>::setKernelArgs(OCLKernel* myKernel,int steps){

	cl_int error_code =  DeviceSuccess;
	cl_kernel targetKernel = myKernel->getKernel();
	int argNum = 0;
	unsigned int width = (unsigned int)memoryManager->getWidth();
	unsigned int height = (unsigned int)memoryManager->getHeight();

	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(cl_mem),  memoryManager->getPreprocessOut());
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}

	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(cl_mem), memoryManager->getDwtOut());
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(width), &width);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(height), &height);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(steps), &steps);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}	
	return DeviceSuccess;
}

