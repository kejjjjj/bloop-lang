# a host program that embeds bloop: links to bloop_lib and includes "bloop.hpp"
add_executable(embedding_example main.cpp)
target_link_libraries(embedding_example PRIVATE bloop_lib)
target_compile_definitions(embedding_example PRIVATE BLOOP_EXAMPLE_MOD="${CMAKE_CURRENT_SOURCE_DIR}/mod.bloop")
