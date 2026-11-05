/*
 * hdf5_ffi.c — shim FFI del pacchetto `hdf5` (namespace BASIC HDF5).
 *
 * L'API C di gmbsuite/gmbhdf5 usa un handle opaco `gmb_h5_file*` e tipi `int64_t`/`double`:
 * qui l'handle viaggia come LONG (puntatore), gli array BASIC LONG arrivano come int64_t*,
 * le stringhe come char*.
 *
 * Convenzioni BASIC:
 *  - H5CREATE/H5OPEN ritornano l'handle (0 = errore), H5CLOSE lo libera;
 *  - H5SIZE(h, name) dice quanti elementi ha un dataset (per dimensionare out());
 *  - H5READF64/H5READI64/H5READDBL scrivono in out() (dimensionato >= H5SIZE) e riportano
 *    rank, dims() e nelem();
 *  - i nomi (H5NAME) e le stringhe (H5READSTR/H5READATTRSTR) tornano come STRING.
 */
#include "gmbhdf5.h"

#include <hdf5.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef long long i64;

#define H(h) ((gmb_h5_file *)(size_t)(h))

/*
 * HDF5 stampa un diagnostico multi-riga sulla console ogni volta che un'operazione fallisce
 * ("HDF5-DIAG: Error detected ..."). Per un binding BASIC è rumore: l'esito arriva già come
 * codice di ritorno (0 = ok, negativo = errore). Si disattiva UNA volta, al caricamento della
 * dylib, la stampa automatica sullo stack di default: i diagnosti restano disponibili a chi
 * usa l'API HDF5 direttamente, ma non inquinano l'output dei programmi BASIC.
 */
static void h5_quiet_errors(void) __attribute__((constructor));
static void h5_quiet_errors(void) { H5Eset_auto2(H5E_DEFAULT, NULL, NULL); }

i64 ofx_h5_create(const char *path) { return (i64)(size_t)gmb_h5_create(path); }
i64 ofx_h5_open(const char *path) { return (i64)(size_t)gmb_h5_open(path); }
i64 ofx_h5_close(i64 h)
{
    if (!h) return 0;
    return (i64)gmb_h5_close(H(h));
}

/* helper: copia dims/rank/nelem verso i buffer del chiamante */
static void copy_dims(const int64_t *dims, int rank, i64 *out_dims, i64 *nelem, i64 n)
{
    int i;
    if (out_dims != NULL) {
        for (i = 0; i < rank && i < 8; i++) out_dims[i] = dims[i];
    }
    if (nelem != NULL) *nelem = n;
}

i64 ofx_h5_write_f64(i64 h, const char *name, i64 rank, const i64 *dims, const double *data)
{
    int64_t d[8];
    int i;
    if (!h || !name || !dims || !data) return -1;
    for (i = 0; i < rank && i < 8; i++) d[i] = dims[i];
    return (i64)gmb_h5_write(H(h), name, GMB_H5_F64, (int)rank, d, data);
}

i64 ofx_h5_write_i64(i64 h, const char *name, i64 rank, const i64 *dims, const i64 *data)
{
    int64_t d[8];
    int i;
    if (!h || !name || !dims || !data) return -1;
    for (i = 0; i < rank && i < 8; i++) d[i] = dims[i];
    return (i64)gmb_h5_write(H(h), name, GMB_H5_I64, (int)rank, d, data);
}

i64 ofx_h5_write_f64c(i64 h, const char *name, i64 rank, const i64 *dims,
                      const double *data, i64 compress)
{
    int64_t d[8];
    int i;
    if (!h || !name || !dims || !data) return -1;
    for (i = 0; i < rank && i < 8; i++) d[i] = dims[i];
    return (i64)gmb_h5_write_compressed(H(h), name, GMB_H5_F64, (int)rank, d, data,
                                        (int)compress);
}

i64 ofx_h5_make_group(i64 h, const char *path)
{
    if (!h || !path) return -1;
    return (i64)gmb_h5_make_group(H(h), path);
}

