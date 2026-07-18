#ifndef _WIN32
#define _XOPEN_SOURCE 700
#endif

#include "interactive.h"

#include "subprocess.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <windows.h>
#define VOIDYT_PATH_SEP '\\'
#else
#include <termios.h>
#include <unistd.h>
#define VOIDYT_PATH_SEP '/'
#endif

#define VOIDYT_MAX_CHOICES 32
#define VOIDYT_TITLE_CAP 512

typedef struct format_choice {
    int audio_only;
    int height;
    unsigned long long bytes;
    char extension[16];
    char selector[VOIDYT_FORMAT_CAP];
} format_choice;

enum menu_key {
    MENU_KEY_NONE,
    MENU_KEY_UP,
    MENU_KEY_DOWN,
    MENU_KEY_HOME,
    MENU_KEY_END,
    MENU_KEY_ENTER,
    MENU_KEY_CANCEL
};

static int copy_string(char *dest, size_t cap, const char *source) {
    size_t length;
    if (dest == NULL || source == NULL || cap == 0) {
        return 0;
    }
    length = strlen(source);
    if (length >= cap) {
        return 0;
    }
    memcpy(dest, source, length + 1);
    return 1;
}

static int join_path(char *dest, size_t cap, const char *left, const char *right) {
    size_t left_length;
    int needs_separator;
    if (dest == NULL || left == NULL || right == NULL || cap == 0) {
        return 0;
    }
    left_length = strlen(left);
    needs_separator = left_length > 0 && left[left_length - 1] != '/' &&
                      left[left_length - 1] != '\\';
    if (left_length + (size_t)needs_separator + strlen(right) + 1 > cap) {
        return 0;
    }
    memcpy(dest, left, left_length);
    if (needs_separator) {
        dest[left_length++] = VOIDYT_PATH_SEP;
    }
    memcpy(dest + left_length, right, strlen(right) + 1);
    return 1;
}

static int terminal_is_interactive(void) {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) && _isatty(_fileno(stdout));
#else
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
#endif
}

static int make_temp_file(char *dest, size_t cap) {
#ifdef _WIN32
    char temp_directory[VOIDYT_PATH_CAP];
    char temp_file[MAX_PATH];
    DWORD length = GetTempPathA((DWORD)sizeof(temp_directory), temp_directory);
    if (length == 0 || length >= sizeof(temp_directory) ||
        GetTempFileNameA(temp_directory, "vyt", 0, temp_file) == 0) {
        return 0;
    }
    if (!copy_string(dest, cap, temp_file)) {
        remove(temp_file);
        return 0;
    }
    return 1;
#else
    const char *temp_directory = getenv("TMPDIR");
    int descriptor;
    int written;
    if (temp_directory == NULL || temp_directory[0] == '\0') {
        temp_directory = "/tmp";
    }
    written = snprintf(dest, cap, "%s/void-yt-menu.XXXXXX", temp_directory);
    if (written < 0 || (size_t)written >= cap) {
        return 0;
    }
    descriptor = mkstemp(dest);
    if (descriptor < 0) {
        return 0;
    }
    close(descriptor);
    return 1;
#endif
}

static int split_fields(char *line, char *fields[], size_t field_count) {
    size_t index;
    char *cursor = line;
    for (index = 0; index < field_count; ++index) {
        char *tab;
        fields[index] = cursor;
        if (index + 1 == field_count) {
            return strchr(cursor, '\t') == NULL;
        }
        tab = strchr(cursor, '\t');
        if (tab == NULL) {
            return 0;
        }
        *tab = '\0';
        cursor = tab + 1;
    }
    return 1;
}

