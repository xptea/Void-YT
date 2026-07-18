#ifndef VOIDYT_INTERACTIVE_H
#define VOIDYT_INTERACTIVE_H

#include "deps.h"

#define VOIDYT_FORMAT_CAP 512

typedef struct voidyt_download_selection {
    char format[VOIDYT_FORMAT_CAP];
    char directory[VOIDYT_PATH_CAP];
    int audio_only;
} voidyt_download_selection;

/* Returns 1 for a selection, 0 when unavailable, and -1 when cancelled. */
int voidyt_prepare_interactive_download(const voidyt_dependencies *deps,
                                        const char *url,
                                        voidyt_download_selection *selection);

#endif