i64 ofx_h5_write_attr_str(i64 h, const char *obj, const char *attr, const char *val)
{
    if (!h || !obj || !attr || !val) return -1;
    return (i64)gmb_h5_write_attr_str(H(h), obj, attr, val);
}

i64 ofx_h5_write_attr_f64(i64 h, const char *obj, const char *attr, double val)
{
    if (!h || !obj || !attr) return -1;
    return (i64)gmb_h5_write_attr_f64(H(h), obj, attr, val);
}

i64 ofx_h5_count(i64 h) { return h ? (i64)gmb_h5_count(H(h)) : -1; }

const char *ofx_h5_name(i64 h, i64 k)
{
    if (!h) return "";
    return gmb_h5_name(H(h), (int)k);
}

i64 ofx_h5_kind(i64 h, i64 k) { return h ? (i64)gmb_h5_kind(H(h), (int)k) : -1; }

/* Quanti elementi ha il dataset `name` (per dimensionare out() in BASIC). */
i64 ofx_h5_size(i64 h, const char *name)
{
    gmb_h5_type ty;
    int rank = 0;
    int64_t dims[8];
    void *data = NULL;
    int64_t n = 0;
    if (!h || !name) return -1;
    if (gmb_h5_read(H(h), name, &ty, &rank, dims, &data, &n) != 0) return -1;
    gmb_h5_free(data);
    return (i64)n;
}

i64 ofx_h5_read_f64(i64 h, const char *name, double *out, i64 *dims, i64 *nelem)
{
    gmb_h5_type ty;
    int rank = 0;
    int64_t d[8];
    void *data = NULL;
    int64_t n = 0, i;
    if (!h || !name || !out) return -1;
    if (gmb_h5_read(H(h), name, &ty, &rank, d, &data, &n) != 0) return -1;
    if (ty != GMB_H5_F64) { gmb_h5_free(data); return -2; }
    for (i = 0; i < n; i++) out[i] = ((double *)data)[i];
    copy_dims(d, rank, dims, nelem, n);
    gmb_h5_free(data);
    return 0;
}

i64 ofx_h5_read_i64(i64 h, const char *name, i64 *out, i64 *dims, i64 *nelem)
{
    gmb_h5_type ty;
    int rank = 0;
    int64_t d[8];
    void *data = NULL;
    int64_t n = 0, i;
    if (!h || !name || !out) return -1;
    if (gmb_h5_read(H(h), name, &ty, &rank, d, &data, &n) != 0) return -1;
    if (ty != GMB_H5_I64) { gmb_h5_free(data); return -2; }
    for (i = 0; i < n; i++) out[i] = ((int64_t *)data)[i];
    copy_dims(d, rank, dims, nelem, n);
    gmb_h5_free(data);
    return 0;
}

/* Lettura type-agnostica (int/uint/float -> double). */
i64 ofx_h5_read_dbl(i64 h, const char *name, double *out, i64 *dims, i64 *nelem)
{
    double *data = NULL;
    int rank = 0;
    int64_t d[8];
    int64_t n = 0, i;
    if (!h || !name || !out) return -1;
    if (gmb_h5_read_doubles(H(h), name, &data, d, &rank, &n) != 0) return -1;
    for (i = 0; i < n; i++) out[i] = data[i];
    copy_dims(d, rank, dims, nelem, n);
    gmb_h5_free(data);
    return 0;
}

/* Stringhe: buffer statico per thread, copiato subito dal runtime FFI. */
static char g_str[1024];

const char *ofx_h5_read_str(i64 h, const char *name)
{
    g_str[0] = '\0';
    if (!h || !name) return g_str;
    if (gmb_h5_read_string(H(h), name, g_str, (int)sizeof(g_str)) != 0) g_str[0] = '\0';
    return g_str;
}

const char *ofx_h5_read_attr_str(i64 h, const char *obj, const char *attr)
{
    g_str[0] = '\0';
    if (!h || !obj || !attr) return g_str;
    if (gmb_h5_read_attr_str(H(h), obj, attr, g_str, (int)sizeof(g_str)) != 0) g_str[0] = '\0';
    return g_str;
}

const char *ofx_h5_version(void) { return gmb_h5_version(); }
