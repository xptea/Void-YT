cmake_minimum_required(VERSION 3.20)

project(void-yt VERSION 0.1.0 LANGUAGES C)

set(VOIDYT_BUILD_VERSION "${PROJECT_VERSION}" CACHE STRING "Version displayed by the executable")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(void-yt
    src/main.c
    src/deps.c
    src/process.c
)

target_include_directories(void-yt PRIVATE src)
target_compile_definitions(void-yt PRIVATE VOIDYT_VERSION="${VOIDYT_BUILD_VERSION}")

if(MSVC)
    target_compile_options(void-yt PRIVATE /W4 /permissive-)
else()
    target_compile_options(void-yt PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME version COMMAND void-yt --version)
    add_test(NAME help COMMAND void-yt --help)
endif()

install(TARGETS void-yt RUNTIME DESTINATION .)
