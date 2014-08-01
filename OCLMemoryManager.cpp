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

#include "OCLMemoryManager.h"
#include <math.h>
#include "OCLBasic.h"

template<typename T> OCLMemoryManager<T>::OCLMemoryManager(ocl_args_d_t* ocl) :ocl(ocl),      
	                                        rgbBuffer(NULL),
											width(0),
											height(0),
											preprocessIn(0),
											preprocessOut(0),
											dwtOut(0)
{
}


template<typename T> OCLMemoryManager<T>::~OCLMemoryManager(void)
{
	freeBuffers();
}


template<typename T> void OCLMemoryManager<T>::fillHostInputBuffer(std::vector<T*> components, size_t w, size_t h){

	if (components.size() == 3) {
		int rgbIndex = 0;
		for (unsigned int i = 0; i < w*h; i++) {
			 rgbBuffer[rgbIndex++] = components[0][i];
			 rgbBuffer[rgbIndex++] = components[1][i];
			 rgbBuffer[rgbIndex++] = components[2][i];
		}
	} else {
		memcpy(rgbBuffer, components[0], w*h*sizeof(T));
	}
}


template<typename T>  void OCLMemoryManager<T>::init(std::vector<T*> components,	size_t w,	size_t h, bool floatingPointOnDevice){
	if (w <=0 || h <= 0 || components.size() == 0)
		return;

	if (w != width || h != height) {

		freeBuffers();
		cl_uint align = requiredOpenCLAlignment(ocl->device);
		rgbBuffer = (T*)aligned_malloc(w*h*sizeof(T) * components.size(), 4*1024);

		fillHostInputBuffer(components,w,h);

		cl_context context  = NULL;
		// Obtain the OpenCL context from the command-queue properties
		cl_int error_code = clGetCommandQueueInfo(ocl->commandQueue, CL_QUEUE_CONTEXT, sizeof(cl_context), &context, NULL);
		if (CL_SUCCESS != error_code)
		{
			LogError("Error: clGetCommandQueueInfo (CL_QUEUE_CONTEXT) returned %s.\n", TranslateOpenCLError(error_code));
			return;
		}

		preprocessIn = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR , w*h * sizeof(T) * components.size(), rgbBuffer, &error_code);
		if (CL_SUCCESS != error_code)
		{
			LogError("Error: clCreateBuffer (in) returned %s.\n", TranslateOpenCLError(error_code));
			return;
		}

		preprocessOut = clCreateBuffer(context, CL_MEM_READ_WRITE, w*h* (floatingPointOnDevice ? sizeof(float) : sizeof(int) ) * components.size(), NULL, &error_code);
		if (CL_SUCCESS != error_code)
		{
			LogError("Error: clCreateBuffer (in) returned %s.\n", TranslateOpenCLError(error_code));
			return;
		}
		
		dwtOut = clCreateBuffer(context, CL_MEM_READ_WRITE, w*h* (floatingPointOnDevice ? sizeof(float) : sizeof(int) ) * components.size(), NULL, &error_code);
		if (CL_SUCCESS != error_code)
		{
			LogError("Error: clCreateBuffer (in) returned %s.\n", TranslateOpenCLError(error_code));
			return;
		}

		width = w;
	    height = h;
	} else { 
	
	
		fillHostInputBuffer(components,width,height);

		cl_int error_code  = clEnqueueWriteBuffer(ocl->commandQueue, preprocessIn, CL_TRUE, 0, sizeof(T) * width*height * components.size(), rgbBuffer, 0, NULL, NULL);
		if (CL_SUCCESS != error_code)
		{
			LogError("Error: clEnqueueWriteImage (CL_QUEUE_CONTEXT) returned %s.\n", TranslateOpenCLError(error_code));
			return;
		}

	}

}

template<typename T> void OCLMemoryManager<T>::freeBuffers(){
	if (rgbBuffer) {
		aligned_free(rgbBuffer);
		rgbBuffer = NULL;
	}

	cl_context context  = NULL;
	// Obtain the OpenCL context from the command-queue properties
	cl_int error_code = clGetCommandQueueInfo(ocl->commandQueue, CL_QUEUE_CONTEXT, sizeof(cl_context), &context, NULL);
	if (CL_SUCCESS != error_code)
	{
		LogError("Error: clGetCommandQueueInfo (CL_QUEUE_CONTEXT) returned %s.\n", TranslateOpenCLError(error_code));
		return;
	}
	// release old buffers
	if (preprocessIn) {
		error_code = clReleaseMemObject(preprocessIn);
		if (CL_SUCCESS != error_code)
		{
			LogError("Error: clReleaseMemObject (CL_QUEUE_CONTEXT) returned %s.\n", TranslateOpenCLError(error_code));
			return;
		}
	}
	if (preprocessOut) {
		error_code = clReleaseMemObject(preprocessOut);
		if (CL_SUCCESS != error_code)
		{
			LogError("Error: clReleaseMemObject (CL_QUEUE_CONTEXT) returned %s.\n", TranslateOpenCLError(error_code));
			return;
		}
	}	
	if (dwtOut) {
		error_code = clReleaseMemObject(dwtOut);
		if (CL_SUCCESS != error_code)
		{
			LogError("Error: clReleaseMemObject (CL_QUEUE_CONTEXT) returned %s.\n", TranslateOpenCLError(error_code));
			return;
		}
	}

}
