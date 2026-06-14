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

add_executable(hello_imgui_rtt
    "hello_imgui_rtt.cpp"
)
set_property(TARGET hello_imgui_rtt PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_imgui_rtt PRIVATE nothofagus)

add_executable(hello_imgui_overlay
    "hello_imgui_overlay.cpp"
)
set_property(TARGET hello_imgui_overlay PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_imgui_overlay PRIVATE nothofagus)

add_executable(hello_custom_font
    "hello_custom_font.cpp"
)
set_property(TARGET hello_custom_font PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_custom_font PRIVATE nothofagus)
# Mirror the lib's PRIVATE NOTHOFAGUS_HAS_CJK_* defines onto the example so its
# macro-guarded CJK demo blocks compile in when the matching font is embedded.
foreach(_cjk SC TC JP KR)
    if(NOTHOFAGUS_EMBED_CJK_${_cjk})
        target_compile_definitions(hello_custom_font PRIVATE NOTHOFAGUS_HAS_CJK_${_cjk})
    endif()
endforeach()

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

add_executable(hello_tilemap
    "hello_tilemap.cpp"
)
set_property(TARGET hello_tilemap PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_tilemap PRIVATE nothofagus)

add_executable(hello_dense_land
    "hello_dense_land.cpp"
)
set_property(TARGET hello_dense_land PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_dense_land PRIVATE nothofagus)

add_executable(hello_sparse_land
    "hello_sparse_land.cpp"
)
set_property(TARGET hello_sparse_land PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_sparse_land PRIVATE nothofagus)

add_executable(hello_mesh
    "hello_mesh.cpp"
)
set_property(TARGET hello_mesh PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_mesh PRIVATE nothofagus)

add_executable(hello_markdown
    "hello_markdown.cpp"
)
set_property(TARGET hello_markdown PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_markdown PRIVATE nothofagus)

add_executable(hello_dpi_scaling
    "hello_dpi_scaling.cpp"
)
set_property(TARGET hello_dpi_scaling PROPERTY CXX_STANDARD 20)
target_link_libraries(hello_dpi_scaling PRIVATE nothofagus)
