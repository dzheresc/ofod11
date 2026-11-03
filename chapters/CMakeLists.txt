# One executable per chapter example; run with e.g. ./build/<preset>/chapters/ch04_interest_rates
add_executable(ch01_introduction ch01_introduction/cpp/payoffs.cc)
add_executable(ch02_futures_markets ch02_futures_markets/cpp/margin_account.cc)
add_executable(ch03_hedging ch03_hedging/cpp/hedging.cc)
add_executable(ch04_interest_rates ch04_interest_rates/cpp/interest_rates.cc)
add_executable(ch05_forward_futures_prices
  ch05_forward_futures_prices/cpp/forward_prices.cc)
add_executable(ch06_interest_rate_futures
  ch06_interest_rate_futures/cpp/rate_futures.cc)
add_executable(ch07_swaps ch07_swaps/cpp/swaps.cc)
add_executable(ch08_securitization ch08_securitization/cpp/waterfall.cc)
add_executable(ch09_xva ch09_xva/cpp/xva.cc)
add_executable(ch10_options_markets ch10_options_markets/cpp/options_markets.cc)
add_executable(ch11_option_properties ch11_option_properties/cpp/bounds.cc)
add_executable(ch12_trading_strategies ch12_trading_strategies/cpp/strategies.cc)
add_executable(ch13_binomial_trees ch13_binomial_trees/cpp/binomial_trees.cc)
add_executable(ch14_wiener_processes ch14_wiener_processes/cpp/wiener.cc)
add_executable(ch15_black_scholes ch15_black_scholes/cpp/black_scholes.cc)
add_executable(ch16_employee_stock_options ch16_employee_stock_options/cpp/eso.cc)
add_executable(ch17_index_currency_options
  ch17_index_currency_options/cpp/index_currency.cc)
add_executable(ch18_futures_options ch18_futures_options/cpp/futures_options.cc)
add_executable(ch19_greek_letters ch19_greek_letters/cpp/greeks.cc)
add_executable(ch20_volatility_smiles ch20_volatility_smiles/cpp/smiles.cc)
add_executable(ch21_numerical_procedures
  ch21_numerical_procedures/cpp/numerical.cc)
add_executable(ch22_var_es ch22_var_es/cpp/var.cc)
add_executable(ch23_volatility_correlation
  ch23_volatility_correlation/cpp/volatility.cc)
add_executable(ch24_credit_risk ch24_credit_risk/cpp/credit.cc)
add_executable(ch25_credit_derivatives ch25_credit_derivatives/cpp/cds.cc)
add_executable(ch26_exotic_options ch26_exotic_options/cpp/exotics.cc)
add_executable(ch27_more_models ch27_more_models/cpp/models.cc)
add_executable(ch28_martingales_measures
  ch28_martingales_measures/cpp/measures.cc)
add_executable(ch29_market_models ch29_market_models/cpp/market_models.cc)
add_executable(ch30_convexity_timing_quanto
  ch30_convexity_timing_quanto/cpp/adjustments.cc)
add_executable(ch31_short_rate_models ch31_short_rate_models/cpp/short_rate.cc)
add_executable(ch32_no_arbitrage_models ch32_no_arbitrage_models/cpp/trees.cc)
add_executable(ch33_forward_rates ch33_forward_rates/cpp/forward_rates.cc)
add_executable(ch34_swaps_revisited ch34_swaps_revisited/cpp/swaps.cc)
add_executable(ch35_commodity_derivatives
  ch35_commodity_derivatives/cpp/commodities.cc)
add_executable(ch36_real_options ch36_real_options/cpp/real_options.cc)
add_executable(ch37_derivatives_mishaps
  ch37_derivatives_mishaps/cpp/mishaps.cc)

foreach(target ch01_introduction ch02_futures_markets ch03_hedging
    ch04_interest_rates ch05_forward_futures_prices ch06_interest_rate_futures
    ch07_swaps ch08_securitization ch09_xva ch10_options_markets
    ch11_option_properties
    ch12_trading_strategies ch13_binomial_trees ch14_wiener_processes
    ch15_black_scholes ch16_employee_stock_options ch17_index_currency_options
    ch18_futures_options ch19_greek_letters ch20_volatility_smiles
    ch21_numerical_procedures ch22_var_es ch23_volatility_correlation
    ch24_credit_risk ch25_credit_derivatives ch26_exotic_options
    ch27_more_models ch28_martingales_measures ch29_market_models
    ch30_convexity_timing_quanto ch31_short_rate_models
    ch32_no_arbitrage_models ch33_forward_rates ch34_swaps_revisited
    ch35_commodity_derivatives ch36_real_options ch37_derivatives_mishaps)
  target_link_libraries(${target} PRIVATE ofod::ofod)
endforeach()

# Examples that read the sample series in data/market.
foreach(target ch22_var_es ch23_volatility_correlation)
  target_compile_definitions(${target} PRIVATE
    OFOD_DATA_DIR="${PROJECT_SOURCE_DIR}/data/market")
endforeach()
