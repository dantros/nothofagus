cmake_minimum_required(VERSION 3.26)
project(vma_cl CXX)

# VMA_IMPL_SRC, VMA_INCLUDE_DIR, and VMA_VULKAN_INCLUDE_DIR are passed in
# from the parent project via ExternalProject_Add CMAKE_ARGS.
add_library(vma_impl STATIC "${VMA_IMPL_SRC}")
target_include_directories(vma_impl PRIVATE
    "${VMA_INCLUDE_DIR}"
    "${VMA_VULKAN_INCLUDE_DIR}")
set_property(TARGET vma_impl PROPERTY CXX_STANDARD 20)

include(GNUInstallDirs)
install(TARGETS vma_impl ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
