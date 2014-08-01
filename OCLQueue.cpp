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

#include "OCLQueue.h"
#include "OCLUtil.h"

OCLQueue::OCLQueue(QueueInfo info) : queue(info.cmd_queue)
{
}


OCLQueue::~OCLQueue(void)
{
}


tDeviceRC OCLQueue::finish(void)
{
    // Wait until the end of the execution
    cl_int error_code = clFinish(queue);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: clFinish returned %s.\n", TranslateOpenCLError(error_code));
        return error_code;
    }
    return CL_SUCCESS;
}

tDeviceRC OCLQueue::flush(void)
{
    // Wait until the end of the execution
    cl_int error_code = clFlush(queue);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: clFinish returned %s.\n", TranslateOpenCLError(error_code));
        return error_code;
    }
    return CL_SUCCESS;
}
