#include "OCLDataManager.h"

OCLDataManager::OCLDataManager(void)
{
	 HostToDeviceThreadCallable x(hostToDeviceQueue);
     hostToDeviceThread = new boost::thread(x);
}


OCLDataManager::~OCLDataManager(void)
{
	if (hostToDeviceThread) {
		hostToDeviceThread->join();
		delete hostToDeviceThread;
	}
}
