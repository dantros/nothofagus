add_executable(tilemap_tests
    "tilemap_tests.cpp"
)
set_property(TARGET tilemap_tests PROPERTY CXX_STANDARD 20)
target_link_libraries(tilemap_tests PRIVATE nothofagus Catch2::Catch2WithMain)

add_executable(sparsemap_tests
    "sparsemap_tests.cpp"
)
set_property(TARGET sparsemap_tests PROPERTY CXX_STANDARD 20)
target_link_libraries(sparsemap_tests PRIVATE nothofagus Catch2::Catch2WithMain)

add_executable(indirect_texture_tests
    "indirect_texture_tests.cpp"
)
set_property(TARGET indirect_texture_tests PROPERTY CXX_STANDARD 20)
target_link_libraries(indirect_texture_tests PRIVATE nothofagus Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(tilemap_tests)
catch_discover_tests(sparsemap_tests)
catch_discover_tests(indirect_texture_tests)
