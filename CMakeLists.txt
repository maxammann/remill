# Copyright (c) 2018 Trail of Bits, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.21)
include(cmake/vcpkg_helper.cmake)

# Setup to use ccache
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ccache.cmake")

project(remill C CXX ASM)
include(GNUInstallDirs)
include(FetchContent)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/settings.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/utils.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/options.cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

if(REMILL_ENABLE_TESTING)
  include(CTest)
endif()

message(STATUS "Compiler ID is ${CMAKE_C_COMPILER_ID}")

#
# libraries
#

# Z3
find_package(Z3 CONFIG REQUIRED)

# LLVM
find_package(LLVM CONFIG REQUIRED)
llvm_map_components_to_libnames(llvm_libs
  support core irreader
  bitreader bitwriter
  passes asmprinter
  aarch64codegen aarch64asmparser
  armcodegen armasmparser
  interpreter mcjit
  nvptxdesc
  x86codegen x86asmparser
  sparccodegen sparcasmparser
  webassemblydesc)
message(STATUS "LLVM Libraries: ${llvm_libs}")

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

string(REPLACE "." ";" LLVM_VERSION_LIST ${LLVM_PACKAGE_VERSION})
list(GET LLVM_VERSION_LIST 0 LLVM_MAJOR_VERSION)
list(GET LLVM_VERSION_LIST 1 LLVM_MINOR_VERSION)

set(LLVM_MAJOR_VERSION "${LLVM_MAJOR_VERSION}")
set(LLVM_MINOR_VERSION "${LLVM_MINOR_VERSION}")

set(REMILL_LLVM_VERSION "${LLVM_MAJOR_VERSION}")

