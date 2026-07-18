#include "deps.h"
#include "subprocess.h"
#include "update.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef VOIDYT_VERSION
#define VOIDYT_VERSION "dev"
#endif

static void print_usage(FILE *stream) {
    fprintf(stream,
            "Void-YT %s\n"
            "A small native front end for the bundled yt-dlp and QuickJS.\n\n"
            "Usage:\n"
            "  void-yt URL [yt-dlp options]\n"
            "  void-yt download URL [yt-dlp options]\n"
            "  void-yt formats URL [yt-dlp options]\n"
            "  void-yt doctor\n"
            "  void-yt --version\n\n"
            "Automatic updates are checked at launch. Set VOID_YT_NO_UPDATE=1\n"
            "to disable the check.\n\n"
            "FFmpeg is optional. Without it, Void-YT requests the best stream\n"
            "that already contains both video and audio.\n",
            VOIDYT_VERSION);
}

static void print_dependency(const char *name, const char *path, int required) {
    if (path[0] != '\0') {
        printf("[ok]      %-9s %s\n", name, path);
    } else {
        printf("[%s] %-9s not found\n", required ? "missing" : "optional", name);
    }
}

static int run_doctor(const voidyt_dependencies *deps) {
    printf("Void-YT %s\n", VOIDYT_VERSION);
    printf("Application directory: %s\n\n", deps->app_dir);
    print_dependency("yt-dlp", deps->ytdlp, 1);
    print_dependency("QuickJS", deps->qjs, 1);
    print_dependency("FFmpeg", deps->ffmpeg, 0);
    print_dependency("curl", deps->curl, 0);
    if (deps->ytdlp[0] == '\0' || deps->qjs[0] == '\0') {
        fprintf(stderr,
                "\nThis source build is missing a required tool. Official release "
                "archives bundle both tools.\n");
        return 1;
    }
    if (deps->ffmpeg[0] == '\0') {
        printf("\nFFmpeg features are disabled; combined audio/video downloads still work.\n");
    }
    if (deps->curl[0] == '\0') {
        printf("Automatic update checks are disabled because curl is unavailable.\n");
    }
    return 0;
}

static int looks_like_url(const char *value) {
    return value != NULL &&
           (strncmp(value, "http://", 7) == 0 || strncmp(value, "https://", 8) == 0);
}

static int run_ytdlp(const voidyt_dependencies *deps,
                     int list_formats,
                     int first_forwarded,
                     int argc,
                     char **argv) {
    size_t extra = (size_t)(argc - first_forwarded);
    size_t capacity = extra + 16;
    const char **child_args = (const char **)calloc(capacity, sizeof(char *));
    char runtime_arg[VOIDYT_PATH_CAP + 16];
    size_t index = 0;
    int i;
    int result;

    if (deps->ytdlp[0] == '\0') {
        fprintf(stderr,
                "void-yt: yt-dlp was not found. Run an official bundled release "
                "or set VOID_YT_YTDLP.\n");
        return 2;
    }
    if (child_args == NULL) {
        fprintf(stderr, "void-yt: out of memory\n");
        return 2;
    }

    child_args[index++] = deps->ytdlp;
    child_args[index++] = "--newline";
    child_args[index++] = "--progress";
    if (deps->qjs[0] != '\0') {
        if (snprintf(runtime_arg, sizeof(runtime_arg), "quickjs:%s", deps->qjs) < 0) {
            free(child_args);
            return 2;
        }
        child_args[index++] = "--js-runtimes";
        child_args[index++] = runtime_arg;
    } else {
        fprintf(stderr,
                "void-yt: warning: QuickJS was not found; full YouTube support "
                "may be unavailable.\n");
    }
    if (deps->ffmpeg[0] != '\0') {
        child_args[index++] = "--ffmpeg-location";
        child_args[index++] = deps->ffmpeg;
    } else if (!list_formats) {
        child_args[index++] = "--format";
        child_args[index++] = "best[acodec!=none][vcodec!=none]/best";
    }
    if (list_formats) {
        child_args[index++] = "--list-formats";
    }
    for (i = first_forwarded; i < argc; ++i) {
        child_args[index++] = argv[i];
    }
    child_args[index] = NULL;

    result = voidyt_run_process(deps->ytdlp, child_args);
    free(child_args);
    return result;
}

int main(int argc, char **argv) {
    voidyt_dependencies deps;
    voidyt_discover_dependencies(argv[0], &deps);

    if (voidyt_maybe_auto_update(&deps, argc, argv)) {
        return 0;
    }

    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0 ||
        strcmp(argv[1], "help") == 0) {
        print_usage(argc < 2 ? stderr : stdout);
        return argc < 2 ? 2 : 0;
    }
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "version") == 0) {
        printf("void-yt %s\n", VOIDYT_VERSION);
        return 0;
    }
    if (strcmp(argv[1], "doctor") == 0) {
        return run_doctor(&deps);
    }
    if (strcmp(argv[1], "download") == 0) {
        if (argc < 3) {
            fprintf(stderr, "void-yt: download requires a URL\n");
            return 2;
        }
        return run_ytdlp(&deps, 0, 2, argc, argv);
    }
    if (strcmp(argv[1], "formats") == 0) {
        if (argc < 3) {
            fprintf(stderr, "void-yt: formats requires a URL\n");
            return 2;
        }
        return run_ytdlp(&deps, 1, 2, argc, argv);
    }
    if (looks_like_url(argv[1])) {
        return run_ytdlp(&deps, 0, 1, argc, argv);
    }

    fprintf(stderr, "void-yt: unknown command: %s\n\n", argv[1]);
    print_usage(stderr);
    return 2;
}
