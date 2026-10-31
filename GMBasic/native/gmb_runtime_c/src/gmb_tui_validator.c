/* ============================================================================
 * gmb_tui_validator.c — gmb_tui_validator. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== Validator ===================== */

static bool validator_int_check(const gmb_validator_t* v, const char* text)
{
    if (!text || !*text) return false;
    char* end = NULL;
    long val = strtol(text, &end, 10);
    if (end == text || *end != '\0') return false;
    return val >= v->v_min && val <= v->v_max;
}
static const char* validator_int_message(const gmb_validator_t* v, const char* text)
{
    if (validator_int_check(v, text)) return NULL;
    snprintf((char*)v->error_buf, sizeof(v->error_buf), "Deve essere un intero tra %ld e %ld", v->v_min, v->v_max);
    return v->error_buf;
}
static bool validator_float_check(const gmb_validator_t* v, const char* text)
{
    if (!text || !*text) return false;
    char* end = NULL;
    double val = strtod(text, &end);
    if (end == text || *end != '\0') return false;
    return val >= (double)v->v_min && val <= (double)v->v_max;
}
static bool validator_required_check(const gmb_validator_t* v, const char* text)
{
    (void)v;
    if (!text) return false;
    while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r') text++;
    return *text != '\0';
}
static bool validator_oneof_check(const gmb_validator_t* v, const char* text)
{
    if (!v->oneof_values) return false;
    for (int i = 0; i < v->oneof_count; i++)
        if (v->oneof_values[i] && strcmp(v->oneof_values[i], text) == 0) return true;
    return false;
}
static const char* validator_oneof_message(const gmb_validator_t* v, const char* text)
{
    if (validator_oneof_check(v, text)) return NULL;
    return "Valore non ammesso";
}
static bool validator_fn_check(const gmb_validator_t* v, const char* text)
{
    if (!v->fn) return true;
    return v->fn(v->fn_ctx, text);
}

void gmb_validator_int(gmb_validator_t* v, long min, long max)
{
    memset(v, 0, sizeof(*v));
    v->is_numeric = true;
    v->v_min = min;
    v->v_max = max;
    v->is_valid = validator_int_check;
    v->error = validator_int_message;
}
void gmb_validator_float(gmb_validator_t* v, double min, double max)
{
    memset(v, 0, sizeof(*v));
    v->is_numeric = true;
    v->v_min = (long)min;
    v->v_max = (long)max;
    v->is_valid = validator_float_check;
}
void gmb_validator_fn(gmb_validator_t* v, bool (*fn)(void*, const char*), void* ctx, const char* message)
{
    memset(v, 0, sizeof(*v));
    v->fn = fn;
    v->fn_ctx = ctx;
    v->fn_message = message;
    v->is_valid = validator_fn_check;
}
void gmb_validator_required(gmb_validator_t* v)
{
    v->is_valid = validator_required_check;
    v->error = NULL;
}
void gmb_validator_oneof(gmb_validator_t* v, const char* const* values, int count)
{
    v->oneof_values = values;
    v->oneof_count = count;
    v->is_valid = validator_oneof_check;
    v->error = validator_oneof_message;
}
void gmb_validator_pathfree(gmb_validator_t* v)
{
    v->is_valid = NULL;
}
bool gmb_validator_is_valid(const gmb_validator_t* v, const char* text)
{
    if (!v) return false;
    if (v->is_valid) return v->is_valid(v, text);
    return true;
}
bool gmb_validator_is_numeric(const gmb_validator_t* v)
{
    return v && v->is_numeric;
}
const char* gmb_validator_error(const gmb_validator_t* v, const char* text)
{
    if (!v) return "Valore non valido";
    if (v->error) return v->error(v, text);
    if (v->fn_message) return v->fn_message;
    if (!gmb_validator_is_valid(v, text)) return "Valore non valido";
    return NULL;
}
