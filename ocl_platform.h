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

#include <CL/cl.h>
#include <string>

using namespace std;

typedef cl_mem tDeviceMem;
typedef cl_int tDeviceRC;
#define DeviceSuccess CL_SUCCESS

struct QueueInfo {
	QueueInfo(cl_command_queue queue) :  cmd_queue(queue)
	{}

	cl_command_queue cmd_queue;

};

struct KernelInitInfoBase : QueueInfo {

	KernelInitInfoBase(cl_command_queue queue, string bldOptions) :
		                                 QueueInfo(queue), 
										 buildOptions(bldOptions)
	{}
	KernelInitInfoBase(const KernelInitInfoBase& other) : 
		                                 QueueInfo(other.cmd_queue),
										 buildOptions(other.buildOptions)
	{
	}

	string buildOptions;
};

struct KernelInitInfo : KernelInitInfoBase {
	KernelInitInfo(cl_command_queue queue,
					string progName,
					string knlName,
					string bldOptions) : KernelInitInfoBase(queue, bldOptions), 
					                     programName(progName),
										 kernelName(knlName)
	{}
	KernelInitInfo(KernelInitInfoBase dwtInfo,
					string progName,
					string knlName) : KernelInitInfoBase(dwtInfo), 
					                     programName(progName),
										 kernelName(knlName)
	{}
	string programName;
	string kernelName;
};
