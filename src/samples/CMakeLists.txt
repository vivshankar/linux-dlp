# **********************************************************
# Copyright (c) 2010-2012 Google, Inc.    All rights reserved.
# Copyright (c) 2009-2010 VMware, Inc.    All rights reserved.
# **********************************************************

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
# * Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
# 
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# 
# * Neither the name of VMware, Inc. nor the names of its contributors may be
#   used to endorse or promote products derived from this software without
#   specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

cmake_minimum_required(VERSION 2.6)

# To match Makefiles and have just one build type per configured build
# dir, we collapse VS generator configs to a single choice.
# This must be done prior to the project() command and the var
# must be set in the cache.
if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  if (DEBUG)
    set(CMAKE_CONFIGURATION_TYPES "Debug" CACHE STRING "" FORCE)
  else (DEBUG)
    set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "" FORCE)
  endif (DEBUG)
  # we want to use the _LOCATION_<config> property
  string(TOUPPER "${CMAKE_CONFIGURATION_TYPES}" upper)
  set(location_suffix "_${upper}")
else ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  set(location_suffix "")
endif ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")

project(DynamoRIO_samples)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  # we don't support the Debug and Release subdirs
  foreach (config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${config}" config_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper}
      "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_upper}
      "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_upper}
      "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
  endforeach ()
endif ()

option(SHOW_RESULTS
  "Display client results in pop-up (Windows) or console message (Linux)"
  ON)
if (SHOW_RESULTS)
  add_definitions(-DSHOW_RESULTS)
endif (SHOW_RESULTS)

option(SHOW_SYMBOLS "Use symbol lookup in clients that support it" ON)
if (SHOW_SYMBOLS)
  add_definitions(-DSHOW_SYMBOLS)
endif (SHOW_SYMBOLS)
if (NOT DEFINED GENERATE_PDBS)
  # support running tests over ssh where pdb building is problematic
  set(GENERATE_PDBS ON)
endif (NOT DEFINED GENERATE_PDBS)

if (NOT DEFINED DynamoRIO_DIR)
  set(DynamoRIO_DIR "${PROJECT_SOURCE_DIR}/../cmake" CACHE PATH
    "DynamoRIO installation's cmake directory")
endif (NOT DEFINED DynamoRIO_DIR)

find_package(DynamoRIO 3.2)
if (NOT DynamoRIO_FOUND)
  message(FATAL_ERROR "DynamoRIO package required to build")
endif(NOT DynamoRIO_FOUND)

if (WIN32)
  # disable stack protection: "unresolved external symbol ___security_cookie"
  # disable the warning "unreferenced formal parameter" #4100
  # disable the warning "conditional expression is constant" #4127
  # disable the warning "cast from function pointer to data pointer" #4054
  set(CL_CFLAGS "/GS- /wd4100 /wd4127 /wd4054")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CL_CFLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CL_CFLAGS}")
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif (WIN32)

file(GLOB srcs *.c *.cpp)
# Not all clients are supported on all plaforms
string(REGEX REPLACE "[^;]*MF_moduledb.c" "" srcs "${srcs}")
if (UNIX OR CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)
  string(REGEX REPLACE "[^;]*stats.c" "" srcs "${srcs}")
endif (UNIX OR CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)

foreach (src ${srcs})
  get_filename_component(tgt ${src} NAME_WE)
  
  if ("${tgt}" STREQUAL "tracedump")
    # standalone app
    add_executable(${tgt} ${src})
    # We don't want an rpath as it makes it hard to switch
    # between debug and release at runtime (rpath is removed
    # on "make install"; here we avoid in build dir).
    # DynamoRIOConfig.cmake sets for standalone but not lib.
    set(DynamoRIO_RPATH OFF)
    configure_DynamoRIO_standalone(${tgt})

    get_target_property(tgtnm ${tgt} LOCATION)
    if (UNIX)
      set(FIND_MSG "(set LD_LIBRARY_PATH)")
    else (UNIX)
      set(FIND_MSG "(set PATH or copy to same directory)")
    endif (UNIX)
    add_custom_command(TARGET ${tgt}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND}
      ARGS -E echo "Make sure the loader finds the dynamorio library ${FIND_MSG}"
      VERBATIM)
  else ("${tgt}" STREQUAL "tracedump")
    add_library(${tgt} SHARED ${src})
    if (SHOW_SYMBOLS AND "${tgt}" STREQUAL "instrcalls" AND DR_EXT_DRSYMS_STATIC)
      # required for using drsyms built as a static library
      set(DynamoRIO_USE_LIBC ON)
    endif ()
    configure_DynamoRIO_client(${tgt})

    if (SHOW_SYMBOLS AND "${tgt}" STREQUAL "instrcalls")
      use_DynamoRIO_extension(${tgt} drsyms)
      if (DR_EXT_DRSYMS_STATIC)
        set(DynamoRIO_USE_LIBC OFF) # reset
      endif ()
    endif ()
    if ("${tgt}" STREQUAL "strace")
      use_DynamoRIO_extension(${tgt} drmgr)
    endif ()
    if ("${tgt}" STREQUAL "wrap")
      use_DynamoRIO_extension(${tgt} drwrap)
    endif ()
    if ("${tgt}" STREQUAL "memtrace")
      use_DynamoRIO_extension(${tgt} drmgr)
      use_DynamoRIO_extension(${tgt} drutil)
    else ("${tgt}" STREQUAL "memtrace")
      use_DynamoRIO_extension(${tgt} drmgr)
      use_DynamoRIO_extension(${tgt} drutil)
    endif()

    get_target_property(tgtnm ${tgt} LOCATION${location_suffix})
    add_custom_command(TARGET ${tgt}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND}
      ARGS -E echo "Usage: pass to drconfig or drrun: -client ${tgtnm} 0 \"\""
      VERBATIM)
  endif ("${tgt}" STREQUAL "tracedump")

  set(tgts ${tgts} ${tgt})
endforeach (src)

