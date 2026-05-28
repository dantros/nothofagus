add_executable(tilemap_tests
    "tilemap_tests.cpp"
)
set_property(TARGET tilemap_tests PROPERTY CXX_STANDARD 20)
target_link_libraries(tilemap_tests PRIVATE nothofagus Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(tilemap_tests)
