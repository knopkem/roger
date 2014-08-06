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

#include "OCLKernel.h"
#include <vector>
#include "OCLMemoryManager.h"



template<typename T> class OCLDWT
{
public:
	OCLDWT(KernelInitInfoBase initInfo, OCLMemoryManager<T>* memMgr);
	~OCLDWT(void);

	void encode(bool lossy, std::vector<int*> components,int w,	int h,int windowX, int windowY);
private:

	tDeviceRC setKernelArgs(OCLKernel* myKernel,int steps);
	KernelInitInfoBase initInfo;
	OCLMemoryManager<T>* memoryManager;
	OCLKernel* forward53;

};

