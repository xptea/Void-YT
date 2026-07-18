#ifndef VOIDYT_UPDATE_H
#define VOIDYT_UPDATE_H

#include "deps.h"

int voidyt_compare_versions(const char *current, const char *latest, int *comparison);
int voidyt_maybe_auto_update(const voidyt_dependencies *deps, int argc, char **argv);

#endif
