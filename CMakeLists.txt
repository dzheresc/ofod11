cmake_minimum_required(VERSION 3.28)

project(ofod11 VERSION 0.1.0 LANGUAGES CXX
        DESCRIPTION "Code companion for Hull, Options, Futures and Other Derivatives (11th ed.)")

option(OFOD_BUILD_TESTS "Build C++ tests" ON)
option(OFOD_BUILD_BENCH "Build benchmarks" OFF)
option(OFOD_BUILD_COMPARE "Build the QuantLib comparison benchmarks" OFF)
option(OFOD_BUILD_CHAPTERS "Build chapter examples" ON)
option(OFOD_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

add_subdirectory(cpp)

if(OFOD_BUILD_TESTS)
  enable_testing()
  add_subdirectory(cpp/tests)
endif()

if(OFOD_BUILD_BENCH)
  add_subdirectory(cpp/bench)
endif()

if(OFOD_BUILD_COMPARE)
  add_subdirectory(cpp/compare)
endif()

if(OFOD_BUILD_CHAPTERS)
  add_subdirectory(chapters)
endif()
