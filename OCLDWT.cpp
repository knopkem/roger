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
					memoryManager(memMgr)
				
											
{
}


template<typename T> OCLDWT<T>::~OCLDWT(void)
{
}



template <typename T> tDeviceRC OCLDWT<T>::copyLLBandToSrc(int LLSizeX, int LLSizeY)
{
	
 // copy forward or reverse transformed LL band from output back into the input
	size_t origin[] = { 0, 0, 0};
	cl_int err = CL_SUCCESS;

	// The region size in pixels
	size_t region[] = {LLSizeX, LLSizeY, 1 };
			
	err = clEnqueueCopyImage  ( initInfo.cmd_queue, 	//copy command will be queued
					*memoryManager->getPreprocessOut(),		
					*memoryManager->getDwtOut(),		
					origin,	    // origin of source image
					origin,     // origin of destination image
					region,		//(width, height, depth) in pixels of the 2D or 3D rectangle being copied
					0,
					NULL,
					NULL);
					
	if (CL_SUCCESS != err)
	{
		LogError("Error: clEnqueueCopyImage (srcMem) returned %s.\n", TranslateOpenCLError(err));
	}
	
	return err;

}


template<typename T> tDeviceRC OCLDWT<T>::setKernelArgs(OCLKernel* myKernel,unsigned int width, unsigned int height,int steps){

	cl_int error_code =  DeviceSuccess;
	cl_kernel targetKernel = myKernel->getKernel();
	int argNum = 0;
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

