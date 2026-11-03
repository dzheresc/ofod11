find_package(Catch2 3 CONFIG REQUIRED)
find_package(nlohmann_json 3 CONFIG REQUIRED)

add_executable(ofod_tests
  core/compounding_test.cc
  core/date_test.cc
  credit/tranche_test.cc
  credit/credit_models_test.cc
  futures/futures_test.cc
  risk/hedging_test.cc
  risk/var_test.cc
  stats/volatility_test.cc
  math/linalg_test.cc
  stats/descriptive_test.cc
  pricers/swap_pricer_test.cc
  curves/bond_bootstrap_test.cc
  curves/zero_curve_test.cc
  golden/golden_test.cc
  instruments/bond_test.cc
  instruments/option_test.cc
  instruments/payoff_test.cc
  models/brownian_test.cc
  pricers/binomial_tree_test.cc
  pricers/black_scholes_test.cc
  random/normal_generator_test.cc
  pricers/bounds_test.cc
  pricers/numerical_test.cc
  pricers/exotics_test.cc
  pricers/models_test.cc
  pricers/rate_options_test.cc
  pricers/commodity_test.cc
  models/short_rate_test.cc
  models/lmm_test.cc
  pricers/smile_test.cc
  greeks/greeks_test.cc
  strategies/strategies_test.cc
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
