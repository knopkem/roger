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

#include "OCLUtil.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include "time.h"
#include "stdarg.h"
#endif
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>

#include <string>
#include <iostream>

bool quiet = false;

// constructor - called only once
ocl_args_d_t::ocl_args_d_t():
    context(NULL),
    device(NULL),
    commandQueue(NULL)
{
    quiet = false;
}

//destructor - called only once
ocl_args_d_t::~ocl_args_d_t()
{
    if (commandQueue)
    {
        clReleaseCommandQueue(commandQueue);
    }
    if (device)
    {
        //clReleaseDevice(device);
    }
    if (context)
    {
        clReleaseContext(context);
    }
}

void LogInfo(const char* str, ...)
{
    if (str && (quiet == false))
    {
        std::cout << "INFO: " << str << std::endl;
    }
}

void LogError(const char* str, ...)
{
    if (str)
    {
        std::cerr << "ERROR: " << str << std::endl;
    }
}

// Obtains a list of OpenCL platforms available
// numPlatforms returns the number of platforms available
// Note: A memory allocation is done for platforms
// The caller should be responsible to deallocate it
cl_int GetPlatformIds(cl_platform_id **platforms, cl_uint *numPlatforms)
{
    cl_int errorCode = CL_SUCCESS;

    *platforms = NULL;
    *numPlatforms = 0;

    // Get (in numPlatforms) the number of OpenCL platforms available
    // No platform ID will be return, since platforms is NULL
    errorCode = clGetPlatformIDs(0, NULL, numPlatforms);
    if (errorCode != CL_SUCCESS)
    {
        LogError("clGetplatform_ids() to get num platforms returned %s.", TranslateOpenCLError(errorCode));
        return errorCode;
    }
    else if (*numPlatforms == 0)
    {
        LogError("No platforms found!");
        return CL_INVALID_PLATFORM;
    }

    *platforms = new cl_platform_id[*numPlatforms];
    if (*platforms == NULL)
    {
        LogError("Couldn't allocate memory for platforms!");
        return CL_OUT_OF_HOST_MEMORY;
    }

    // Now, obtains a list of numPlatforms OpenCL platforms available
    // The list of platforms available will be returned in platforms
    errorCode = clGetPlatformIDs(*numPlatforms, *platforms, NULL);
    if (errorCode != CL_SUCCESS)
    {
        LogError("clGetplatform_ids() to get platforms returned %s.", TranslateOpenCLError(errorCode));
        return errorCode;
    }

    return CL_SUCCESS;
}

// Translating the input flags to an OpenCL device type
cl_device_type TranslateDeviceType(bool preferCpu, bool preferGpu, bool preferShared)
{
    cl_device_type deviceType = CL_DEVICE_TYPE_ALL;

    // Looking for both CPU and GPU devices is like selecting CL_DEVICE_TYPE_ALL
    if (preferCpu && preferGpu)
    {
        preferCpu = false;
        preferGpu = false;
        deviceType = CL_DEVICE_TYPE_ALL;
    }
    else if (preferShared)
    {
        deviceType = CL_DEVICE_TYPE_ALL;
    }
    else if (preferCpu)
    {
        deviceType = CL_DEVICE_TYPE_CPU;
    }
    else if (preferGpu)
    {
        deviceType = CL_DEVICE_TYPE_GPU;
    }

    return deviceType;
}


// Check weather platform is an OpenCL vendor platform
cl_int CheckPreferredVendorMatch(cl_platform_id platform, const char* preferredVendor, bool* match)
{
    size_t stringLength = 0;
    cl_int errorCode = CL_SUCCESS;

    *match = false;

    // In order to read the platform vendor id, we first read the platform's vendor std::string length (param_value is NULL).
    // The value returned in stringLength
    errorCode = clGetPlatformInfo(platform, CL_PLATFORM_VENDOR, 0, NULL, &stringLength);
    if (errorCode != CL_SUCCESS)
    {
        LogError("clGetPlatformInfo() to get CL_PLATFORM_VENDOR length returned '%s'.", TranslateOpenCLError(errorCode));
        return errorCode;
    }

    // Now. that we know the platform's vendor std::string length, we can allocate space for it and read it
    char* str = new char[stringLength];
    if (str == NULL)
    {
        LogError("Couldn't allocate memory for CL_PLATFORM_VENDOR std::string.");
        return CL_OUT_OF_HOST_MEMORY;
    }

    // Read the platform's vendor std::string
    // The read value returned in str
    errorCode = clGetPlatformInfo(platform, CL_PLATFORM_VENDOR, sizeof(str), str, NULL);
    if (errorCode != CL_SUCCESS)
    {
        LogError("clGetplatform_ids() to get CL_PLATFORM_VENDOR returned %s.", TranslateOpenCLError(errorCode));
    }
    else if (strcmp(str, preferredVendor) == 0)
    {
        // The checked platform is the one we're looking for
        *match = true;
    }

    delete []str;

    return errorCode;
}


