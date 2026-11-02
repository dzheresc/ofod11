add_library(ofod
  src/core/compounding.cc
  src/credit/tranche.cc
  src/futures/margin.cc
  src/futures/rate_futures.cc
  src/futures/treasury.cc
  src/risk/hedging.cc
  src/stats/descriptive.cc
  src/curves/swap_bootstrap.cc
  src/pricers/swap_pricer.cc
  src/core/date.cc
  src/curves/bond_bootstrap.cc
  src/curves/zero_curve.cc
  src/instruments/bond.cc
  src/math/interpolation.cc
  src/math/normal.cc
  src/pricers/bond_pricer.cc
  src/pricers/forward_pricer.cc
  src/pricers/fra_pricer.cc
  src/instruments/option.cc
  src/instruments/portfolio.cc
  src/pricers/binomial_tree.cc
  src/pricers/bounds.cc
  src/pricers/black_scholes.cc
  src/pricers/finite_difference.cc
  src/pricers/monte_carlo.cc
  src/random/sobol.cc
  src/models/brownian.cc
  src/random/normal_generator.cc
  src/strategies/strategies.cc
)
add_library(ofod::ofod ALIAS ofod)

target_include_directories(ofod PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(ofod PUBLIC cxx_std_20)
find_package(Threads REQUIRED)
target_link_libraries(ofod PUBLIC Threads::Threads)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ofod PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
  if(OFOD_WARNINGS_AS_ERRORS)
    target_compile_options(ofod PRIVATE -Werror)
  endif()
endif()
set_target_properties(ofod PROPERTIES POSITION_INDEPENDENT_CODE ON)

option(OFOD_INSTALL "Generate install rules for libofod" ON)
if(OFOD_INSTALL)
include(GNUInstallDirs)
install(TARGETS ofod EXPORT ofodTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT ofodTargets NAMESPACE ofod:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ofod)
include(CMakePackageConfigHelpers)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/ofodConfig.cmake
  "include(\${CMAKE_CURRENT_LIST_DIR}/ofodTargets.cmake)\n")
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/ofodConfigVersion.cmake
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ofodConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/ofodConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ofod)
endif()
