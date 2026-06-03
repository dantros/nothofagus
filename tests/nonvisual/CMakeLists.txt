add_executable(dense_land_tests
    "dense_land_tests.cpp"
)
set_property(TARGET dense_land_tests PROPERTY CXX_STANDARD 20)
target_link_libraries(dense_land_tests PRIVATE nothofagus Catch2::Catch2WithMain)

add_executable(sparse_land_tests
    "sparse_land_tests.cpp"
)
set_property(TARGET sparse_land_tests PROPERTY CXX_STANDARD 20)
target_link_libraries(sparse_land_tests PRIVATE nothofagus Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(dense_land_tests)
catch_discover_tests(sparse_land_tests)
