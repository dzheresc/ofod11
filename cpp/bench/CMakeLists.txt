find_package(benchmark CONFIG REQUIRED)

add_executable(ofod_bench bench_pricers.cc bench_library.cc)
target_link_libraries(ofod_bench PRIVATE ofod::ofod benchmark::benchmark_main)

add_executable(ofod_heap_driver heap_driver.cc)
target_link_libraries(ofod_heap_driver PRIVATE ofod::ofod)
