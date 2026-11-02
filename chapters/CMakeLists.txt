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
add_executable(ch10_options_markets ch10_options_markets/cpp/options_markets.cc)
add_executable(ch11_option_properties ch11_option_properties/cpp/bounds.cc)
add_executable(ch12_trading_strategies ch12_trading_strategies/cpp/strategies.cc)
add_executable(ch13_binomial_trees ch13_binomial_trees/cpp/binomial_trees.cc)
add_executable(ch14_wiener_processes ch14_wiener_processes/cpp/wiener.cc)
add_executable(ch15_black_scholes ch15_black_scholes/cpp/black_scholes.cc)
add_executable(ch17_index_currency_options
  ch17_index_currency_options/cpp/index_currency.cc)
add_executable(ch18_futures_options ch18_futures_options/cpp/futures_options.cc)
add_executable(ch21_numerical_procedures
  ch21_numerical_procedures/cpp/numerical.cc)

foreach(target ch01_introduction ch02_futures_markets ch03_hedging
    ch04_interest_rates ch05_forward_futures_prices ch06_interest_rate_futures
    ch07_swaps ch08_securitization ch10_options_markets ch11_option_properties
    ch12_trading_strategies ch13_binomial_trees ch14_wiener_processes
    ch15_black_scholes ch17_index_currency_options ch18_futures_options
    ch21_numerical_procedures)
  target_link_libraries(${target} PRIVATE ofod::ofod)
endforeach()
