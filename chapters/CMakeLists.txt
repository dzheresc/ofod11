# One executable per chapter example; run with e.g. ./build/<preset>/chapters/ch04_interest_rates
add_executable(ch01_introduction ch01_introduction/cpp/payoffs.cc)
add_executable(ch04_interest_rates ch04_interest_rates/cpp/interest_rates.cc)
add_executable(ch05_forward_futures_prices
  ch05_forward_futures_prices/cpp/forward_prices.cc)

foreach(target ch01_introduction ch04_interest_rates ch05_forward_futures_prices)
  target_link_libraries(${target} PRIVATE ofod::ofod)
endforeach()
