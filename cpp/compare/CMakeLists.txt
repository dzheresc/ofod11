# Comparison benchmarks against QuantLib; see docs/COMPARISON.md.
find_package(benchmark CONFIG REQUIRED)

# vcpkg's quantlib port installs a CMake package; Ubuntu's libquantlib0-dev
# installs only a pkg-config file.
find_package(QuantLib CONFIG QUIET)
if(TARGET QuantLib::QuantLib)
  set(OFOD_QUANTLIB QuantLib::QuantLib)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(QUANTLIB REQUIRED IMPORTED_TARGET quantlib)
  set(OFOD_QUANTLIB PkgConfig::QUANTLIB)
endif()

add_executable(ofod_compare_quantlib compare_quantlib.cc)
target_link_libraries(ofod_compare_quantlib
                      PRIVATE ofod::ofod ${OFOD_QUANTLIB} benchmark::benchmark_main)
