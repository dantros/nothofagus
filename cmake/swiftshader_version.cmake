# Pinned SwiftShader prebuilt coordinates — the single source of truth shared by
# the CMake fetch (cmake/fetch_swiftshader.cmake) and the CI workflow
# (.github/workflows/rendering-tests.yml, which reads these via `cmake -P`).
#
# SwiftShader is a software (CPU) Vulkan ICD. Rendering the visual tests against
# it gives deterministic pixels that do not vary with GPU drivers, which is why
# CI authors its golden_headless_vulkan/ set with it.
#
# To bump the SwiftShader version: publish a new prebuilt release at
# https://github.com/dantros/swiftshader_prebuilts with the new upstream commit
# SHA in its tag + filename, then update all three variables below together and
# regenerate the golden_headless_vulkan/ PNGs.

set(NOTHOFAGUS_SWIFTSHADER_TAG "swiftshader-d26a3e66-linux-x86_64")
set(NOTHOFAGUS_SWIFTSHADER_URL
    "https://github.com/dantros/swiftshader_prebuilts/releases/download/${NOTHOFAGUS_SWIFTSHADER_TAG}/libvk_swiftshader-d26a3e66-linux-x86_64.so")
set(NOTHOFAGUS_SWIFTSHADER_SHA256
    "fbf976da70759946eaf3cd97cbff52b8aece93b8bd80c9f05112082040d85d92")
