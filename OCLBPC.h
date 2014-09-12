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
#include "OCLMemoryManager.h"



template<typename T> class OCLBPC
{
public:
	OCLBPC(KernelInitInfoBase initInfo, OCLMemoryManager<T>* memMgr);
	~OCLBPC(void);
	void run(size_t codeblockX, size_t codeblockY);
private:
	tDeviceRC setKernelArgs(unsigned int codeblockX, unsigned int codeblockY);
	KernelInitInfoBase initInfo;
	OCLMemoryManager<T>* memoryManager;
	OCLKernel* bpc;
};

