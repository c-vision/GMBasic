/* verify.c — implementazione VERIFY */
#include "verify.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>

gmb_verify_result_t gmb_verify_file_exists(const char *path) {
    if (!path || !*path) return GMB_VERIFY_FAIL;
    return access(path, F_OK) == 0 ? GMB_VERIFY_SUCCESS : GMB_VERIFY_RETRY;
}
gmb_verify_result_t gmb_verify_a11y_changed(const char *before_json, const char *after_json) {
    if (!before_json || !after_json) return GMB_VERIFY_FAIL;
    return strcmp(before_json, after_json) != 0 ? GMB_VERIFY_SUCCESS : GMB_VERIFY_RETRY;
}
gmb_verify_result_t gmb_verify_dom_contains(const char *html, const char *needle) {
    if (!html || !needle) return GMB_VERIFY_FAIL;
    return strstr(html, needle) ? GMB_VERIFY_SUCCESS : GMB_VERIFY_RETRY;
}
