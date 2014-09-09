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


/**
A note about resolution levels: For a transform with N resolution levels, resolution levels run from 0 up to N-1.

**/

template<typename T> tDeviceRC OCLDWT<T>::setKernelArgs(OCLKernel* myKernel,unsigned int width, unsigned int height,int steps, int level, int levels){

	cl_int error_code =  DeviceSuccess;
	cl_kernel targetKernel = myKernel->getKernel();
	int argNum = 0;
	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(cl_mem),  memoryManager->getDwtIn(level));
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
	cl_mem* outLL = memoryManager->getDwtOut();
    if (level != levels-1)
		outLL =  memoryManager->getDwtIn(level);

	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(outLL), outLL);
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
	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(level), &level);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}	
	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(levels), &levels);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}	

	return DeviceSuccess;
}

