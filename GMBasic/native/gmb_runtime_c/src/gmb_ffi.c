/* ============================================================================
 * GMBasic — Native Module FFI helpers (gmb_ffi.c)
 * Implementazione copy-in/copy-out del contratto definito in include/gmb_ffi.h.
 * L'host (IL backend / C99 / VM) risolve `gmb_ffi_module_entry` nella .so/.dylib
 * del pacchetto e usa questi helper per marshalling sicuro e errori BASIC.
 * ============================================================================ */

#include "gmb_ffi.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* ============================================================================
 * 3. Marshalling
 * ============================================================================ */

gmb_ffi_val_t gmb_ffi_make_int(gmb_ffi_int_t v)
{
    return gmb_make_val_int(v);
}

gmb_ffi_val_t gmb_ffi_make_double(gmb_ffi_double_t v)
{
    return gmb_make_val_double(v);
}

gmb_ffi_val_t gmb_ffi_make_bool(gmb_ffi_bool_t v)
{
    return gmb_make_val_bool(v);
}

gmb_ffi_val_t gmb_ffi_make_string(const char* s)
{
    return gmb_make_val_string(s != NULL ? s : "");
}

gmb_ffi_val_t gmb_ffi_make_string_len(const char* s, size_t n)
{
    gmb_ffi_val_t v;
    if (s == NULL) { s = ""; n = 0; }
    v.kind = GMB_TYPE_STRING;
    v.v_string.data = (char*)malloc(n + 1);
    if (v.v_string.data == NULL) return gmb_make_val_string("");
    if (n > 0) memcpy(v.v_string.data, s, n);
    v.v_string.data[n] = '\0';
    v.v_string.length = n;
    v.v_string.ref_count = 1;
    return v;
}

gmb_ffi_val_t gmb_ffi_make_nil(void)
{
    return gmb_val_nil();
}

gmb_ffi_val_t gmb_ffi_make_closure(gmb_ffi_closure_t c)
{
    gmb_fn_t* p = (gmb_fn_t*)malloc(sizeof(gmb_fn_t));
    if (p == NULL) return gmb_val_nil();
    *p = c;
    return gmb_make_val_obj(gmb_obj_new_typed(p, "gmb_ffi_closure"));
}

gmb_ffi_closure_t gmb_ffi_get_closure(const gmb_ffi_val_t* v)
{
    gmb_ffi_closure_t r = { NULL, NULL };
    if (v == NULL || v->kind != GMB_TYPE_OBJECT || !v->v_ptr) return r;
    gmb_obj_t* o = (gmb_obj_t*)v->v_ptr;
    if (o == NULL || o->data == NULL) return r;
    return *(gmb_fn_t*)o->data;
}

gmb_ffi_int_t gmb_ffi_get_int(const gmb_ffi_val_t* v, gmb_ffi_int_t def)
{
    if (v == NULL) return def;
    switch (v->kind)
    {
        case GMB_TYPE_INT:    return v->v_int;
        case GMB_TYPE_DOUBLE: return (gmb_ffi_int_t)v->v_double;
        case GMB_TYPE_BOOL:   return v->v_bool ? 1 : 0;
        default: return def;
    }
}

gmb_ffi_double_t gmb_ffi_get_double(const gmb_ffi_val_t* v, gmb_ffi_double_t def)
{
    if (v == NULL) return def;
    switch (v->kind)
    {
        case GMB_TYPE_DOUBLE: return v->v_double;
        case GMB_TYPE_INT:    return (gmb_ffi_double_t)v->v_int;
        case GMB_TYPE_BOOL:   return v->v_bool ? 1.0 : 0.0;
        default: return def;
    }
}

gmb_ffi_bool_t gmb_ffi_get_bool(const gmb_ffi_val_t* v, gmb_ffi_bool_t def)
{
    if (v == NULL) return def;
    switch (v->kind)
    {
        case GMB_TYPE_BOOL:   return v->v_bool;
        case GMB_TYPE_INT:    return v->v_int != 0;
        case GMB_TYPE_DOUBLE: return v->v_double != 0.0;
        default: return def;
    }
}

const char* gmb_ffi_get_string(const gmb_ffi_val_t* v, size_t* len_out)
{
    if (len_out != NULL) *len_out = 0;
    if (v == NULL || v->kind != GMB_TYPE_STRING) return "";
    if (len_out != NULL) *len_out = v->v_string.length;
    return v->v_string.data != NULL ? v->v_string.data : "";
}

gmb_ffi_list_t gmb_ffi_get_list(const gmb_ffi_val_t* v)
{
    /* Lista boxed (gmb_make_val_list) arriva come GMB_TYPE_OBJECT con data="gmb_list";
     * un array nativo diretto è GMB_TYPE_ARRAY. Entrambi → gmb_val_as_list (copia). */
    if (v == NULL) return gmb_list_new();
    if (v->kind != GMB_TYPE_OBJECT && v->kind != GMB_TYPE_ARRAY) return gmb_list_new();
    return gmb_val_as_list(*v);
}

/* ============================================================================
 * Output multiplo (BYREF di tipo C)
 * ============================================================================ */

void gmb_ffi_set_double_out(double* out, gmb_ffi_double_t v)
{
    if (out != NULL) *out = v;
}

void gmb_ffi_set_int_out(gmb_ffi_int_t* out, gmb_ffi_int_t v)
{
    if (out != NULL) *out = v;
}

void gmb_ffi_set_bool_out(gmb_ffi_bool_t* out, gmb_ffi_bool_t v)
{
    if (out != NULL) *out = v;
}

void gmb_ffi_set_string_out(char* out_str, size_t out_cap, const char* s)
{
    if (out_str == NULL || out_cap == 0) return;
    if (s == NULL) { out_str[0] = '\0'; return; }
    size_t n = strlen(s);
    if (n >= out_cap) n = out_cap - 1;
    memcpy(out_str, s, n);
    out_str[n] = '\0';
}

/* ============================================================================
 * 4. Errori → BASIC ON ERROR
 * ============================================================================ */

static char g_last_error[512] = "";

void gmb_ffi_errorv(const char* fmt, va_list ap)
{
    vsnprintf(g_last_error, sizeof(g_last_error), fmt != NULL ? fmt : "gmb_ffi error", ap);
}

void gmb_ffi_error(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    gmb_ffi_errorv(fmt, ap);
    va_end(ap);
}

const char* gmb_ffi_last_error(void)
{
    return g_last_error;
}