static int load_choices(const char *path,
                        char *title,
                        size_t title_cap,
                        format_choice choices[],
                        size_t *choice_count) {
    FILE *file = fopen(path, "rb");
    char line[2048];
    size_t count = 0;
    if (file == NULL) {
        return 0;
    }
    title[0] = '\0';
    while (fgets(line, sizeof(line), file) != NULL) {
        size_t length = strlen(line);
        char *fields[5];
        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = '\0';
        }
        if (strncmp(line, "title\t", 6) == 0) {
            copy_string(title, title_cap, line + 6);
            continue;
        }
        if (count >= VOIDYT_MAX_CHOICES || !split_fields(line, fields, 5)) {
            continue;
        }
        if (strcmp(fields[0], "video") != 0 && strcmp(fields[0], "audio") != 0) {
            continue;
        }
        choices[count].audio_only = strcmp(fields[0], "audio") == 0;
        choices[count].height = atoi(fields[1]);
        choices[count].bytes = strtoull(fields[3], NULL, 10);
        if (!copy_string(choices[count].extension, sizeof(choices[count].extension), fields[2]) ||
            !copy_string(choices[count].selector, sizeof(choices[count].selector), fields[4])) {
            continue;
        }
        ++count;
    }
    fclose(file);
    if (title[0] == '\0') {
        copy_string(title, title_cap, "Download");
    }
    *choice_count = count;
    return count > 0;
}

static void format_size(unsigned long long bytes, char *dest, size_t cap) {
    const double mib = (double)bytes / (1024.0 * 1024.0);
    if (bytes == 0) {
        snprintf(dest, cap, "size unknown");
    } else if (mib >= 100.0) {
        snprintf(dest, cap, "~%.0f MB", mib);
    } else if (mib >= 10.0) {
        snprintf(dest, cap, "~%.1f MB", mib);
    } else {
        snprintf(dest, cap, "~%.2f MB", mib);
    }
}

static void render_menu(const char *title,
                        const format_choice choices[],
                        size_t count,
                        size_t selected,
                        int redraw) {
    size_t index;
    if (redraw) {
        printf("\x1b[%zuA", count + 2);
    }
    printf("\r\x1b[2KDownload: %s\n", title);
    printf("\r\x1b[2KUse Up/Down and press Enter (Esc to cancel)\n");
    for (index = 0; index < count; ++index) {
        char size_text[32];
        format_size(choices[index].bytes, size_text, sizeof(size_text));
        printf("\r\x1b[2K%s", index == selected ? "\x1b[36m> " : "  ");
        if (choices[index].audio_only) {
            printf("audio only  |  %-4s  |  %s", choices[index].extension, size_text);
        } else {
            printf("%-5dp  |  %-4s  |  %s", choices[index].height,
                   choices[index].extension, size_text);
        }
        if (index == selected) {
            printf("\x1b[0m");
        }
        putchar('\n');
    }
    fflush(stdout);
}

#ifdef _WIN32
static enum menu_key read_menu_key(void) {
    int key = _getch();
    if (key == 0 || key == 224) {
        key = _getch();
        if (key == 72) return MENU_KEY_UP;
        if (key == 80) return MENU_KEY_DOWN;
        if (key == 71) return MENU_KEY_HOME;
        if (key == 79) return MENU_KEY_END;
        return MENU_KEY_NONE;
    }
    if (key == 13) return MENU_KEY_ENTER;
    if (key == 27 || key == 3) return MENU_KEY_CANCEL;
    if (key == 'k' || key == 'K') return MENU_KEY_UP;
    if (key == 'j' || key == 'J') return MENU_KEY_DOWN;
    return MENU_KEY_NONE;
}
#else
static enum menu_key read_menu_key(void) {
    unsigned char key;
    ssize_t read_count;
    do {
        read_count = read(STDIN_FILENO, &key, 1);
    } while (read_count == 0);
    if (read_count < 0) return MENU_KEY_CANCEL;
    if (key == '\r' || key == '\n') return MENU_KEY_ENTER;
    if (key == 3) return MENU_KEY_CANCEL;
    if (key == 'k' || key == 'K') return MENU_KEY_UP;
    if (key == 'j' || key == 'J') return MENU_KEY_DOWN;
    if (key == 27) {
        unsigned char sequence[2];
        if (read(STDIN_FILENO, sequence, 2) == 2 && sequence[0] == '[') {
            if (sequence[1] == 'A') return MENU_KEY_UP;
            if (sequence[1] == 'B') return MENU_KEY_DOWN;
            if (sequence[1] == 'H') return MENU_KEY_HOME;
            if (sequence[1] == 'F') return MENU_KEY_END;
        }
        return MENU_KEY_CANCEL;
    }
    return MENU_KEY_NONE;
}
#endif