message("Remill llvm version: ${REMILL_LLVM_VERSION}")
set(REMILL_INSTALL_SEMANTICS_DIR "${CMAKE_INSTALL_PREFIX}/${REMILL_INSTALL_SHARE_DIR}/remill/${REMILL_LLVM_VERSION}/semantics" CACHE PATH "Directory into which semantics are installed")
set(REMILL_BUILD_SEMANTICS_DIR_X86 "${CMAKE_CURRENT_BINARY_DIR}/lib/Arch/X86/Runtime")
set(REMILL_BUILD_SEMANTICS_DIR_AARCH32 "${CMAKE_CURRENT_BINARY_DIR}/lib/Arch/AArch32/Runtime")
set(REMILL_BUILD_SEMANTICS_DIR_AARCH64 "${CMAKE_CURRENT_BINARY_DIR}/lib/Arch/AArch64/Runtime")
set(REMILL_BUILD_SEMANTICS_DIR_SPARC32 "${CMAKE_CURRENT_BINARY_DIR}/lib/Arch/SPARC32/Runtime")
set(REMILL_BUILD_SEMANTICS_DIR_SPARC64 "${CMAKE_CURRENT_BINARY_DIR}/lib/Arch/SPARC64/Runtime")
set(REMILL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(REMILL_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/BCCompiler.cmake")

# Intel XED
find_package(XED CONFIG REQUIRED)

# Google glog module
find_package(glog CONFIG REQUIRED)

# Google gflags
find_package(gflags CONFIG REQUIRED)

set(sleigh_ENABLE_TESTS OFF)
set(sleigh_ADDITIONAL_PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/patches/sleigh/x86-ia.patch;${CMAKE_CURRENT_SOURCE_DIR}/patches/sleigh/arm-thumb.patch" CACHE STRING "" FORCE)

# GHIDRA SLEIGH
FetchContent_Declare(sleigh
  GIT_REPOSITORY https://github.com/lifting-bits/sleigh.git
  GIT_TAG 74838a39ba7ddd91becb1d7f59e6cc3d7e3956de
)

set(sleigh_BUILD_SUPPORT ON CACHE BOOL "" FORCE)
set(sleigh_BUILD_SLEIGHSPECS ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(sleigh)

# For Linux builds, group LLVM libraries into a single group
# that avoids frustrating library ordering issues.
if(UNIX AND NOT APPLE)
  set(LINKER_START_GROUP "-Wl,--start-group")
  set(LINKER_END_GROUP "-Wl,--end-group")
else()
  set(LINKER_START_GROUP "")
  set(LINKER_END_GROUP "")
endif()

#
# Configuration options for semantics
#
option(REMILL_BARRIER_AS_NOP "Remove compiler barriers (inline assembly) in semantics" OFF)
option(REMILL_BUILD_SPARC32_RUNTIME "Build the Runtime for SPARC32. Turn this off if you have include errors with <bits/c++config.h>, or read the README for a fix" ON)

#
# target settings
#

# add everything as public.
add_library(remill_settings INTERFACE)

target_include_directories(remill_settings INTERFACE
  $<BUILD_INTERFACE:${REMILL_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>)

if(WIN32)
  # warnings and compiler settings
  target_compile_options(remill_settings INTERFACE
    "$<$<CONFIG:Debug>:/MDd>$<$<CONFIG:Release>:/MD>"
    /nologo /W3 /EHsc /wd4141 /wd4146 /wd4180 /wd4244
    /wd4258 /wd4267 /wd4291 /wd4345 /wd4351 /wd4355 /wd4456
    /wd4457 /wd4458 /wd4459 /wd4503 /wd4624 /wd4722 /wd4800
    /wd4100 /wd4127 /wd4512 /wd4505 /wd4610 /wd4510 /wd4702
    /wd4245 /wd4706 /wd4310 /wd4701 /wd4703 /wd4389 /wd4611
    /wd4805 /wd4204 /wd4577 /wd4091 /wd4592 /wd4324
  )

  target_compile_definitions(remill_settings INTERFACE
    _CRT_SECURE_NO_DEPRECATE
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_DEPRECATE
    _CRT_NONSTDC_NO_WARNINGS
    _SCL_SECURE_NO_DEPRECATE
    _SCL_SECURE_NO_WARNINGS
    GOOGLE_PROTOBUF_NO_RTTI
  )

else()
  # warnings and compiler settings
  target_compile_options(remill_settings INTERFACE
    -Wall -Wextra -Wno-unused-parameter -Wno-c++98-compat
    -Wno-unreachable-code-return -Wno-nested-anon-types
    -Wno-extended-offsetof
    -Wno-variadic-macros -Wno-return-type-c-linkage
    -Wno-c99-extensions -Wno-ignored-attributes -Wno-unused-local-typedef
    -Wno-unknown-pragmas -Wno-unknown-warning-option -fPIC
    -fno-omit-frame-pointer -fvisibility-inlines-hidden
    -fno-asynchronous-unwind-tables
  )

  # Clang-specific warnings/error options
  if("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "AppleClang")
    target_compile_options(remill_settings INTERFACE
      -Wgnu-alignof-expression -Wno-gnu-anonymous-struct -Wno-gnu-designator
      -Wno-gnu-zero-variadic-macro-arguments -Wno-gnu-statement-expression
      -fno-aligned-allocation
    )
  endif()

  # debug symbols
  if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_options(remill_settings INTERFACE
      -gdwarf-2 -g3
    )
  endif()

  # optimization flags and definitions
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(remill_settings INTERFACE
      -O0
    )
    target_compile_definitions(remill_settings INTERFACE
      "DEBUG"
    )
  else()
    target_compile_options(remill_settings INTERFACE
      -O2
    )
    target_compile_definitions(remill_settings INTERFACE
      "NDEBUG"
    )
  endif()
endif()

target_compile_definitions(remill_settings INTERFACE
  "REMILL_INSTALL_SEMANTICS_DIR=\"${REMILL_INSTALL_SEMANTICS_DIR}\""
  "REMILL_BUILD_SEMANTICS_DIR_X86=\"${REMILL_BUILD_SEMANTICS_DIR_X86}\""
  "REMILL_BUILD_SEMANTICS_DIR_AARCH32=\"${REMILL_BUILD_SEMANTICS_DIR_AARCH32}\""
  "REMILL_BUILD_SEMANTICS_DIR_AARCH64=\"${REMILL_BUILD_SEMANTICS_DIR_AARCH64}\""
  "REMILL_BUILD_SEMANTICS_DIR_SPARC32=\"${REMILL_BUILD_SEMANTICS_DIR_SPARC32}\""
  "REMILL_BUILD_SEMANTICS_DIR_SPARC64=\"${REMILL_BUILD_SEMANTICS_DIR_SPARC64}\""
)

target_link_libraries(remill_settings INTERFACE
  ${llvm_libs}
  XED::XED
  glog::glog
  gflags::gflags
  sleigh::sla
  sleigh::decomp
  sleigh::support
)

add_subdirectory(lib/Arch)
add_subdirectory(lib/BC)
add_subdirectory(lib/OS)
add_subdirectory(lib/Version)

add_library(remill INTERFACE)
target_link_libraries(remill INTERFACE
  ${LINKER_START_GROUP}
  ${llvm_libs}
  XED::XED
  glog::glog
  gflags::gflags
  sleigh::sla
  sleigh::decomp
  sleigh::support
  remill_bc
  remill_os
  remill_arch
  remill_version
  ${LINKER_END_GROUP}
)

#
# Also install clang, libllvm and llvm-link
#
set(INSTALLED_CLANG_NAME "remill-clang-${REMILL_LLVM_VERSION}${CMAKE_EXECUTABLE_SUFFIX}")
set(INSTALLED_LLVMLINK_NAME "remill-llvm-link-${REMILL_LLVM_VERSION}${CMAKE_EXECUTABLE_SUFFIX}")

InstallExternalTarget("ext_clang" "${CLANG_PATH}" "BIN" "${INSTALLED_CLANG_NAME}")

InstallExternalTarget("ext_llvmlink" "${LLVMLINK_PATH}" "BIN" "${INSTALLED_LLVMLINK_NAME}")

# additional targets
#
add_custom_target(semantics)

# shared JIT tools
add_subdirectory(test_runner_lib)

# tools
add_subdirectory(bin)

if(REMILL_ENABLE_INSTALL_TARGET)
  install(TARGETS remill EXPORT remillTargets)

  install(TARGETS remill_settings
    EXPORT remillTargets
  )

  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/remillConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/remillConfig.cmake"
    @ONLY
  )

  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/remillConfig.cmake"
    "${CMAKE_CURRENT_LIST_DIR}/cmake/vcpkg_helper.cmake"
    DESTINATION "${REMILL_INSTALL_LIB_DIR}/cmake/remill"
  )

  install(DIRECTORY "${REMILL_INCLUDE_DIR}/remill/"
    DESTINATION "${REMILL_INSTALL_INCLUDE_DIR}/remill"
  )

  install(EXPORT remillTargets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/remill")
endif()

# tests
if(REMILL_ENABLE_TESTING)
  # Tests require enabling exports on binaries
  # https://cmake.org/cmake/help/latest/variable/CMAKE_ENABLE_EXPORTS.html#variable:CMAKE_ENABLE_EXPORTS
  set(CMAKE_ENABLE_EXPORTS ON)

  find_package(Threads REQUIRED)
  add_custom_target(test_dependencies)

  if(REMILL_ENABLE_TESTING_SLEIGH_THUMB)
    message(STATUS "thumb tests enabled")
    add_subdirectory(tests/Thumb)
  endif()

  if(REMILL_ENABLE_TESTING_X86)
    message(STATUS "X86 tests enabled")
    add_subdirectory(tests/X86)
  endif()

  if(REMILL_ENABLE_TESTING_AARCH64)
    message(STATUS "aarch64 tests enabled")
    add_subdirectory(tests/AArch64)
  endif()
endif()
