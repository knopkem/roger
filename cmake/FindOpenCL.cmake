# - Try to find OpenCL
# Once done this will define
#  
#  OpenCL_FOUND        - system has OpenCL
#  OpenCL_INCLUDE_DIRS - the OpenCL include directory
#  OpenCL_LIBRARIES    - link these to use OpenCL
#
# macro(opencl_add_file sources_cpp sources_h filename_cl includeFile exportName)
# Creates cpp and header from the input opencl file.
# The generated filenames are appended to the sources_cpp and sources_h inputs.

if ( OpenCL_FOUND )
  return()
endif()

macro(opencl_add_file sources_cpp sources_h filename_cl includeFile exportName)
  get_filename_component(basename "${filename_cl}" NAME_WE )
  get_filename_component(extension "${filename_cl}" EXT )
  string(REGEX REPLACE "\\." "" extension "${extension}")  #Strip leading .
  set(srcFile "${basename}_${extension}.cpp") 
  set(headerFile "${basename}_${extension}.h") 
  set(contentsFile "${filename_cl}") 
  set(varName "${basename}_${extension}")
  string(REGEX REPLACE "(.*/)" "" varName "${varName}")  

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${srcFile} ${CMAKE_CURRENT_BINARY_DIR}/${headerFile}
    COMMAND ${CMAKE_COMMAND} 
    ARGS "-DContentsFile:PATH=${contentsFile}" 
         "-DResourceHeaderFile:PATH=${CMAKE_CURRENT_BINARY_DIR}/${headerFile}"
         "-DResourceSourceFile:PATH=${CMAKE_CURRENT_BINARY_DIR}/${srcFile}"
         "-DAdditionalIncludeFile:PATH=${includeFile}"
         "-DVarName:STRING=${varName}"
         "-DExportName:STRING=${exportName}"
         "-P" "${CMAKE_SOURCE_DIR}/cmake/opencl/FileToString.cmake"
    DEPENDS ${contentsFile} ${includeFile}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    list(APPEND ${sources_cpp} "${CMAKE_CURRENT_BINARY_DIR}/${srcFile}")
    list(APPEND ${sources_h}   "${CMAKE_CURRENT_BINARY_DIR}/${headerFile}")
endmacro(opencl_add_file)

if (WIN32)

    # find out if the user asked for a 64-bit build, and use the corresponding 
    # 64 or 32 bit library paths to the search:
    if(CMAKE_SIZEOF_VOID_P MATCHES 8)
        set(_cuda32_64   x64)
        set(_ati32_64    x86_64)
        set(_intel32_64  x64)
    else()
        set(_cuda32_64   Win32)
        set(_ati32_64    x86)
        set(_intel32_64  x86)
    endif() 

    find_path(OpenCL_INCLUDE_DIRS
        CL/cl.h
        PATHS "$ENV{CUDA_PATH}/include"
              "$ENV{ATISTREAMSDKROOT}/include"
              "$ENV{ATIINTERNALSTREAMSDKROOT}/include"
              "$ENV{INTELOCLSDKROOT}/include"
        )

    find_library(OpenCL_LIBRARIES
        opencl
        PATHS "$ENV{CUDA_PATH}/lib/${_cuda32_64}"
              "$ENV{ATISTREAMSDKROOT}/lib/${_ati32_64}"
              "$ENV{ATIINTERNALSTREAMSDKROOT}/lib/${_ati32_64}"
              "$ENV{INTELOCLSDKROOT}/lib/${_intel32_64}"
        )

    unset(_cuda32_64)
    unset(_ati32_64)
    unset(_intel32_64)
else (WIN32)

    # Unix style platforms
    find_library(OpenCL_LIBRARIES OpenCL ENV LD_LIBRARY_PATH)

    get_filename_component(OPENCL_LIB_DIR ${OpenCL_LIBRARIES} PATH)
    get_filename_component(_OPENCL_INC_CAND ${OPENCL_LIB_DIR}/../../include ABSOLUTE)

    # The AMD SDK currently does not place its headers
    # in /usr/include, therefore also search relative
    # to the library
    find_path(OpenCL_INCLUDE_DIRS CL/cl.h PATHS ${_OPENCL_INC_CAND} "/usr/local/cuda/include")

endif (WIN32)

if (OpenCL_INCLUDE_DIRS)
    # CL.hpp is not bundled with OpenCL 2.0
    if (NOT EXISTS "${OpenCL_INCLUDE_DIRS}/CL/cl.hpp")
        list(APPEND OpenCL_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/cmake/opencl")
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenCL 
    "OpenCL not found. Please specify OpenCL_INCLUDE_DIRS and / or OpenCL_LIBRARIES."
    OpenCL_LIBRARIES OpenCL_INCLUDE_DIRS)

if(OpenCL_FOUND)
  message(STATUS "OpenCL found (includes: ${OpenCL_INCLUDE_DIRS}, libs: ${OpenCL_LIBRARIES})")
endif()

MARK_AS_ADVANCED(
  OpenCL_LIBRARIES
)
