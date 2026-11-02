cmake_minimum_required(VERSION 3.28)
project(ofod_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prefer an installed libofod (find_package); fall back to building it from ../cpp.
find_package(ofod CONFIG QUIET)
if(NOT ofod_FOUND)
  set(OFOD_INSTALL OFF CACHE BOOL "" FORCE)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../cpp ${CMAKE_CURRENT_BINARY_DIR}/libofod EXCLUDE_FROM_ALL)
endif()

find_package(Python 3.12 COMPONENTS Interpreter Development.Module REQUIRED)
find_package(nanobind CONFIG REQUIRED)

nanobind_add_module(_core NB_STATIC
  src/module.cc
  src/bind_core.cc
  src/bind_credit.cc
  src/bind_curves.cc
  src/bind_futures.cc
  src/bind_instruments.cc
  src/bind_math.cc
  src/bind_pricers.cc
  src/bind_risk.cc
  src/bind_stats.cc
  src/bind_strategies.cc
)
target_link_libraries(_core PRIVATE ofod::ofod)

install(TARGETS _core LIBRARY DESTINATION ofod_cpp)
