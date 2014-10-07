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

#pragma once


#include <cstdlib>
#include <cassert>
#include <string>
#include <stdexcept>
#include <sstream>
#include <typeinfo>
#include <cstdlib>
#include <algorithm>
#include <iomanip>
#include <exception>

#ifdef _WIN32
#include <Windows.h>
#endif

#include "ocl_platform.h"

// Returns textual representation of the OpenCL error code.
std::string opencl_error_to_str (cl_int error);


// Base class for all exception in samples
class Error : public std::runtime_error
{
public:
    Error (const std::string& msg) :
        std::runtime_error(msg)
    {
    }
};


// Allocates piece of aligned memory
// alignment should be a power of 2
// Out of memory situation is reported by throwing std::bad_alloc exception
void* aligned_malloc (size_t size, size_t alignment);

// Deallocates memory allocated by aligned_malloc
void aligned_free (void *aligned);

double  my_clock(void);

// Represent a given value as a std::string and enclose in quotes
template <typename T>
std::string inquotes (const T& x, const char* q = "\"")
{
    std::ostringstream ostr;
    ostr << q << x << q;
    return ostr.str();
}


// Convert from a std::string to a value of a given type.
// T should have operator>> defined to be read from stream.
template <typename T>
T str_to (const std::string& s)
{
    std::istringstream ss(s);
    T res;
    ss >> res;

    if(!ss || (ss.get(), ss))
    {
        throw Error(
            "Cannot interpret std::string " + inquotes(s) +
            " as object of type " + inquotes(typeid(T).name())
        );
    }

    return res;
}


// Convert from a value of a given type to std::string with optional formatting.
// T should have operator<< defined to be written to stream.
template <typename T>
std::string to_str (const T x, std::streamsize width = 0, char fill = ' ')
{
    using namespace std;
    ostringstream os;
    os << setw(width) << setfill(fill) << x;
    if(!os)
    {
        throw Error("Cannot represent object as a std::string");
    }
    return os.str();
}


// Report about an OpenCL problem.
// Macro is used instead of a function here
// to report source file name and line number.
#define SAMPLE_CHECK_ERRORS(ERR)                        \
    if(ERR != CL_SUCCESS)                               \
    {                                                   \
        throw Error(                                    \
            "OpenCL error " +                           \
            opencl_error_to_str(ERR) +                  \
            " happened in file " + to_str(__FILE__) +   \
            " at line " + to_str(__LINE__) + "."        \
        );                                              \
    }


// Detect if x is std::string representation of int value.
bool is_number (const std::string& x);


// Return one random number uniformally distributed in
// range [0,1] by std::rand.
// T should be a floatting point type
template <typename T>
T rand_uniform_01 ()
{
    return T(std::rand())/RAND_MAX;
}


// Fill array of a given size with random numbers
// uniformally distributed in range of [0,1] by std::rand.
// T should be a floatting point type
template <typename T>
void fill_rand_uniform_01 (T* buffer, size_t size)
{
    std::generate_n(buffer, size, rand_uniform_01<T>);
}


// Returns random index in range 0..n-1
inline size_t rand_index (size_t n)
{
    return static_cast<size_t>(std::rand()/((double)RAND_MAX + 1)*n);
}


// Returns current system time accurate enough for performance measurements
double time_stamp ();

// Follows safe procedure when exception in destructor is thrown.
void destructorException ();


// Query for several frequently used device/kernel capabilities

// Minimal alignment in bytes for memory used in clCreateBuffer with CL_MEM_USE_HOST_PTR
cl_uint requiredOpenCLAlignment (cl_device_id device);

// Maximum number of work-items in a workgroup
size_t deviceMaxWorkGroupSize (cl_device_id device);

// Maximum number of work-items that can be
// specified in each dimension of the workgroup
void deviceMaxWorkItemSizes (cl_device_id device, size_t* sizes);

// Maximum work-group size that can be used to execute
// a kernel on a specific device
size_t kernelMaxWorkGroupSize (cl_kernel kernel, cl_device_id device);


// Returns directory path of current executable.
std::string exe_dir ();


double eventExecutionTime (cl_event event);

