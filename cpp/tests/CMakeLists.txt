find_package(Catch2 3 CONFIG REQUIRED)
find_package(nlohmann_json 3 CONFIG REQUIRED)

add_executable(ofod_tests
  core/compounding_test.cc
  core/date_test.cc
  credit/tranche_test.cc
  futures/futures_test.cc
  risk/hedging_test.cc
  stats/descriptive_test.cc
  pricers/swap_pricer_test.cc
  curves/bond_bootstrap_test.cc
  curves/zero_curve_test.cc
  golden/golden_test.cc
  instruments/bond_test.cc
  instruments/payoff_test.cc
  math/interpolation_test.cc
  math/normal_test.cc
  math/roots_test.cc
  pricers/bond_pricer_test.cc
  pricers/forward_pricer_test.cc
  pricers/fra_pricer_test.cc
)
target_link_libraries(ofod_tests PRIVATE
  ofod::ofod Catch2::Catch2WithMain nlohmann_json::nlohmann_json)
target_compile_definitions(ofod_tests PRIVATE
  OFOD_GOLDEN_DIR="${PROJECT_SOURCE_DIR}/data/golden")

include(Catch)
catch_discover_tests(ofod_tests)
