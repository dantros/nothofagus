add_executable(hello_nothofagus
    "hello_nothofagus.cpp"
)
set_property(TARGET hello_nothofagus PROPERTY CXX_STANDARD 20)
target_include_directories(hello_nothofagus PRIVATE ${NOTHOFAGUS_INCLUDE})
target_link_libraries(hello_nothofagus PRIVATE nothofagus)

add_executable(hello_animated_bellota
    "hello_animated_bellota.cpp"
)
set_property(TARGET hello_animated_bellota PROPERTY CXX_STANDARD 20)
target_include_directories(hello_animated_bellota PRIVATE ${NOTHOFAGUS_INCLUDE})
target_link_libraries(hello_animated_bellota PRIVATE nothofagus)

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

add_executable(set_layer
    "set_layer.cpp"
)
set_property(TARGET set_layer PROPERTY CXX_STANDARD 20)
target_include_directories(set_layer PRIVATE ${NOTHOFAGUS_INCLUDE})
target_link_libraries(set_layer PRIVATE nothofagus)

add_executable(animation_state
    "animation_state.cpp"
)
set_property(TARGET animation_state PROPERTY CXX_STANDARD 20)
target_include_directories(animation_state PRIVATE ${NOTHOFAGUS_INCLUDE})
target_link_libraries(animation_state PRIVATE nothofagus)

add_executable(animation_state_machine
    "animation_state_machine.cpp"
)
set_property(TARGET animation_state_machine PROPERTY CXX_STANDARD 20)
target_include_directories(animation_state_machine PRIVATE ${NOTHOFAGUS_INCLUDE})
target_link_libraries(animation_state_machine PRIVATE nothofagus)