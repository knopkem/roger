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

#pragma once

#include "OCLMemoryManager.h"
#include <math.h>
#include "OCLBasic.h"

template<typename T> OCLMemoryManager<T>::OCLMemoryManager(ocl_args_d_t* ocl, bool lossy, bool outputDwt) :ocl(ocl),
    rgbBuffer(NULL),
    width(0),
    height(0),
    _levels(0),
    _precision(0),
    numComponents(0),
    lossy(lossy),
    dwtOut(0),
    onlyDwtOut(outputDwt)
{

}

template<typename T> OCLMemoryManager<T>::~OCLMemoryManager(void)
{
    freeBuffers();
}

template<typename T> void OCLMemoryManager<T>::fillHostInputBuffer(std::vector<T*> components, size_t w, size_t h) {

    if (components.size() == 4) {
        int rgbIndex = 0;
        for (unsigned int i = 0; i < w*h; i++) {
            rgbBuffer[rgbIndex++] = components[0][i];
            rgbBuffer[rgbIndex++] = components[1][i];
            rgbBuffer[rgbIndex++] = components[2][i];
            rgbBuffer[rgbIndex++] = components[3][i];
        }
    } else {
        memcpy(rgbBuffer, components[0], w*h*sizeof(T));
    }
}

template<typename T>  void OCLMemoryManager<T>::init(std::vector<T*> components,	size_t w,	size_t h, size_t levels,size_t precision) {
    if (w <=0 || h <= 0 || components.size() == 0 || levels <= 0)
        return;

    if (w != width || h != height || levels != _levels || precision != _precision) {
        width = w;
        height = h;
        _levels = levels;
        _precision = precision;
        numComponents = components.size();
        freeBuffers();
        cl_uint align = requiredOpenCLAlignment(ocl->device);
        rgbBuffer = (T*)aligned_malloc(w*h*sizeof(T) * numComponents, 4*1024);

        fillHostInputBuffer(components,w,h);

        cl_context context  = NULL;
        // Obtain the OpenCL context from the command-queue properties
        LogInfo("trying to get opencl context");
        cl_int error_code = clGetCommandQueueInfo(ocl->commandQueue, CL_QUEUE_CONTEXT, sizeof(cl_context), &context, NULL);
        if (CL_SUCCESS != error_code || !context)
        {
            LogError("clGetCommandQueueInfo (CL_QUEUE_CONTEXT) returned %s.", TranslateOpenCLError(error_code));
            return;
        }

#if defined(CL_VERSION_1_2)
        //allocate output image(s)
        cl_image_desc desc;
        desc.image_type = CL_MEM_OBJECT_IMAGE2D;
        desc.image_width = w;
        desc.image_height = h;
        desc.image_depth = 0;
        desc.image_array_size = 0;
        desc.image_row_pitch = 0;
        desc.image_slice_pitch = 0;
        desc.num_mip_levels = 0;
        desc.num_samples = 0;
        desc.buffer = NULL;

        cl_image_format format;
        format.image_channel_order = numComponents == 4 ? CL_RGBA : CL_R;
        format.image_channel_data_type = (onlyDwtOut && lossy) ? CL_FLOAT : CL_SIGNED_INT16;

        LogInfo("trying to create image");
        dwtOut = clCreateImage (context, CL_MEM_READ_WRITE, &format, &desc, NULL,&error_code);
        if (CL_SUCCESS != error_code)
        {
            LogError("clCreateImage (CL_QUEUE_CONTEXT) returned %s.", TranslateOpenCLError(error_code));
            return;
        }

        if (numComponents == 4) {
            format.image_channel_order = CL_R;
            format.image_channel_data_type = CL_SIGNED_INT16;
            for(int i = 0; i < 4; ++i) {
                cl_mem temp = clCreateImage (context, CL_MEM_READ_WRITE, &format, &desc, NULL,&error_code);
                if (CL_SUCCESS != error_code)
                {
                    LogError("clCreateImage (CL_QUEUE_CONTEXT) returned %s.", TranslateOpenCLError(error_code));
                    return;
                }
                dwtOutChannels.push_back(temp);

            }
        }

        //allocate input images
        format.image_channel_order = numComponents == 4 ? CL_RGBA : CL_R;
        format.image_channel_data_type = lossy ? CL_FLOAT : CL_SIGNED_INT16;
        for (size_t i =0; i < levels; ++i) {
            cl_mem temp = clCreateImage (context, CL_MEM_READ_WRITE, &format, &desc, NULL,&error_code);
            if (CL_SUCCESS != error_code)
            {
                LogError("clCreateImage (CL_QUEUE_CONTEXT) returned %s.", TranslateOpenCLError(error_code));
                return;
            }
            dwtIn.push_back(temp);
            desc.image_width = divRndUp(desc.image_width, 2);
            desc.image_height = divRndUp(desc.image_height, 2);
        }
#else
    #error "OpenCL versions below 1.2 currently not supported"
#endif // #if defined(CL_VERSION_1_2)

        hostToDWTIn();

    } else {
        fillHostInputBuffer(components,width,height);
        hostToDWTIn();
    }

}

