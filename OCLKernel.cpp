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

#include "OCLKernel.h"


OCLKernel::OCLKernel(KernelInitInfo initInfo) : myKernel(0),
                                    queue(initInfo.cmd_queue),
                                    program(0),
                                    device(0),
                                    context(0)
{
    CreateAndBuildKernel(initInfo.programName, initInfo.kernelName, initInfo.buildOptions);
    deviceQueue = new OCLQueue(QueueInfo(queue));
}

OCLKernel::~OCLKernel(void)
{
    if (myKernel)
        clReleaseKernel(myKernel);
    if (program)
        clReleaseProgram(program);
    if (deviceQueue)
        delete deviceQueue;
}



// Upload the OpenCL C source code to output argument source
// The memory resource is implictly allocated in the function
// and should be deallocated by the caller
int ReadSourceFromFile(const char* fileName, char** source, size_t* sourceSize)
{
    int errorCode = CL_SUCCESS;

    FILE* fp = NULL;
    fopen_s(&fp, fileName, "rb");
    if (fp == NULL)
    {
        LogError("Error: Couldn't find program source file '%s'.\n", fileName);
        errorCode = CL_INVALID_VALUE;
    }
    else {
        fseek(fp, 0, SEEK_END);
        *sourceSize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        *source = new char[*sourceSize];
        if (*source == NULL)
        {
            LogError("Error: Couldn't allocate %d bytes for program source from file '%s'.\n", *sourceSize, fileName);
            errorCode = CL_OUT_OF_HOST_MEMORY;
        }
        else {
            fread(*source, 1, *sourceSize, fp);
        }
    }
    return errorCode;
}


// Create and build the OpenCL program and create the kernel
// The kernel returns in ocl
int OCLKernel::CreateAndBuildKernel(string openCLFileName, string kernelName, string buildOptions)
{
    cl_int error_code;
    size_t src_size = 0;

    // Obtaing the OpenCL context from the command-queue properties
    error_code = clGetCommandQueueInfo(queue, CL_QUEUE_CONTEXT, sizeof(cl_context), &context, NULL);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: clGetCommandQueueInfo (CL_QUEUE_CONTEXT) returned %s.\n", TranslateOpenCLError(error_code));
        goto Finish;
    }

    // Obtain the OpenCL device from the command-queue properties
    error_code = clGetCommandQueueInfo(queue, CL_QUEUE_DEVICE, sizeof(cl_device_id), &device, NULL);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: clGetCommandQueueInfo (CL_QUEUE_DEVICE) returned %s.\n", TranslateOpenCLError(error_code));
        goto Finish;
    }


    error_code = clGetDeviceInfo(device, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong), &localMemorySize, 0);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: clGetDeviceInfo (CL_DEVICE_LOCAL_MEM_SIZE) returned %s.\n", TranslateOpenCLError(error_code));
        goto Finish;
    }

    // Upload the OpenCL C source code from the input file to source
    // The size of the C program is returned in sourceSize
    char* source = NULL;
    error_code = ReadSourceFromFile(openCLFileName.c_str(), &source, &src_size);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: ReadSourceFromFile returned %s.\n", TranslateOpenCLError(error_code));
        goto Finish;
    }

    // Create program object from the OpenCL C code
    program = clCreateProgramWithSource(context, 1, (const char**)&source, &src_size, &error_code);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: clCreateProgramWithSource returned %s.\n", TranslateOpenCLError(error_code));
        goto Finish;
    }

    // Build (compile & link) the OpenCL C code
    error_code = clBuildProgram(program, 1, &device,  buildOptions.c_str(), NULL, NULL);
    if (error_code != CL_SUCCESS)
    {
        LogError("Error: clBuildProgram() for source program returned %s.\n", TranslateOpenCLError(error_code));

        // In case of error print the build log to the standard output
        // First check the size of the log
        // Then allocate the memory and obtain the log from the program
        size_t log_size = 0;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);

        char* build_log = new char[log_size];
        clGetProgramBuildInfo (program, device, CL_PROGRAM_BUILD_LOG, log_size, build_log, NULL);

        printf("Build Fail Log: \n\t%s\n", build_log);

        delete[] build_log;
        goto Finish;
    }

    // Create the required kernel
    myKernel = clCreateKernel(program, kernelName.c_str(), &error_code);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: clCreateKernel returned %s.\n", TranslateOpenCLError(error_code));
        goto Finish;
    }
Finish:
    if (source)
    {
        delete[] source;
        source = NULL;
    }
    return error_code;
}

tDeviceRC OCLKernel::execute(int dimension, size_t global_work_size[3], size_t local_work_size[3]){
	return execute(dimension, NULL, global_work_size, local_work_size);
}


tDeviceRC OCLKernel::execute(int dimension, size_t global_work_offset[3],size_t global_work_size[3], size_t local_work_size[3]){
   
	cl_int error_code = enqueue(dimension, global_work_offset ,global_work_size, local_work_size);
	if (error_code != CL_SUCCESS)
		return error_code;
    return deviceQueue->finish();
}

tDeviceRC OCLKernel::enqueue(int dimension, size_t global_work_size[3], size_t local_work_size[3]){
	return enqueue(dimension, NULL, global_work_size, local_work_size);
}

tDeviceRC OCLKernel::enqueue(int dimension, size_t global_work_offset[3], size_t global_work_size[3], size_t local_work_size[3]){
   
    // Enqueue the command to synchronously execute the kernel on the device
    // The number of dimensions to be used by the global work-items and by work-items in the work-group is 2
    // The global IDs start at offset (0, 0)
    // The command should be executed immediately (without conditions)
    cl_int error_code = clEnqueueNDRangeKernel(queue, myKernel, dimension, global_work_offset, global_work_size, local_work_size, 0, NULL, NULL);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: clEnqueueNDRangeKernel returned %s.\n", TranslateOpenCLError(error_code));
        return error_code;
    }
	return CL_SUCCESS;
}