// Find and return a "prefferedVendor" OpenCL platform
// In case that prefferedVendor is NULL, the ID of the first discovered platform will be returned
cl_platform_id FindPlatformId(const char* preferredVendor, bool preferCpu, bool preferGpu, bool preferShared)
{
    cl_platform_id platform   = NULL;
    cl_platform_id *platforms = NULL;
    cl_uint numPlatforms      = 0;
    cl_int errorCode          = CL_SUCCESS;

    // Obtains a list of OpenCL platforms available
    // Note: there is a memory asllocation inside GetPlatformIds()
    errorCode = GetPlatformIds(&platforms, &numPlatforms);
    if ((platforms == NULL) || (numPlatforms == 0))
    {
        LogError("clGetplatform_ids() to get platforms returned %s.", TranslateOpenCLError(errorCode));
    }
    else {
        // Check if one of the available platform matches the preferred requirements
        cl_uint maxDevices = 0;
        cl_device_type deviceType = TranslateDeviceType(preferCpu, preferGpu, preferShared);

        for (cl_uint i = 0; i < numPlatforms; i++)
        {
            bool match = true;
            cl_uint numDevices = 0;

            // Obtains the number of deviceType devices available on platform
            // When the function failed we expect numDevices to be zero.
            // We ignore the function return value since a nonzero error code
            // could happen if this platform doesn't support the specified device type.
            clGetDeviceIDs(platforms[i], deviceType, 0, NULL, &numDevices);

            // In case the platform includes preferred deviceType continue to check it
            if (numDevices != 0)
            {
                if (preferredVendor != NULL)
                {
                    // In case we're looking for a specific vendor
                    errorCode = CheckPreferredVendorMatch(platforms[i], preferredVendor, &match);
                }

                // We don't care which OpenCL platform we found
                // So, we'll check it for the preferred device(s)
                else if (preferShared)
                {
                    // In case of preferShared (shared context) -
                    // the first platform with match devices will be selected
                    match = false;
                    if (numDevices > maxDevices)
                    {
                        maxDevices = numDevices;
                        match = true;
                    }
                }

                if (match)
                {
                    platform = platforms[i];
                }
            }
        }
    }

    delete [] platforms;

    // If we couldn't find a platform that matched the specified preferences
    // but we were otherwise successful, try to find any platform.
    if ((platform == NULL) && (errorCode == CL_SUCCESS) &&
            ((preferredVendor != NULL) || (preferCpu == true) || (preferGpu == true) || (preferShared == true)))
    {
        platform = FindPlatformId();
    }

    return platform;
}

// Create a context with the preferred devices
cl_context CreateContext(cl_platform_id platformId, bool preferCpu, bool preferGpu, bool preferShared)
{
    cl_context context = NULL;

    // If both devices are preferred, we'll try both of them separately
    if (preferCpu && preferGpu)
    {
        preferCpu = false;
        preferGpu = false;
    }

    if (preferShared)
    {
        LogInfo("Trying to create a shared context (preferred)...");
        context = CreateSharedContext(platformId);
    }
    else if (preferCpu || preferGpu)
    {
        if (preferCpu)
        {
            LogInfo("Trying to create a CPU context (preferred)...");
            context = CreateCPUContext(platformId);
        }
        else if (preferGpu)
        {
            LogInfo("Trying to create a GPU context (preferred)...");
            context = CreateGPUContext(platformId);
        }
    }
    else {
        if (context == NULL)
        {
            LogInfo("Trying to create a GPU context...");
            context = CreateGPUContext(platformId);
        }
        if (context == NULL)
        {
            LogInfo("Trying to create a CPU context...");
            context = CreateCPUContext(platformId);
        }
    }

    return context;
}

// Create a CPU based context
cl_context CreateCPUContext(cl_platform_id platformId)
{
    cl_int errorCode = CL_SUCCESS;

    cl_context_properties contextProperties[] = {CL_CONTEXT_PLATFORM, (cl_context_properties)platformId, 0};

    // Create context with all the CPU devices in the platforms.
    // The creation is synchronized (pfn_notify is NULL) and NULL user_data
    cl_context  context = clCreateContextFromType(contextProperties, CL_DEVICE_TYPE_CPU, NULL, NULL, &errorCode);
    if ((errorCode != CL_SUCCESS) || (context == NULL))
    {
        LogError("Couldn't create a CPU context, clCreateContextFromType() returned '%s'.", TranslateOpenCLError(errorCode));
    }

    return context;
}

