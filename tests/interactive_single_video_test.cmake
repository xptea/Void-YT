file(READ "${PROJECT_SOURCE_DIR}/src/main.c" main_source)

set(interactive_single_video_args
    "if (interactive_result > 0) {\n        child_args[index++] = \"--no-playlist\";")
string(FIND "${main_source}" "${interactive_single_video_args}" single_video_guard)

if(single_video_guard EQUAL -1)
    message(FATAL_ERROR "Interactive downloads must disable playlist expansion")
endif()
