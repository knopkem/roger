#include "OCLDataTransferManager.h"

/*

1. 4 image frames get converted to 3 host buffers (fourth frame is distributed into three alpha channels of host buffers)

2. for each host buffer, we enqueue a write to device buffer, and receive back a generated event

3. enqueue a series of kernels, set to wait on generated event, and returning a chain of generated event

4. enqueue a read from device buffer waiting on last generated event, with a thread that checks on status of event

5. when complete, copy to SSD and continue

*/


OCLDataTransferManager::OCLDataTransferManager(ocl_args_d_t* ocl) : ocl(ocl)
{
	 HostToDevicePendingFunctor x(ocl, hostToDevicePendingQueue);
     hostToDevicePendingThread = new boost::thread(x);
}


OCLDataTransferManager::~OCLDataTransferManager(void)
{
	if (hostToDevicePendingThread) {
		hostToDevicePendingThread->join();
		delete hostToDevicePendingThread;
	}
}