// Create a GPU based context
cl_context CreateGPUContext(cl_platform_id platformId)
{
    cl_int errorCode = CL_SUCCESS;

    cl_context_properties contextProperties[] = {CL_CONTEXT_PLATFORM, (cl_context_properties)platformId, 0};

    // Create context with all the GPU devices in the platforms.
    // The creation is synchronized (pfn_notify is NULL) and NULL user_data
    cl_context  context = clCreateContextFromType(contextProperties, CL_DEVICE_TYPE_GPU, NULL, NULL, &errorCode);
    if ((errorCode != CL_SUCCESS) || (context == NULL))
    {
        LogError("Couldn't create a GPU context, clCreateContextFromType() returned '%s'.", TranslateOpenCLError(errorCode));
    }

    return context;
}

// Create a shared context with both CPU and GPU devices
cl_context CreateSharedContext(cl_platform_id platformId)
{
    cl_int errorCode = CL_SUCCESS;

    cl_context_properties contextProperties[] = {CL_CONTEXT_PLATFORM, (cl_context_properties)platformId, 0};

    // Create context with all the devices in the platforms.
    // The creation is synchronized (pfn_notify is NULL) and NULL user_data
    cl_context  context = clCreateContextFromType(contextProperties, CL_DEVICE_TYPE_ALL, NULL, NULL, &errorCode);
    if ((errorCode != CL_SUCCESS) || (context == NULL))
    {
        LogError("Couldn't create a shared context, clCreateContextFromType() returned '%s'.", TranslateOpenCLError(errorCode));
    }

    return context;
}


// Translate OpenCL numeric error code (errorCode) to a meaningful error std::string
const char* TranslateOpenCLError(cl_int errorCode)
{
    switch(errorCode)
    {
    case CL_SUCCESS:
        return "CL_SUCCESS";
    case CL_DEVICE_NOT_FOUND:
        return "CL_DEVICE_NOT_FOUND";
    case CL_DEVICE_NOT_AVAILABLE:
        return "CL_DEVICE_NOT_AVAILABLE";
    case CL_COMPILER_NOT_AVAILABLE:
        return "CL_COMPILER_NOT_AVAILABLE";
    case CL_MEM_OBJECT_ALLOCATION_FAILURE:
        return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
    case CL_OUT_OF_RESOURCES:
        return "CL_OUT_OF_RESOURCES";
    case CL_OUT_OF_HOST_MEMORY:
        return "CL_OUT_OF_HOST_MEMORY";
    case CL_PROFILING_INFO_NOT_AVAILABLE:
        return "CL_PROFILING_INFO_NOT_AVAILABLE";
    case CL_MEM_COPY_OVERLAP:
        return "CL_MEM_COPY_OVERLAP";
    case CL_IMAGE_FORMAT_MISMATCH:
        return "CL_IMAGE_FORMAT_MISMATCH";
    case CL_IMAGE_FORMAT_NOT_SUPPORTED:
        return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
    case CL_BUILD_PROGRAM_FAILURE:
        return "CL_BUILD_PROGRAM_FAILURE";
    case CL_MAP_FAILURE:
        return "CL_MAP_FAILURE";
    case CL_INVALID_VALUE:
        return "CL_INVALID_VALUE";
    case CL_INVALID_DEVICE_TYPE:
        return "CL_INVALID_DEVICE_TYPE";
    case CL_INVALID_PLATFORM:
        return "CL_INVALID_PLATFORM";
    case CL_INVALID_DEVICE:
        return "CL_INVALID_DEVICE";
    case CL_INVALID_CONTEXT:
        return "CL_INVALID_CONTEXT";
    case CL_INVALID_QUEUE_PROPERTIES:
        return "CL_INVALID_QUEUE_PROPERTIES";
    case CL_INVALID_COMMAND_QUEUE:
        return "CL_INVALID_COMMAND_QUEUE";
    case CL_INVALID_HOST_PTR:
        return "CL_INVALID_HOST_PTR";
    case CL_INVALID_MEM_OBJECT:
        return "CL_INVALID_MEM_OBJECT";
    case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:
        return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
    case CL_INVALID_IMAGE_SIZE:
        return "CL_INVALID_IMAGE_SIZE";
    case CL_INVALID_SAMPLER:
        return "CL_INVALID_SAMPLER";
    case CL_INVALID_BINARY:
        return "CL_INVALID_BINARY";
    case CL_INVALID_BUILD_OPTIONS:
        return "CL_INVALID_BUILD_OPTIONS";
    case CL_INVALID_PROGRAM:
        return "CL_INVALID_PROGRAM";
    case CL_INVALID_PROGRAM_EXECUTABLE:
        return "CL_INVALID_PROGRAM_EXECUTABLE";
    case CL_INVALID_KERNEL_NAME:
        return "CL_INVALID_KERNEL_NAME";
    case CL_INVALID_KERNEL_DEFINITION:
        return "CL_INVALID_KERNEL_DEFINITION";
    case CL_INVALID_KERNEL:
        return "CL_INVALID_KERNEL";
    case CL_INVALID_ARG_INDEX:
        return "CL_INVALID_ARG_INDEX";
    case CL_INVALID_ARG_VALUE:
        return "CL_INVALID_ARG_VALUE";
    case CL_INVALID_ARG_SIZE:
        return "CL_INVALID_ARG_SIZE";
    case CL_INVALID_KERNEL_ARGS:
        return "CL_INVALID_KERNEL_ARGS";
    case CL_INVALID_WORK_DIMENSION:
        return "CL_INVALID_WORK_DIMENSION";
    case CL_INVALID_WORK_GROUP_SIZE:
        return "CL_INVALID_WORK_GROUP_SIZE";
    case CL_INVALID_WORK_ITEM_SIZE:
        return "CL_INVALID_WORK_ITEM_SIZE";
    case CL_INVALID_GLOBAL_OFFSET:
        return "CL_INVALID_GLOBAL_OFFSET";
    case CL_INVALID_EVENT_WAIT_LIST:
        return "CL_INVALID_EVENT_WAIT_LIST";
    case CL_INVALID_EVENT:
        return "CL_INVALID_EVENT";
    case CL_INVALID_OPERATION:
        return "CL_INVALID_OPERATION";
    case CL_INVALID_GL_OBJECT:
        return "CL_INVALID_GL_OBJECT";
    case CL_INVALID_BUFFER_SIZE:
        return "CL_INVALID_BUFFER_SIZE";
    case CL_INVALID_MIP_LEVEL:
        return "CL_INVALID_MIP_LEVEL";
    default:
        break;
    }

    LogError("Unknown Error %08X", errorCode);
    return "*** Unknown Error ***";
}

