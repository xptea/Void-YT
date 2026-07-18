#ifndef VOIDYT_DEPS_H
#define VOIDYT_DEPS_H

#include <stddef.h>

#define VOIDYT_PATH_CAP 4096

typedef struct voidyt_dependencies {
    char app_dir[VOIDYT_PATH_CAP];
    char ytdlp[VOIDYT_PATH_CAP];
    char qjs[VOIDYT_PATH_CAP];
    char ffmpeg[VOIDYT_PATH_CAP];
    char curl[VOIDYT_PATH_CAP];
} voidyt_dependencies;

int voidyt_discover_dependencies(const char *argv0, voidyt_dependencies *deps);
int voidyt_path_exists(const char *path);

#endif
