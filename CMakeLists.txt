cmake_minimum_required(VERSION 3.16)
project(OFDMSim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# FFTW3: set FFTW_ROOT to the folder containing fftw3.h and the import library, or use vcpkg / system package.
if(NOT DEFINED FFTW_ROOT AND DEFINED ENV{FFTW_ROOT})
  set(FFTW_ROOT "$ENV{FFTW_ROOT}")
endif()

if(FFTW_ROOT)
  message(STATUS "FFTW_ROOT=${FFTW_ROOT}")
  add_executable(ofdmsim
    src/main.cpp
    src/modulation.cpp
    src/ofdm_engine.cpp
    src/ofdm_frame.cpp
    src/channel_awgn.cpp
    src/pilots.cpp
    src/equalizer.cpp
    src/simulation.cpp
    src/self_test.cpp
  )
  target_include_directories(ofdmsim PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${FFTW_ROOT}")
  target_link_directories(ofdmsim PRIVATE "${FFTW_ROOT}")
  target_link_libraries(ofdmsim PRIVATE fftw3)
else()
  message(STATUS "FFTW_ROOT not set — skipping ofdmsim target. Example: cmake -DFFTW_ROOT=/path/to/fftw ..")
endif()
