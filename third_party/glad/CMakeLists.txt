
# Building glad as C++11 code
add_library(glad STATIC "src/glad.c")
set_property(TARGET glad PROPERTY CXX_STANDARD 11)
set_target_properties(glad PROPERTIES LINKER_LANGUAGE CXX)
set_source_files_properties("src/glad.c" PROPERTIES LANGUAGE CXX )
target_include_directories(glad PRIVATE "include")