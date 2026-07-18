#include "ffmpeg.h"

#include "subprocess.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define VOIDYT_PATH_SEP '\\'
#define VOIDYT_FFMPEG_NAME "ffmpeg.exe"
#define VOIDYT_INSTALLER_NAME "install-ffmpeg.ps1"
#else
#define VOIDYT_PATH_SEP '/'
#define VOIDYT_FFMPEG_NAME "ffmpeg"
#define VOIDYT_INSTALLER_NAME "install-ffmpeg.sh"
#endif

static int join_path(char *dest, size_t cap, const char *left, const char *right) {
    size_t left_len;
    int needs_sep;

    if (dest == NULL || left == NULL || right == NULL || cap == 0) {
        return 0;
    }
    left_len = strlen(left);
    needs_sep = left_len > 0 && left[left_len - 1] != '/' && left[left_len - 1] != '\\';
    if (left_len + (size_t)needs_sep + strlen(right) + 1 > cap) {
        return 0;
    }
    memcpy(dest, left, left_len);
    if (needs_sep) {
        dest[left_len++] = VOIDYT_PATH_SEP;
    }
    memcpy(dest + left_len, right, strlen(right) + 1);
    return 1;
}

static int install_disabled(void) {
    const char *value = getenv("VOID_YT_NO_FFMPEG_INSTALL");
    return value != NULL &&
           (strcmp(value, "1") == 0 || strcmp(value, "true") == 0 ||
            strcmp(value, "yes") == 0);
}

int voidyt_ensure_ffmpeg(voidyt_dependencies *deps) {
    char tools_dir[VOIDYT_PATH_CAP];
    char installer[VOIDYT_PATH_CAP];
    char target[VOIDYT_PATH_CAP];
    int result;

    if (deps == NULL) {
        return 0;
    }
    if (deps->ffmpeg[0] != '\0') {
        return 1;
    }
    if (install_disabled()) {
        return 0;
    }
    if (deps->curl[0] == '\0') {
        fprintf(stderr,
                "void-yt: FFmpeg is missing and cannot be installed because curl is unavailable.\n");
        return 0;
    }
    if (!join_path(tools_dir, sizeof(tools_dir), deps->app_dir, "tools") ||
        !join_path(installer, sizeof(installer), tools_dir, VOIDYT_INSTALLER_NAME) ||
        !voidyt_path_exists(installer) ||
        !join_path(target, sizeof(target), tools_dir, VOIDYT_FFMPEG_NAME)) {
        fprintf(stderr,
                "void-yt: FFmpeg is missing and the first-run installer is unavailable.\n");
        return 0;
    }

    printf("FFmpeg was not found. Installing a verified FFmpeg 8.1.2 build...\n");
    fflush(stdout);
#ifdef _WIN32
    {
        const char *system_root = getenv("SystemRoot");
        char powershell[VOIDYT_PATH_CAP];
        const char *arguments[12];
        if (system_root == NULL || system_root[0] == '\0') {
            system_root = "C:\\Windows";
        }
        if (snprintf(powershell, sizeof(powershell),
                     "%s\\System32\\WindowsPowerShell\\v1.0\\powershell.exe",
                     system_root) < 0 ||
            !voidyt_path_exists(powershell)) {
            fprintf(stderr, "void-yt: Windows PowerShell is required to install FFmpeg.\n");
            return 0;
        }
        arguments[0] = powershell;
        arguments[1] = "-NoProfile";
        arguments[2] = "-NonInteractive";
        arguments[3] = "-ExecutionPolicy";
        arguments[4] = "Bypass";
        arguments[5] = "-File";
        arguments[6] = installer;
        arguments[7] = "-ApplicationDirectory";
        arguments[8] = deps->app_dir;
        arguments[9] = "-Curl";
        arguments[10] = deps->curl;
        arguments[11] = NULL;
        result = voidyt_run_process(powershell, arguments);
    }
#else
    {
        const char *arguments[] = {"/bin/sh", installer, deps->app_dir, deps->curl, NULL};
        result = voidyt_run_process("/bin/sh", arguments);
    }
#endif
    if (result != 0 || !voidyt_path_exists(target)) {
        fprintf(stderr,
                "void-yt: FFmpeg installation failed; continuing with combined formats only.\n");
        return 0;
    }
    result = snprintf(deps->ffmpeg, sizeof(deps->ffmpeg), "%s", target);
    if (result < 0 || (size_t)result >= sizeof(deps->ffmpeg)) {
        deps->ffmpeg[0] = '\0';
        return 0;
    }
    printf("FFmpeg is ready: %s\n", deps->ffmpeg);
    return 1;
}