static int choose_format(const char *title,
                         const format_choice choices[],
                         size_t count,
                         size_t *selected_out) {
    size_t selected = 0;
    int redraw = 0;
#ifdef _WIN32
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD original_mode = 0;
    int restore_mode = GetConsoleMode(output, &original_mode) != 0;
    if (restore_mode) {
        SetConsoleMode(output, original_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#else
    struct termios original_mode;
    struct termios raw_mode;
    if (tcgetattr(STDIN_FILENO, &original_mode) != 0) {
        return -1;
    }
    raw_mode = original_mode;
    raw_mode.c_lflag &= (tcflag_t)~(ICANON | ECHO | ISIG);
    raw_mode.c_cc[VMIN] = 0;
    raw_mode.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_mode) != 0) {
        return -1;
    }
#endif
    printf("\x1b[?25l");
    for (;;) {
        enum menu_key key;
        render_menu(title, choices, count, selected, redraw);
        redraw = 1;
        key = read_menu_key();
        if (key == MENU_KEY_UP) {
            selected = selected == 0 ? count - 1 : selected - 1;
        } else if (key == MENU_KEY_DOWN) {
            selected = (selected + 1) % count;
        } else if (key == MENU_KEY_HOME) {
            selected = 0;
        } else if (key == MENU_KEY_END) {
            selected = count - 1;
        } else if (key == MENU_KEY_ENTER || key == MENU_KEY_CANCEL) {
            printf("\x1b[?25h");
            fflush(stdout);
#ifdef _WIN32
            if (restore_mode) SetConsoleMode(output, original_mode);
#else
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_mode);
#endif
            if (key == MENU_KEY_CANCEL) return 0;
            *selected_out = selected;
            return 1;
        }
    }
}

static int default_download_directory(char *dest, size_t cap) {
#ifdef _WIN32
    char home[VOIDYT_PATH_CAP];
    DWORD home_length = GetEnvironmentVariableA("USERPROFILE", home, (DWORD)sizeof(home));
    if (home_length > 0 && home_length < sizeof(home) &&
        join_path(dest, cap, home, "Downloads")) {
        return 1;
    }
#else
    const char *home = getenv("HOME");
    if (home != NULL && home[0] != '\0' && join_path(dest, cap, home, "Downloads")) {
        return 1;
    }
#endif
#ifdef _WIN32
    {
        DWORD length = GetCurrentDirectoryA((DWORD)cap, dest);
        return length > 0 && (size_t)length < cap;
    }
#else
    return getcwd(dest, cap) != NULL;
#endif
}

static void strip_surrounding_quotes(char *value) {
    size_t length = strlen(value);
    if (length >= 2 && ((value[0] == '"' && value[length - 1] == '"') ||
                        (value[0] == '\'' && value[length - 1] == '\''))) {
        memmove(value, value + 1, length - 2);
        value[length - 2] = '\0';
    }
}

