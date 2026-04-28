add_executable(hello_nothofagus
    "hello_nothofagus.cpp"
)
set_property(TARGET hello_nothofagus PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_nothofagus PRIVATE nothofagus)

add_executable(hello_direct_texture
    "hello_direct_texture.cpp"
)
set_property(TARGET hello_direct_texture PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_direct_texture PRIVATE nothofagus)

add_executable(hello_animation
    "hello_animation.cpp"
)
set_property(TARGET hello_animation PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_animation PRIVATE nothofagus)

add_executable(hello_text
    "hello_text.cpp"
)
set_property(TARGET hello_text PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_text PRIVATE nothofagus)

add_executable(hello_tint
    "hello_tint.cpp"
)
set_property(TARGET hello_tint PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_tint PRIVATE nothofagus)

add_executable(test_keyboard
    "test_keyboard.cpp"
)
set_property(TARGET test_keyboard PROPERTY CXX_STANDARD 20)
target_link_libraries(test_keyboard PRIVATE nothofagus)

add_executable(test_create_destroy
    "test_create_destroy.cpp"
)
set_property(TARGET test_create_destroy PROPERTY CXX_STANDARD 20)
target_link_libraries(test_create_destroy PRIVATE nothofagus)

add_executable(hello_layers
    "hello_layers.cpp"
)
set_property(TARGET hello_layers PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_layers PRIVATE nothofagus)

add_executable(hello_animation_state_machine
    "hello_animation_state_machine.cpp"
)
set_property(TARGET hello_animation_state_machine PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_animation_state_machine PRIVATE nothofagus)

add_executable(hello_render_to_texture
    "hello_render_to_texture.cpp"
)
set_property(TARGET hello_render_to_texture PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_render_to_texture PRIVATE nothofagus)

add_executable(hello_nested_render_targets
    "hello_nested_render_targets.cpp"
)
set_property(TARGET hello_nested_render_targets PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_nested_render_targets PRIVATE nothofagus)

add_executable(hello_screenshot
    "hello_screenshot.cpp"
)
set_property(TARGET hello_screenshot PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_screenshot PRIVATE nothofagus)

add_executable(hello_headless
    "hello_headless.cpp"
)
set_property(TARGET hello_headless PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_headless PRIVATE nothofagus)

add_executable(test_gamepad
    "test_gamepad.cpp"
)
set_property(TARGET test_gamepad PROPERTY CXX_STANDARD 20)
target_link_libraries(test_gamepad PRIVATE nothofagus)

add_executable(hello_tilemap2
    "hello_tilemap2.cpp"
)
set_property(TARGET hello_tilemap2 PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_tilemap2 PRIVATE nothofagus)
