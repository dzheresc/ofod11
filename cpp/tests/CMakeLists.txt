find_package(Catch2 3 CONFIG REQUIRED)

add_executable(ofod_tests
  math/normal_test.cc
)
target_link_libraries(ofod_tests PRIVATE ofod::ofod Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(ofod_tests)
