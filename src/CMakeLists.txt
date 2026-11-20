cmake_minimum_required(VERSION 3.20)

project(bloop VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC AND POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>")
endif()

# everything except main.cpp is the library that a host program links to (see bloop.hpp)
file(GLOB_RECURSE LIBRARY_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/lexer/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ast/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/parser/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/resolver/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/bytecode/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/vm/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/std/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/module/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/embed/*.cpp"
)

add_library(bloop_lib STATIC ${LIBRARY_SOURCES})

# the headers need C++20, so a host that links to the library gets it too
target_compile_features(bloop_lib PUBLIC cxx_std_20)

# a host adds this directory to its include path and includes "bloop.hpp"
target_include_directories(bloop_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(bloop "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
target_link_libraries(bloop PRIVATE bloop_lib)

set_target_properties(bloop PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

#if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
#    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
#endif()

foreach(target bloop_lib bloop)
    target_compile_definitions(${target} PRIVATE
        $<$<CONFIG:Debug>:DEBUG_MODE=1>
    )

    if (MSVC)
        target_compile_options(${target} PRIVATE
            /W4 /WX
            /wd4514 /wd4626 /wd4820 /wd5267 /wd4351 /wd5045 /wd4371 /wd4324
            $<$<CONFIG:Debug>:/Od /Zi /RTC1>
            $<$<CONFIG:RelWithDebInfo>:/O2 /Zi /MD>
            $<$<CONFIG:Release>:/O2>
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Werror -pedantic
            $<$<CONFIG:Debug>:-O0 -g>
            $<$<CONFIG:Release>:-O3>
        )
    endif()
endforeach()