template<typename T> tDeviceRC OCLMemoryManager<T>::hostToDWTIn() {
    size_t origin[] = {0,0,0}; // Defines the offset in pixels in the image from where to write.
    size_t region[] = {width, height, 1}; // Size of object to be transferred
    cl_int error_code = clEnqueueWriteImage(ocl->commandQueue, dwtIn[0], CL_TRUE, origin, region,0,0, rgbBuffer, 0, NULL,NULL);
    if (CL_SUCCESS != error_code)
    {
        LogError("clEnqueueWriteImage (CL_QUEUE_CONTEXT) returned %s.", TranslateOpenCLError(error_code));
    }
    return error_code;

}

template<typename T> tDeviceRC OCLMemoryManager<T>::mapImage(cl_mem img, void** mappedPtr) {
    if (!mappedPtr)
        return -1;

    cl_int error_code = CL_SUCCESS;
    size_t image_dimensions[3] = { width, height, 1 };
    size_t image_origin[3] = { 0, 0, 0 };
    size_t image_pitch = 0;

    *mappedPtr = clEnqueueMapImage(   ocl->commandQueue,
                                      img,
                                      CL_TRUE,
                                      CL_MAP_READ,
                                      image_origin,
                                      image_dimensions,
                                      &image_pitch,
                                      NULL,
                                      0,
                                      NULL,
                                      NULL,
                                      &error_code);
    if (CL_SUCCESS != error_code)
    {
        LogError("clEnqueueMapImage return %s.", TranslateOpenCLError(error_code));

    }

    return error_code;
}

template<typename T> tDeviceRC OCLMemoryManager<T>::mapBuffer(cl_mem buffer, void** mappedPtr) {
    if (!mappedPtr)
        return -1;

    cl_int error_code = CL_SUCCESS;
    *mappedPtr = clEnqueueMapImage(   ocl->commandQueue,
                                      buffer,
                                      CL_TRUE,
                                      CL_MAP_READ,
                                      0,
                                      width*height*sizeof(short),
                                      0
                                      NULL,
                                      NULL,
                                      &error_code);
    if (CL_SUCCESS != error_code)
    {
        LogError("clEnqueueMapBuffer return %s.", TranslateOpenCLError(error_code));

    }

    return error_code;
}

template<typename T> tDeviceRC OCLMemoryManager<T>::unmapMemory(cl_mem img, void* mappedPtr) {
    if (!mappedPtr)
        return -1;

    cl_int error_code = clEnqueueUnmapMemObject( ocl->commandQueue, img, mappedPtr, 0,NULL,NULL);
    if (CL_SUCCESS != error_code)
    {
        LogError("clEnqueueUnmapMemObject return %s.", TranslateOpenCLError(error_code));

    }
    return error_code;

}

template<typename T> void OCLMemoryManager<T>::freeBuffers() {
    if (rgbBuffer) {
        aligned_free(rgbBuffer);
        rgbBuffer = NULL;
    }

    cl_context context  = NULL;
    // Obtain the OpenCL context from the command-queue properties
    cl_int error_code = clGetCommandQueueInfo(ocl->commandQueue, CL_QUEUE_CONTEXT, sizeof(cl_context), &context, NULL);
    if (CL_SUCCESS != error_code)
    {
        LogError("clGetCommandQueueInfo (CL_QUEUE_CONTEXT) returned %s.", TranslateOpenCLError(error_code));
        return;
    }
    // release old buffers
    for(std::vector<cl_mem>::iterator it = dwtIn.begin(); it != dwtIn.end(); ++it) {
        error_code = clReleaseMemObject(*it);
        if (CL_SUCCESS != error_code)
        {
            LogError("clReleaseMemObject (CL_QUEUE_CONTEXT) returned %s.", TranslateOpenCLError(error_code));
            return;
        }
    }

    for(std::vector<cl_mem>::iterator it = dwtOutChannels.begin(); it != dwtOutChannels.end(); ++it) {
        error_code = clReleaseMemObject(*it);
        if (CL_SUCCESS != error_code)
        {
            LogError("clReleaseMemObject (CL_QUEUE_CONTEXT) returned %s.", TranslateOpenCLError(error_code));
            return;
        }
    }

    if (dwtOut) {
        error_code = clReleaseMemObject(dwtOut);
        if (CL_SUCCESS != error_code)
        {
            LogError("clReleaseMemObject (CL_QUEUE_CONTEXT) returned %s.", TranslateOpenCLError(error_code));
            return;
        }
    }

}
