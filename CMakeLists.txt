cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(VitaRecorder)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O3 -std=gnu99 -DTAIPOOL_AS_STDLIB")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti -fno-exceptions")

include_directories(
)

link_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
)

if (NOT ${RELEASE})
  add_definitions(-DENABLE_LOGGING)
endif()

add_executable(VitaRecorder
  main.c
  renderer.c
  encoder.c
  rescaler.c
)

target_link_libraries(VitaRecorder
  taipool
  taihen_stub
  SceLibKernel_stub_weak
  SceDisplay_stub_weak
  SceSysmem_stub_weak
  jpeg
  k
  gcc
  SceIofilemgr_stub_weak
  SceKernelThreadMgr_stub
  SceSysmodule_stub_weak
  ScePower_stub_weak
  SceJpegEnc_stub_weak
  SceAppMgr_stub_weak
  SceCtrl_stub_weak
  SceRtc_stub_weak
)

set_target_properties(VitaRecorder
  PROPERTIES LINK_FLAGS "-nostdlib"
)

vita_create_self(VitaRecorder.suprx VitaRecorder
  CONFIG ${CMAKE_SOURCE_DIR}/VitaRecorder.yml
)
