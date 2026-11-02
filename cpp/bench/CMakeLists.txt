find_package(benchmark CONFIG REQUIRED)

add_executable(ofod_bench bench_pricers.cc)
target_link_libraries(ofod_bench PRIVATE ofod::ofod benchmark::benchmark_main)
