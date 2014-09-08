#pragma once

#include <boost/thread.hpp>
#include "concurrent_queue.h"
#include "ocl_platform.h"


struct HostToDeviceInfo{
	HostToDeviceInfo() : src(NULL), width(0), height(0), offsetX(0), offsetY(0) {

	}
	void* src;
	size_t width;
	size_t height;
	size_t offsetX;
	size_t offsetY;
};

struct HostToDeviceThreadCallable
{
	HostToDeviceThreadCallable(concurrent_queue<HostToDeviceInfo>& queue) : hostToDeviceQueue(queue) {

	}
    void operator()() {
		HostToDeviceInfo info;
		while (true) {
		    hostToDeviceQueue.wait_and_pop(info);
			//process info
			if (!info.src)
				return;
		}
	}
	concurrent_queue<HostToDeviceInfo>& hostToDeviceQueue;
};




class OCLDataManager
{
public:
	OCLDataManager(void);
	~OCLDataManager(void);
private:
	boost::thread* hostToDeviceThread;
	boost::mutex hostToDeviceMutex; 
	concurrent_queue<HostToDeviceInfo> hostToDeviceQueue;
};

