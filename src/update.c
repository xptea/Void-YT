#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "update.h"

#include "subprocess.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef VOIDYT_VERSION
#define VOIDYT_VERSION "dev"
#endif

#ifndef VOIDYT_UPDATE_REPOSITORY
#define VOIDYT_UPDATE_REPOSITORY "xptea/Void-YT"
#endif

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#define VOIDYT_DIR_SEP '\\'
#define VOIDYT_UPDATER_NAME "update.ps1"
#define VOIDYT_EXECUTABLE_NAME "void-yt.exe"
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define VOIDYT_DIR_SEP '/'
#define VOIDYT_UPDATER_NAME "update.sh"
#define VOIDYT_EXECUTABLE_NAME "void-yt"
#endif

static int join_path(char *dest, size_t cap, const char *left, const char *right) {
    size_t left_len = strlen(left);
    int separator = left_len > 0 && left[left_len - 1] != '/' && left[left_len - 1] != '\\';
    size_t right_len = strlen(right);
    if (left_len + (size_t)separator + right_len + 1 > cap) {
        return 0;
    }
    memcpy(dest, left, left_len);
    if (separator) {
        dest[left_len++] = VOIDYT_DIR_SEP;
    }
    memcpy(dest + left_len, right, right_len + 1);
    return 1;
}

static int parse_version(const char *value, unsigned long parts[3]) {
    const char *cursor = value;
    size_t index;
    char *end;
    if (cursor == NULL) {
        return 0;
    }
    while (isspace((unsigned char)*cursor)) {
        ++cursor;
    }
    if (*cursor == 'v' || *cursor == 'V') {
        ++cursor;
    }
    for (index = 0; index < 3; ++index) {
        if (!isdigit((unsigned char)*cursor)) {
            return 0;
        }
        errno = 0;
        parts[index] = strtoul(cursor, &end, 10);
        if (errno != 0 || end == cursor) {
            return 0;
        }
        cursor = end;
        if (index < 2) {
            if (*cursor != '.') {
                return 0;
            }
            ++cursor;
        }
    }
    while (isspace((unsigned char)*cursor)) {
        ++cursor;
    }
    return *cursor == '\0';
}

int voidyt_compare_versions(const char *current, const char *latest, int *comparison) {
    unsigned long current_parts[3];
    unsigned long latest_parts[3];
    size_t index;
    if (comparison == NULL || !parse_version(current, current_parts) ||
        !parse_version(latest, latest_parts)) {
        return 0;
    }
    *comparison = 0;
    for (index = 0; index < 3; ++index) {
        if (current_parts[index] < latest_parts[index]) {
            *comparison = -1;
            break;
        }
        if (current_parts[index] > latest_parts[index]) {
            *comparison = 1;
            break;
        }
    }
    return 1;
}

static int updates_disabled(void) {
    const char *value = getenv("VOID_YT_NO_UPDATE");
    return value != NULL && value[0] != '\0' && strcmp(value, "0") != 0;
}

static int make_temp_path(char *dest, size_t cap, const char *suffix) {
    int result;
#ifdef _WIN32
    char directory[VOIDYT_PATH_CAP];
    DWORD length = GetTempPathA((DWORD)sizeof(directory), directory);
    if (length == 0 || length >= sizeof(directory)) {
        return 0;
    }
    result = snprintf(dest, cap, "%svoid-yt-%lu%s", directory,
                      (unsigned long)GetCurrentProcessId(), suffix);
#else
    const char *directory = getenv("TMPDIR");
    if (directory == NULL || directory[0] == '\0') {
        directory = "/tmp";
    }
    result = snprintf(dest, cap, "%s/void-yt-%ld%s", directory, (long)getpid(), suffix);
#endif
    return result > 0 && (size_t)result < cap;
}

static int read_version_file(const char *path, char *dest, size_t cap) {
    FILE *file = fopen(path, "rb");
    size_t length;
    if (file == NULL) {
        return 0;
    }
    length = fread(dest, 1, cap - 1, file);
    fclose(file);
    dest[length] = '\0';
    while (length > 0 && isspace((unsigned char)dest[length - 1])) {
        dest[--length] = '\0';
    }
    return length > 0;
}

