
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

if (MSVC AND POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>")
endif()

file(GLOB_RECURSE PROJECT_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/lexer/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/ast/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/parser/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/resolver/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/bytecode/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/vm/*.cpp"
)

add_executable(bloop_test ${PROJECT_SOURCES})

target_include_directories(bloop_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)

target_compile_definitions(bloop_test PRIVATE BLOOP_TEST)
add_compile_definitions(SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")


if (MSVC)
    target_compile_options(bloop_test PRIVATE
        /W4 /WX
        /wd4514 /wd4626 /wd4820 /wd5267 /wd4351 /wd5045 /wd4371 /wd4324
        $<$<CONFIG:Debug>:/Od /Zi /RTC1>
        $<$<CONFIG:Release>:/O2 /MD>
    )
else()
    target_compile_options(bloop_test PRIVATE
        -Wall -Wextra -Werror -pedantic
        $<$<CONFIG:Debug>:-O0 -g>
        $<$<CONFIG:Release>:-O3>
    )
endif()

target_compile_definitions(bloop_test PRIVATE
    $<$<CONFIG:Debug>:DEBUG_MODE=1>
)

add_test(NAME BloopTests COMMAND bloop_test)
