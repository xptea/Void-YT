file(READ "${PROJECT_SOURCE_DIR}/scripts/install.ps1" windows_installer)
file(READ "${PROJECT_SOURCE_DIR}/scripts/install.sh" unix_installer)

string(REPLACE "@GITHUB_REPOSITORY@" "xptea/Void-YT" windows_stamped "${windows_installer}")
string(REPLACE "@GITHUB_REPOSITORY@" "xptea/Void-YT" unix_stamped "${unix_installer}")

string(FIND "${windows_stamped}" "$Repository -eq \"xptea/Void-YT\"" windows_bad_guard)
if(NOT windows_bad_guard EQUAL -1)
    message(FATAL_ERROR "The stamped Windows installer rejects its valid repository")
endif()

string(FIND "${unix_stamped}" "[ \"$repo\" = \"xptea/Void-YT\" ]" unix_bad_guard)
if(NOT unix_bad_guard EQUAL -1)
    message(FATAL_ERROR "The stamped Unix installer rejects its valid repository")
endif()
