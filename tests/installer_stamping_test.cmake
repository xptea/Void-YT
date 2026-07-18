file(READ "${PROJECT_SOURCE_DIR}/scripts/install.ps1" windows_installer)
file(READ "${PROJECT_SOURCE_DIR}/scripts/install.sh" unix_installer)
file(READ "${PROJECT_SOURCE_DIR}/README.md" readme)

string(REPLACE "@GITHUB_REPOSITORY@" "xptea/Void-YT" windows_stamped "${windows_installer}")
string(REPLACE "@GITHUB_REPOSITORY@" "xptea/Void-YT" unix_stamped "${unix_installer}")

string(FIND "${windows_stamped}" "$Repository -eq \"xptea/Void-YT\"" windows_bad_guard)
if(NOT windows_bad_guard EQUAL -1)
    message(FATAL_ERROR "The stamped Windows installer rejects its valid repository")
endif()

string(FIND "${windows_stamped}" "RuntimeInformation]::OSArchitecture" windows_architecture_gate)
if(NOT windows_architecture_gate EQUAL -1)
    message(FATAL_ERROR "The Windows installer must let Windows determine executable compatibility")
endif()

string(FIND "${unix_stamped}" "[ \"$repo\" = \"xptea/Void-YT\" ]" unix_bad_guard)
if(NOT unix_bad_guard EQUAL -1)
    message(FATAL_ERROR "The stamped Unix installer rejects its valid repository")
endif()

string(FIND "${readme}" "powershell -NoProfile -ExecutionPolicy Bypass -Command -" windows_streaming_command)
if(NOT windows_streaming_command EQUAL -1)
    message(FATAL_ERROR "The Windows install command streams a multiline script into powershell -Command -")
endif()

string(FIND "${readme}" "Out-String | Invoke-Expression" windows_complete_script_command)
if(windows_complete_script_command EQUAL -1)
    message(FATAL_ERROR "The Windows install command must evaluate the complete downloaded script")
endif()
