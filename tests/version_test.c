#include "update.h"

#include <stdio.h>

static int expect(const char *current, const char *latest, int expected) {
    int actual = 99;
    if (!voidyt_compare_versions(current, latest, &actual) || actual != expected) {
        fprintf(stderr, "comparison failed: %s vs %s (got %d, expected %d)\n",
                current, latest, actual, expected);
        return 0;
    }
    return 1;
}

int main(void) {
    int comparison;
    if (!expect("0.1.0", "0.2.0", -1) ||
        !expect("1.2.3", "1.2.3", 0) ||
        !expect("2.0.0", "1.99.99", 1) ||
        !expect("v1.10.2", "1.11.0", -1) ||
        voidyt_compare_versions("dev", "1.0.0", &comparison) ||
        voidyt_compare_versions("1.0", "1.0.1", &comparison) ||
        voidyt_compare_versions("1.0.0-bad", "1.0.1", &comparison)) {
        return 1;
    }
    return 0;
}