static int copy_file(const char *source, const char *destination) {
    FILE *input = fopen(source, "rb");
    FILE *output;
    char buffer[8192];
    size_t count;
    if (input == NULL) {
        return 0;
    }
    output = fopen(destination, "wb");
    if (output == NULL) {
        fclose(input);
        return 0;
    }
    while ((count = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if (fwrite(buffer, 1, count, output) != count) {
            fclose(input);
            fclose(output);
            return 0;
        }
    }
    fclose(input);
    if (fclose(output) != 0) {
        return 0;
    }
#ifndef _WIN32
    chmod(destination, 0700);
#endif
    return 1;
}

#ifdef _WIN32
static int append_text(char *dest, size_t cap, size_t *length, const char *text) {
    size_t text_length = strlen(text);
    if (*length + text_length + 1 > cap) {
        return 0;
    }
    memcpy(dest + *length, text, text_length);
    *length += text_length;
    dest[*length] = '\0';
    return 1;
}

static int append_windows_argument(char *dest, size_t cap, size_t *length, const char *arg) {
    const char *cursor = arg;
    size_t backslashes = 0;
    if (*length > 0 && !append_text(dest, cap, length, " ")) {
        return 0;
    }
    if (!append_text(dest, cap, length, "\"")) {
        return 0;
    }
    while (*cursor != '\0') {
        if (*cursor == '\\') {
            ++backslashes;
            ++cursor;
            continue;
        }
        if (*cursor == '\"') {
            while (backslashes > 0) {
                if (!append_text(dest, cap, length, "\\\\")) return 0;
                --backslashes;
            }
            if (!append_text(dest, cap, length, "\\\"")) return 0;
            backslashes = 0;
            ++cursor;
            continue;
        }
        while (backslashes > 0) {
            if (!append_text(dest, cap, length, "\\")) return 0;
            --backslashes;
        }
        backslashes = 0;
        {
            char character[2] = {*cursor++, '\0'};
            if (!append_text(dest, cap, length, character)) return 0;
        }
    }
    while (backslashes > 0) {
        if (!append_text(dest, cap, length, "\\\\")) return 0;
        --backslashes;
    }
    return append_text(dest, cap, length, "\"");
}
#endif

static int spawn_updater(const char *script,
                         const char *repository,
                         const char *app_dir,
                         const char *executable,
                         int argc,
                         char **argv) {
    char pid_text[32];
    int index;
#ifdef _WIN32
    const char *updater_args[14];
    char restart_command[32768] = "";
    size_t restart_length = 0;
    intptr_t process_handle;
    snprintf(pid_text, sizeof(pid_text), "%lu", (unsigned long)GetCurrentProcessId());
    for (index = 1; index < argc; ++index) {
        if (!append_windows_argument(restart_command, sizeof(restart_command),
                                     &restart_length, argv[index])) {
            fprintf(stderr, "void-yt: update restart command is too long\n");
            return 0;
        }
    }
    SetEnvironmentVariableA("VOID_YT_RESTART_COMMAND_LINE", restart_command);
    updater_args[0] = "powershell.exe";
    updater_args[1] = "-NoProfile";
    updater_args[2] = "-ExecutionPolicy";
    updater_args[3] = "Bypass";
    updater_args[4] = "-File";
    updater_args[5] = script;
    updater_args[6] = "-Repository";
    updater_args[7] = repository;
    updater_args[8] = "-ParentPid";
    updater_args[9] = pid_text;
    updater_args[10] = "-InstallDirectory";
    updater_args[11] = app_dir;
    updater_args[12] = "-Executable";
    updater_args[13] = executable;
    {
        const char *complete_args[15];
        for (index = 0; index < 14; ++index) {
            complete_args[index] = updater_args[index];
        }
        complete_args[14] = NULL;
        process_handle = _spawnvp(_P_NOWAIT, "powershell.exe", complete_args);
    }
    SetEnvironmentVariableA("VOID_YT_RESTART_COMMAND_LINE", NULL);
    if (process_handle == -1) {
        return 0;
    }
    CloseHandle((HANDLE)process_handle);
    return 1;
#else
    pid_t child;
    const char **updater_args = (const char **)calloc((size_t)argc + 7, sizeof(char *));
    size_t arg_index = 0;
    if (updater_args == NULL) {
        return 0;
    }
    snprintf(pid_text, sizeof(pid_text), "%ld", (long)getpid());
    updater_args[arg_index++] = "/bin/sh";
    updater_args[arg_index++] = script;
    updater_args[arg_index++] = repository;
    updater_args[arg_index++] = pid_text;
    updater_args[arg_index++] = app_dir;
    updater_args[arg_index++] = executable;
    for (index = 1; index < argc; ++index) {
        updater_args[arg_index++] = argv[index];
    }
    updater_args[arg_index] = NULL;
    child = fork();
    if (child == 0) {
        setsid();
        execv("/bin/sh", (char *const *)updater_args);
        _exit(127);
    }
    free(updater_args);
    return child > 0;
#endif
}

int voidyt_maybe_auto_update(const voidyt_dependencies *deps, int argc, char **argv) {
    const char *repository = getenv("VOID_YT_REPO");
    const char *base_override = getenv("VOID_YT_UPDATE_BASE_URL");
    char base_url[1024];
    char version_url[1200];
    char version_path[VOIDYT_PATH_CAP];
    char latest_version[64];
    char bundled_updater[VOIDYT_PATH_CAP];
    char tools_dir[VOIDYT_PATH_CAP];
    char temp_updater[VOIDYT_PATH_CAP];
    char executable[VOIDYT_PATH_CAP];
    const char *curl_args[14];
    int comparison;

    if (deps == NULL || updates_disabled() || deps->curl[0] == '\0') {
        return 0;
    }
    if (repository == NULL || repository[0] == '\0') {
        repository = VOIDYT_UPDATE_REPOSITORY;
    }
    if (base_override != NULL && base_override[0] != '\0') {
        if (snprintf(base_url, sizeof(base_url), "%s", base_override) >= (int)sizeof(base_url)) {
            return 0;
        }
    } else if (snprintf(base_url, sizeof(base_url),
                        "https://github.com/%s/releases/latest/download", repository) >=
               (int)sizeof(base_url)) {
        return 0;
    }
    if (snprintf(version_url, sizeof(version_url), "%s/version.txt", base_url) >=
        (int)sizeof(version_url) ||
        !make_temp_path(version_path, sizeof(version_path), ".version")) {
        return 0;
    }

    curl_args[0] = deps->curl;
    curl_args[1] = "-fsL";
    curl_args[2] = "--connect-timeout";
    curl_args[3] = "3";
    curl_args[4] = "--max-time";
    curl_args[5] = "8";
    curl_args[6] = "--retry";
    curl_args[7] = "1";
    curl_args[8] = "-o";
    curl_args[9] = version_path;
    curl_args[10] = "--";
    curl_args[11] = version_url;
    curl_args[12] = NULL;
    if (voidyt_run_process(deps->curl, curl_args) != 0 ||
        !read_version_file(version_path, latest_version, sizeof(latest_version))) {
        remove(version_path);
        return 0;
    }
    remove(version_path);
    if (!voidyt_compare_versions(VOIDYT_VERSION, latest_version, &comparison) || comparison >= 0) {
        return 0;
    }

    if (!join_path(tools_dir, sizeof(tools_dir), deps->app_dir, "tools") ||
        !join_path(bundled_updater, sizeof(bundled_updater), tools_dir, VOIDYT_UPDATER_NAME) ||
        !voidyt_path_exists(bundled_updater) ||
        !make_temp_path(temp_updater, sizeof(temp_updater),
#ifdef _WIN32
                        ".update.ps1"
#else
                        ".update.sh"
#endif
                        ) ||
        !copy_file(bundled_updater, temp_updater) ||
        !join_path(executable, sizeof(executable), deps->app_dir, VOIDYT_EXECUTABLE_NAME)) {
        fprintf(stderr,
                "void-yt: version %s is available, but the bundled updater is unavailable.\n",
                latest_version);
        return 0;
    }

    printf("Void-YT %s is available (current: %s). Updating and restarting...\n",
           latest_version, VOIDYT_VERSION);
    fflush(stdout);
    if (!spawn_updater(temp_updater, repository, deps->app_dir, executable, argc, argv)) {
        remove(temp_updater);
        fprintf(stderr, "void-yt: could not start the updater; continuing with this version.\n");
        return 0;
    }
    return 1;
}
