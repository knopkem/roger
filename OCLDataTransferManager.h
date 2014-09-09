#pragma once

#include <boost/thread.hpp>
#include "concurrent_queue.h"
#include "ocl_platform.h"
#include "OCLUtil.h"


struct HostToDeviceInfo{
	HostToDeviceInfo() : src(NULL), width(0), height(0), offsetX(0), offsetY(0), dst(0) {

	}
	void* src;
	size_t width;
	size_t height;
	size_t offsetX;
	size_t offsetY;
	cl_mem dst;
};


const int QUEUE_SIZE = 4;

struct HostToDevicePendingFunctor
{
	HostToDevicePendingFunctor(ocl_args_d_t* ocl, concurrent_queue<HostToDeviceInfo>& pendingQueue) : ocl(ocl), pendingQueue(pendingQueue) {
	  for(int i = 0; i < QUEUE_SIZE; ++i) {
		// availableEventsQueue.push(returned_event[i]);
	  }
	}
    void operator()() {
		HostToDeviceInfo info;
		while (true) {
		    pendingQueue.wait_and_pop(info);

			if (!info.src)
				return;
			size_t origin[] = {info.offsetX,info.offsetY,0}; // Defines the offset in pixels in the image from where to write.
			size_t region[] = {info.width, info.height, 1}; // Size of object to be transferred
			cl_event returned_event; 
			cl_int error_code = clEnqueueWriteImage(ocl->commandQueue, info.dst, CL_FALSE, origin, region,0,0, info.src, 0, NULL,&returned_event);
			if (CL_SUCCESS != error_code)
			{
				LogError("Error: clEnqueueWriteImage (CL_QUEUE_CONTEXT) returned %s.\n", TranslateOpenCLError(error_code));
			}
		}
	}
	concurrent_queue<HostToDeviceInfo>& pendingQueue;
	ocl_args_d_t* ocl;
	cl_event returned_event[QUEUE_SIZE];
	//concurrent_queue<cl_event> availableEventsQueue;
};


class OCLDataTransferManager
{
public:
	OCLDataTransferManager(ocl_args_d_t* ocl);
	~OCLDataTransferManager(void);
private:
	boost::thread* hostToDevicePendingThread;
	concurrent_queue<HostToDeviceInfo> hostToDevicePendingQueue;
	concurrent_queue<HostToDeviceInfo> hostToDeviceCompleteQueue;
	ocl_args_d_t* ocl;

};

