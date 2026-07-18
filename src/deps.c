#ifndef _WIN32
#define _XOPEN_SOURCE 700
#endif

#include "deps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define VOIDYT_PATH_SEP '\\'
#define VOIDYT_PATH_LIST_SEP ';'
#else
#include <limits.h>
#include <unistd.h>
#define VOIDYT_PATH_SEP '/'
#define VOIDYT_PATH_LIST_SEP ':'
#endif

static int copy_string(char *dest, size_t cap, const char *source) {
    size_t len;
    if (dest == NULL || source == NULL || cap == 0) {
        return 0;
    }
    len = strlen(source);
    if (len >= cap) {
        return 0;
    }
    memcpy(dest, source, len + 1);
    return 1;
}

int voidyt_path_exists(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
#ifdef _WIN32
    {
        DWORD attrs = GetFileAttributesA(path);
        return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
    }
#else
    return access(path, X_OK) == 0;
#endif
}

static int join_path(char *dest, size_t cap, const char *left, const char *right) {
    size_t left_len;
    int needs_sep;
    if (left == NULL || right == NULL) {
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

static int find_on_path(const char *filename, char *dest, size_t cap) {
    const char *path = getenv("PATH");
    const char *cursor;
    if (path == NULL || filename == NULL) {
        return 0;
    }

    cursor = path;
    while (*cursor != '\0') {
        const char *end = strchr(cursor, VOIDYT_PATH_LIST_SEP);
        size_t len = end == NULL ? strlen(cursor) : (size_t)(end - cursor);
        char directory[VOIDYT_PATH_CAP];
        char candidate[VOIDYT_PATH_CAP];
        if (len > 0 && len < sizeof(directory)) {
            memcpy(directory, cursor, len);
            directory[len] = '\0';
            if (join_path(candidate, sizeof(candidate), directory, filename) &&
                voidyt_path_exists(candidate)) {
                return copy_string(dest, cap, candidate);
            }
        }
        if (end == NULL) {
            break;
        }
        cursor = end + 1;
    }
    return 0;
}

static int resolve_app_dir(const char *argv0, char *dest, size_t cap) {
    char full[VOIDYT_PATH_CAP];
    char *slash;
#ifdef _WIN32
    DWORD length = GetModuleFileNameA(NULL, full, (DWORD)sizeof(full));
    (void)argv0;
    if (length == 0 || length >= sizeof(full)) {
        return 0;
    }
#else
    char candidate[VOIDYT_PATH_CAP];
    if (argv0 == NULL) {
        return 0;
    }
    if (strchr(argv0, '/') == NULL) {
        if (!find_on_path(argv0, candidate, sizeof(candidate))) {
            return 0;
        }
    } else if (!copy_string(candidate, sizeof(candidate), argv0)) {
        return 0;
    }
    if (realpath(candidate, full) == NULL) {
        return 0;
    }
#endif
    slash = strrchr(full, '/');
#ifdef _WIN32
    {
        char *backslash = strrchr(full, '\\');
        if (backslash != NULL && (slash == NULL || backslash > slash)) {
            slash = backslash;
        }
    }
#endif
    if (slash == NULL) {
        return copy_string(dest, cap, ".");
    }
    *slash = '\0';
    return copy_string(dest, cap, full);
}

static void discover_one(const char *environment_name,
                         const char *app_dir,
                         const char *bundled_name,
                         const char *path_name,
                         char *dest,
                         size_t cap) {
    const char *override = getenv(environment_name);
    char tools_dir[VOIDYT_PATH_CAP];
    char candidate[VOIDYT_PATH_CAP];

    dest[0] = '\0';
    if (override != NULL && voidyt_path_exists(override)) {
        copy_string(dest, cap, override);
        return;
    }
    if (join_path(tools_dir, sizeof(tools_dir), app_dir, "tools") &&
        join_path(candidate, sizeof(candidate), tools_dir, bundled_name) &&
        voidyt_path_exists(candidate)) {
        copy_string(dest, cap, candidate);
        return;
    }
    find_on_path(path_name, dest, cap);
}

int voidyt_discover_dependencies(const char *argv0, voidyt_dependencies *deps) {
    if (deps == NULL) {
        return 0;
    }
    memset(deps, 0, sizeof(*deps));
    if (!resolve_app_dir(argv0, deps->app_dir, sizeof(deps->app_dir))) {
        copy_string(deps->app_dir, sizeof(deps->app_dir), ".");
    }
#ifdef _WIN32
    discover_one("VOID_YT_YTDLP", deps->app_dir, "yt-dlp.exe", "yt-dlp.exe",
                 deps->ytdlp, sizeof(deps->ytdlp));
    discover_one("VOID_YT_QJS", deps->app_dir, "qjs.exe", "qjs.exe",
                 deps->qjs, sizeof(deps->qjs));
    discover_one("VOID_YT_FFMPEG", deps->app_dir, "ffmpeg.exe", "ffmpeg.exe",
                 deps->ffmpeg, sizeof(deps->ffmpeg));
#else
    discover_one("VOID_YT_YTDLP", deps->app_dir, "yt-dlp", "yt-dlp",
                 deps->ytdlp, sizeof(deps->ytdlp));
    discover_one("VOID_YT_QJS", deps->app_dir, "qjs", "qjs",
                 deps->qjs, sizeof(deps->qjs));
    discover_one("VOID_YT_FFMPEG", deps->app_dir, "ffmpeg", "ffmpeg",
                 deps->ffmpeg, sizeof(deps->ffmpeg));
#endif
    return deps->ytdlp[0] != '\0';
}
