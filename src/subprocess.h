#ifndef VOIDYT_SUBPROCESS_H
#define VOIDYT_SUBPROCESS_H

typedef void (*voidyt_process_line_callback)(const char *line, void *context);

int voidyt_run_process(const char *program, const char *const arguments[]);
int voidyt_run_process_capture(const char *program,
                               const char *const arguments[],
                               voidyt_process_line_callback callback,
                               void *context);

#endif
