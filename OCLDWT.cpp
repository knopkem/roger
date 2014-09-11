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


inline size_t divRndUp(const size_t n, const size_t d) {
	return n/d + !!(n % d); 
  }


template<typename T> OCLDWT<T>::OCLDWT(KernelInitInfoBase initInfo, OCLMemoryManager<T>* memMgr) : 
	                initInfo(initInfo),
					memoryManager(memMgr),
					numKernelArgs(0)

				
											
{
}


template<typename T> OCLDWT<T>::~OCLDWT(void)
{
}


template<typename T> tDeviceRC OCLDWT<T>::setKernelArgs(OCLKernel* myKernel,size_t width, size_t height,size_t steps, size_t level, size_t levels){
	numKernelArgs = 0;
	cl_kernel targetKernel = myKernel->getKernel();
	cl_int error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(cl_mem),  memoryManager->getDwtIn(level));
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}

	error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(cl_mem), (level < levels-1) ?
		                                                    memoryManager->getDwtIn(level+1) :  memoryManager->getOutput() );
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}

	error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(cl_mem), memoryManager->getOutput());
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}

	error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(width), &width);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(height), &height);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(steps), &steps);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}	
	return DeviceSuccess;
}



/**
A note about resolution levels: For a transform with N resolution levels, resolution levels run from 0 up to N-1.

**/
template<typename T> tDeviceRC OCLDWT<T>::setKernelArgsQuant(OCLKernel* myKernel, size_t level, size_t levels, float quantLL, float quantLH, float quantHH){

	cl_kernel targetKernel = myKernel->getKernel();
	cl_int error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(level), &level);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(levels), &levels);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}	

	error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(quantLL), &quantLL);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}	
	error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(quantLH), &quantLH);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(quantHH), &quantHH);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}	

	return DeviceSuccess;
}
