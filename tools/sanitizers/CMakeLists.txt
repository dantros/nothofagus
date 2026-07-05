# Sanitizer fixture: a small self-closing threaded workload used by run_sanitizers.sh to
# exercise the threaded sim/render path under TSan and ASan+UBSan against SwiftShader.
# Gated by NOTHOFAGUS_BUILD_SANITIZER_FIXTURE (default OFF); the helper script turns it on.
# Requires the headless-Vulkan backend (SwiftShader is a Vulkan ICD).

if(NOT NOTHOFAGUS_HEADLESS_VULKAN)
    message(FATAL_ERROR
        "NOTHOFAGUS_BUILD_SANITIZER_FIXTURE requires NOTHOFAGUS_HEADLESS_VULKAN=ON "
        "(the sanitizer fixture renders on the SwiftShader Vulkan ICD). "
        "Use tools/sanitizers/run_sanitizers.sh, which sets this up.")
endif()

add_executable(sanitizer_threaded_smoke "threaded_smoke.cpp")
set_property(TARGET sanitizer_threaded_smoke PROPERTY CXX_STANDARD 20)
target_link_libraries(sanitizer_threaded_smoke PRIVATE nothofagus)
