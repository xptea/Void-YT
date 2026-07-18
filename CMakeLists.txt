cmake_minimum_required(VERSION 3.20)

project(void-yt VERSION 0.3.3 LANGUAGES C)

set(VOIDYT_BUILD_VERSION "${PROJECT_VERSION}" CACHE STRING "Version displayed by the executable")
set(VOIDYT_UPDATE_REPOSITORY "xptea/Void-YT" CACHE STRING "GitHub repository used for automatic updates")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(void-yt
    src/main.c
    src/deps.c
    src/ffmpeg.c
    src/interactive.c
    src/process.c
    src/update.c
)

target_include_directories(void-yt PRIVATE src)
target_compile_definitions(void-yt PRIVATE
    VOIDYT_VERSION="${VOIDYT_BUILD_VERSION}"
    VOIDYT_UPDATE_REPOSITORY="${VOIDYT_UPDATE_REPOSITORY}"
)

if(MSVC)
    target_compile_options(void-yt PRIVATE /W4 /permissive-)
else()
    target_compile_options(void-yt PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME version COMMAND void-yt --version)
    add_test(NAME help COMMAND void-yt --help)
    set_tests_properties(version help PROPERTIES ENVIRONMENT "VOID_YT_NO_UPDATE=1")

    add_executable(version-test
        tests/version_test.c
        src/update.c
        src/process.c
        src/deps.c
    )
    target_include_directories(version-test PRIVATE src)
    if(MSVC)
        target_compile_options(version-test PRIVATE /W4 /permissive-)
    else()
        target_compile_options(version-test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME version-comparison COMMAND version-test)

    add_executable(process-capture-test
        tests/process_capture_test.c
        src/process.c
    )
    target_include_directories(process-capture-test PRIVATE src)
    if(MSVC)
        target_compile_options(process-capture-test PRIVATE /W4 /permissive-)
    else()
        target_compile_options(process-capture-test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME process-capture COMMAND process-capture-test)

    add_test(NAME installer-stamping
        COMMAND ${CMAKE_COMMAND}
            -DPROJECT_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P ${CMAKE_SOURCE_DIR}/tests/installer_stamping_test.cmake
    )
    add_test(NAME interactive-single-video
        COMMAND ${CMAKE_COMMAND}
            -DPROJECT_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P ${CMAKE_SOURCE_DIR}/tests/interactive_single_video_test.cmake
    )
endif()

install(TARGETS void-yt RUNTIME DESTINATION .)
