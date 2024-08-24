add_executable(hello_nothofagus
    "hello_nothofagus.cpp"
)
set_property(TARGET hello_nothofagus PROPERTY CXX_STANDARD 20)
target_include_directories(hello_nothofagus PRIVATE ${NOTHOFAGUS_INCLUDE})
target_link_libraries(hello_nothofagus PRIVATE nothofagus)