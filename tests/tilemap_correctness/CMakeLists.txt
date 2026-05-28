add_executable(tilemap_correctness_tests
    "tilemap_correctness.cpp"
)
set_property(TARGET tilemap_correctness_tests PROPERTY CXX_STANDARD 20)
target_link_libraries(tilemap_correctness_tests PRIVATE nothofagus)

# Hand-rolled test main — exits 0 on success, 1 on failure. CTest needs only the command.
add_test(NAME tilemap_correctness COMMAND tilemap_correctness_tests)
