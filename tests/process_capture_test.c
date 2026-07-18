#include "subprocess.h"

#include <stdio.h>
#include <string.h>

typedef struct captured_lines {
    char text[256];
    size_t length;
} captured_lines;

static void capture_line(const char *line, void *context) {
    captured_lines *captured = (captured_lines *)context;
    size_t available = sizeof(captured->text) - captured->length - 1;
    size_t length = strlen(line);
    if (length > available) length = available;
    memcpy(captured->text + captured->length, line, length);
    captured->length += length;
    if (captured->length + 1 < sizeof(captured->text)) {
        captured->text[captured->length++] = '\n';
    }
    captured->text[captured->length] = '\0';
}

int main(int argc, char **argv) {
    const char *child_args[3];
    captured_lines captured = {{0}, 0};
    int result;

    if (argc == 2 && strcmp(argv[1], "--child") == 0) {
        printf("captured stdout\n");
        fprintf(stderr, "captured stderr\n");
        return 7;
    }

    child_args[0] = argv[0];
    child_args[1] = "--child";
    child_args[2] = NULL;
    result = voidyt_run_process_capture(argv[0], child_args, capture_line, &captured);
    if (result != 7 || strstr(captured.text, "captured stdout") == NULL ||
        strstr(captured.text, "captured stderr") == NULL) {
        fprintf(stderr, "capture failed (exit %d):\n%s", result, captured.text);
        return 1;
    }
    return 0;
}
