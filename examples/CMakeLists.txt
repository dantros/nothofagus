add_executable(hello_nothofagus
    "hello_nothofagus.cpp"
)
set_property(TARGET hello_nothofagus PROPERTY CXX_STANDARD 20)
target_include_directories(hello_nothofagus PRIVATE ${NOTHOFAGUS_INCLUDE})
target_link_libraries(hello_nothofagus PRIVATE nothofagus)

add_executable(hello_text
    "hello_text.cpp"
)
set_property(TARGET hello_text PROPERTY CXX_STANDARD 20)
target_include_directories(hello_text PRIVATE ${NOTHOFAGUS_INCLUDE})
target_link_libraries(hello_text PRIVATE nothofagus)

add_executable(hello_tint
    "hello_tint.cpp"
)
set_property(TARGET hello_tint PROPERTY CXX_STANDARD 20)
target_include_directories(hello_tint PRIVATE ${NOTHOFAGUS_INCLUDE})
target_link_libraries(hello_tint PRIVATE nothofagus)

add_executable(test_keyboard
    "test_keyboard.cpp"
)
set_property(TARGET test_keyboard PROPERTY CXX_STANDARD 20)
target_include_directories(test_keyboard PRIVATE ${NOTHOFAGUS_INCLUDE})
target_link_libraries(test_keyboard PRIVATE nothofagus)