static int prompt_directory(char *dest, size_t cap) {
    char default_directory[VOIDYT_PATH_CAP];
    char input[VOIDYT_PATH_CAP];
    size_t length;
    if (!default_download_directory(default_directory, sizeof(default_directory))) {
        return 0;
    }
    printf("\nDownload folder [%s]: ", default_directory);
    fflush(stdout);
    if (fgets(input, sizeof(input), stdin) == NULL) {
        return 0;
    }
    length = strlen(input);
    while (length > 0 && (input[length - 1] == '\n' || input[length - 1] == '\r')) {
        input[--length] = '\0';
    }
    if (input[0] == '\0') {
        return copy_string(dest, cap, default_directory);
    }
    strip_surrounding_quotes(input);
#ifndef _WIN32
    if (input[0] == '~' && (input[1] == '/' || input[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home != NULL && home[0] != '\0') {
            return input[1] == '\0' ? copy_string(dest, cap, home) :
                                      join_path(dest, cap, home, input + 2);
        }
    }
#endif
    return copy_string(dest, cap, input);
}

int voidyt_prepare_interactive_download(const voidyt_dependencies *deps,
                                        const char *url,
                                        voidyt_download_selection *selection) {
    char metadata_path[VOIDYT_PATH_CAP] = "";
    char choices_path[VOIDYT_PATH_CAP] = "";
    char tools_directory[VOIDYT_PATH_CAP];
    char helper_script[VOIDYT_PATH_CAP];
    char runtime_argument[VOIDYT_PATH_CAP + 16];
    char title[VOIDYT_TITLE_CAP];
    format_choice choices[VOIDYT_MAX_CHOICES];
    size_t choice_count = 0;
    size_t selected = 0;
    int result = 0;
    int written;
    const char *metadata_args[14];
    const char *parser_args[6];

    if (deps == NULL || url == NULL || selection == NULL || !terminal_is_interactive() ||
        deps->ytdlp[0] == '\0' || deps->qjs[0] == '\0' || deps->ffmpeg[0] == '\0') {
        return 0;
    }
    memset(selection, 0, sizeof(*selection));
    written = snprintf(runtime_argument, sizeof(runtime_argument), "quickjs:%s", deps->qjs);
    if (!join_path(tools_directory, sizeof(tools_directory), deps->app_dir, "tools") ||
        !join_path(helper_script, sizeof(helper_script), tools_directory, "format-menu.js") ||
        !voidyt_path_exists(helper_script) || written < 0 ||
        (size_t)written >= sizeof(runtime_argument) ||
        !make_temp_file(metadata_path, sizeof(metadata_path)) ||
        !make_temp_file(choices_path, sizeof(choices_path))) {
        goto cleanup;
    }

    printf("Loading available formats...\n");
    fflush(stdout);
    metadata_args[0] = deps->ytdlp;
    metadata_args[1] = "--ignore-config";
    metadata_args[2] = "--no-playlist";
    metadata_args[3] = "--skip-download";
    metadata_args[4] = "--quiet";
    metadata_args[5] = "--print-to-file";
    metadata_args[6] = "%(.{title,duration,formats})j";
    metadata_args[7] = metadata_path;
    metadata_args[8] = "--js-runtimes";
    metadata_args[9] = runtime_argument;
    metadata_args[10] = "--";
    metadata_args[11] = url;
    metadata_args[12] = NULL;
    if (voidyt_run_process(deps->ytdlp, metadata_args) != 0) {
        fprintf(stderr, "void-yt: could not load formats; using yt-dlp defaults.\n");
        goto cleanup;
    }

    parser_args[0] = deps->qjs;
    parser_args[1] = "--std";
    parser_args[2] = helper_script;
    parser_args[3] = metadata_path;
    parser_args[4] = choices_path;
    parser_args[5] = NULL;
    if (voidyt_run_process(deps->qjs, parser_args) != 0 ||
        !load_choices(choices_path, title, sizeof(title), choices, &choice_count)) {
        fprintf(stderr, "void-yt: could not build the format menu; using yt-dlp defaults.\n");
        goto cleanup;
    }
    if (!choose_format(title, choices, choice_count, &selected)) {
        result = -1;
        goto cleanup;
    }
    if (!copy_string(selection->format, sizeof(selection->format), choices[selected].selector) ||
        !prompt_directory(selection->directory, sizeof(selection->directory))) {
        result = -1;
        goto cleanup;
    }
    selection->audio_only = choices[selected].audio_only;
    result = 1;

cleanup:
    if (metadata_path[0] != '\0') remove(metadata_path);
    if (choices_path[0] != '\0') remove(choices_path);
    return result;
}
