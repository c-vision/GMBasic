/* verify.h — VERIFY loop ACTION→OBSERVE→VERIFY→RETRY (CUA §20)
 * Il runtime verifica ogni azione: file exists? dialog closed? a11y changed?
 * Ritorna SUCCESS → next action, FAILURE → retry/alternative. */
#ifndef GMB_VERIFY_H
#define GMB_VERIFY_H

typedef enum { GMB_VERIFY_SUCCESS = 0, GMB_VERIFY_RETRY = 1, GMB_VERIFY_FAIL = 2 } gmb_verify_result_t;

gmb_verify_result_t gmb_verify_file_exists(const char *path);
gmb_verify_result_t gmb_verify_a11y_changed(const char *before_json, const char *after_json);
gmb_verify_result_t gmb_verify_dom_contains(const char *html, const char *needle);

#endif
