#include "deps.h"
#include "ffmpeg.h"
#include "interactive.h"
#include "subprocess.h"
#include "update.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef VOIDYT_VERSION
#define VOIDYT_VERSION "dev"
#endif

#define VOIDYT_DOWNLOAD_LOG_CAP 65536
#define VOIDYT_PROGRESS_MARKER "VOIDYT_PROGRESS:"
#define VOIDYT_DONE_MARKER "VOIDYT_DONE:"

typedef struct voidyt_download_output {
    char logs[VOIDYT_DOWNLOAD_LOG_CAP];
    size_t log_length;
    int last_percent;
} voidyt_download_output;

static void render_download_progress(int percent) {
    char bar[31];
    int filled;
    int i;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    filled = percent * 30 / 100;
    for (i = 0; i < 30; ++i) {
        bar[i] = i < filled ? '#' : '-';
    }
    bar[30] = '\0';
    printf("\rDownloading [%s] %3d%%", bar, percent);
    fflush(stdout);
}

static void append_download_log(voidyt_download_output *output, const char *line) {
    size_t available;
    size_t length;
    if (output->log_length + 1 >= sizeof(output->logs)) return;
    available = sizeof(output->logs) - output->log_length - 1;
    length = strlen(line);
    if (length > available) length = available;
    memcpy(output->logs + output->log_length, line, length);
    output->log_length += length;
    if (output->log_length + 1 < sizeof(output->logs)) {
        output->logs[output->log_length++] = '\n';
    }
    output->logs[output->log_length] = '\0';
}

static void handle_download_output(const char *line, void *context) {
    voidyt_download_output *output = (voidyt_download_output *)context;
    const char *progress = strstr(line, VOIDYT_PROGRESS_MARKER);
    char *end = NULL;
    double value;
    int percent;
    if (progress != NULL) {
        progress += strlen(VOIDYT_PROGRESS_MARKER);
        value = strtod(progress, &end);
        if (end != progress) {
            percent = (int)(value + 0.5);
            if (percent != output->last_percent) {
                render_download_progress(percent);
                output->last_percent = percent;
            }
        }
        return;
    }
    if (strstr(line, VOIDYT_DONE_MARKER) != NULL) return;
    append_download_log(output, line);
}

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
            "Passing only a URL opens the quality menu and folder prompt.\n"
            "Additional yt-dlp options bypass the interactive menu.\n\n"
            "Missing FFmpeg is installed on the first doctor or download command.\n"
            "Set VOID_YT_NO_FFMPEG_INSTALL=1 to keep combined formats only.\n",
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
    size_t capacity = extra + 24;
    const char **child_args = (const char **)calloc(capacity, sizeof(char *));
    char runtime_arg[VOIDYT_PATH_CAP + 16];
    voidyt_download_selection selection;
    size_t index = 0;
    int interactive_result = 0;
    int i;
    int result;
    int written;

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
    if (!list_formats && argc == first_forwarded + 1) {
        interactive_result = voidyt_prepare_interactive_download(
            deps, argv[first_forwarded], &selection);
        if (interactive_result < 0) {
            free(child_args);
            printf("Download cancelled.\n");
            return 130;
        }
    }

    child_args[index++] = deps->ytdlp;
    child_args[index++] = "--newline";
    child_args[index++] = "--progress";
    if (deps->qjs[0] != '\0') {
        written = snprintf(runtime_arg, sizeof(runtime_arg), "quickjs:%s", deps->qjs);
        if (written < 0 || (size_t)written >= sizeof(runtime_arg)) {
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
    if (interactive_result > 0) {
        child_args[index++] = "--no-playlist";
        child_args[index++] = "--no-colors";
        child_args[index++] = "--progress-template";
        child_args[index++] = "download:" VOIDYT_PROGRESS_MARKER "%(progress._percent_str)s";
        child_args[index++] = "--print";
        child_args[index++] = "after_move:" VOIDYT_DONE_MARKER "%(filepath)s";
        child_args[index++] = "--format";
        child_args[index++] = selection.format;
        child_args[index++] = "--paths";
        child_args[index++] = selection.directory;
        if (selection.audio_only) {
            child_args[index++] = "--extract-audio";
            child_args[index++] = "--audio-format";
            child_args[index++] = "mp3";
        }
    }
    if (list_formats) {
        child_args[index++] = "--list-formats";
    }
    for (i = first_forwarded; i < argc; ++i) {
        child_args[index++] = argv[i];
    }
    child_args[index] = NULL;

    if (interactive_result > 0) {
        voidyt_download_output output;
        memset(&output, 0, sizeof(output));
        output.last_percent = -1;
        render_download_progress(0);
        result = voidyt_run_process_capture(
            deps->ytdlp, child_args, handle_download_output, &output);
        if (result == 0) {
            if (output.last_percent < 100) render_download_progress(100);
            printf("\nDownload complete: %s\n", selection.directory);
        } else {
            printf("\n");
            fprintf(stderr, "Download failed (exit code %d).\n", result);
            if (output.log_length > 0) {
                fprintf(stderr, "\n%s", output.logs);
            } else {
                fprintf(stderr, "No diagnostic output was returned by yt-dlp.\n");
            }
        }
    } else {
        result = voidyt_run_process(deps->ytdlp, child_args);
    }
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
        voidyt_ensure_ffmpeg(&deps);
        return run_doctor(&deps);
    }
    if (strcmp(argv[1], "download") == 0) {
        if (argc < 3) {
            fprintf(stderr, "void-yt: download requires a URL\n");
            return 2;
        }
        voidyt_ensure_ffmpeg(&deps);
        return run_ytdlp(&deps, 0, 2, argc, argv);
    }
    if (strcmp(argv[1], "formats") == 0) {
        if (argc < 3) {
            fprintf(stderr, "void-yt: formats requires a URL\n");
            return 2;
        }
        voidyt_ensure_ffmpeg(&deps);
        return run_ytdlp(&deps, 1, 2, argc, argv);
    }
    if (looks_like_url(argv[1])) {
        voidyt_ensure_ffmpeg(&deps);
        return run_ytdlp(&deps, 0, 1, argc, argv);
    }

    fprintf(stderr, "void-yt: unknown command: %s\n\n", argv[1]);
    print_usage(stderr);
    return 2;
}
