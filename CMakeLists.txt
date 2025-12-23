cmake_minimum_required(VERSION 3.20)

project(bloop VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC AND POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>")
endif()

file(GLOB_RECURSE PROJECT_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/lexer/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ast/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/parser/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/resolver/*.cpp"
)

add_executable(bloop ${PROJECT_SOURCES})

target_include_directories(bloop PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

set_target_properties(bloop PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if (MSVC)
    target_compile_options(bloop PRIVATE
        /W4 /WX
        /wd4514 /wd4626 /wd4820 /wd5267 /wd4351 /wd5045 /wd4371 /wd4324
    )
else()
    target_compile_options(bloop PRIVATE
        -Wall -Wextra -Werror -pedantic
    )
endif()

target_compile_definitions(bloop PRIVATE
    $<$<CONFIG:Debug>:DEBUG_MODE=1>
)

if (MSVC)
    target_compile_options(bloop PRIVATE
        $<$<CONFIG:Debug>:/Od /Zi /RTC1>
        $<$<CONFIG:Release>:/O2 /MD>
    )
else()
    target_compile_options(bloop PRIVATE
        $<$<CONFIG:Debug>:-O0 -g>
        $<$<CONFIG:Release>:-O3>
    )
endif()