// Initialize OpenCL environment: platform, device(s), context and command-queue
// The platform is selected and the context is created based on the data argument
// The created OpenCL objects are returned in ocl structure
int InitOpenCL(ocl_args_d_t* ocl, data_args_d_t* data)
{
    cl_int errorCode = CL_SUCCESS;

    // First find the preferred OpenCL platform that has the preferred device (CPU or GPU)
    cl_platform_id platformId = FindPlatformId(data->vendorName, data->preferCpu, data->preferGpu);
    if (platformId == NULL)
    {
        LogError("Couldn't find a platform ID.");
        return CL_INVALID_PLATFORM;
    }

    // Create an OpenCL context with the preferred devices
    ocl->context = CreateContext(platformId, data->preferCpu, data->preferGpu);
    if (ocl->context == NULL)
    {
        LogError("Couldn't create a context.");
        return CL_INVALID_CONTEXT;
    }

    // Get the first device associated with the context.
    errorCode = clGetContextInfo(ocl->context, CL_CONTEXT_DEVICES, sizeof(cl_device_id), &ocl->device, NULL);
    if (errorCode != CL_SUCCESS)
    {
        LogError("clGetContextInfo() to get list of devices returned %s.", TranslateOpenCLError(errorCode));
        return errorCode;
    }

    // Create an in-order commands-queue to the context's device
    // The commands-queue is created while profiling commands is enabled
    // So, we can capturing profiling information that measure execution time of a command.
    cl_command_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    ocl->commandQueue = clCreateCommandQueue(ocl->context, ocl->device, properties, &errorCode);
    if (errorCode != CL_SUCCESS)
    {
        LogError("clCreateCommandQueue() returned %s.", TranslateOpenCLError(errorCode));
        return errorCode;
    }

    return CL_SUCCESS;
}

// function returns time in micro-seconds
unsigned long long HostTime()
{
#if defined (_WIN32)

    LARGE_INTEGER freqInfo;

    QueryPerformanceFrequency(&freqInfo);

    // convert to frequency in micro-seconds
    static double freq = (double)freqInfo.QuadPart / 1000000.0f;

    //Generates the rdtsc instruction, which returns the processor time stamp.
    //The processor time stamp records the number of clock cycles since the last reset.
    LARGE_INTEGER ticks;

    QueryPerformanceCounter(&ticks);

    // return time in micro-seconds
    return (unsigned long long)(((double)ticks.QuadPart / freq));

#elif defined(__APPLE__)
    return 0; //TODO fix it for apple
#else
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return (unsigned long long)((tp.tv_sec * 1000000000 + tp.tv_nsec) / 1000000);
#endif
}
