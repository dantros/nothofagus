add_executable(hello_nothofagus
    "hello_nothofagus.cpp"
)
set_property(TARGET hello_nothofagus PROPERTY CXX_STANDARD 20)
target_include_directories(hello_nothofagus PRIVATE ${NOTHOFAGUS_INCLUDE})
target_link_libraries(hello_nothofagus PRIVATE nothofagus)

add_executable(hello_direct_texture
    "hello_direct_texture.cpp"
)
set_property(TARGET hello_direct_texture PROPERTY CXX_STANDARD 20)
target_include_directories(hello_direct_texture PRIVATE ${NOTHOFAGUS_INCLUDE})
target_link_libraries(hello_direct_texture PRIVATE nothofagus)

add_executable(hello_animation
    "hello_animation.cpp"
)
set_property(TARGET hello_animation PROPERTY CXX_STANDARD 20)
target_include_directories(hello_animation PRIVATE ${NOTHOFAGUS_INCLUDE})
target_link_libraries(hello_animation PRIVATE nothofagus)

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

add_executable(hello_layers
    "hello_layers.cpp"
)
set_property(TARGET hello_layers PROPERTY CXX_STANDARD 20)
target_include_directories(hello_layers PRIVATE ${NOTHOFAGUS_INCLUDE})
target_link_libraries(hello_layers PRIVATE nothofagus)

add_executable(hello_animation_state_machine0
    "hello_animation_state_machine0.cpp"
)
set_property(TARGET hello_animation_state_machine0 PROPERTY CXX_STANDARD 20)
target_include_directories(hello_animation_state_machine0 PRIVATE ${NOTHOFAGUS_INCLUDE})
target_link_libraries(hello_animation_state_machine0 PRIVATE nothofagus)

add_executable(hello_animation_state_machine
    "hello_animation_state_machine.cpp"
)
set_property(TARGET hello_animation_state_machine PROPERTY CXX_STANDARD 20)
target_include_directories(hello_animation_state_machine PRIVATE ${NOTHOFAGUS_INCLUDE})
target_link_libraries(hello_animation_state_machine PRIVATE nothofagus)