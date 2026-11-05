/* gmbhdf5 — wrapper sulla libreria HDF5 ufficiale. */
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "gmbhdf5.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hdf5.h"

typedef struct { char *name; gmb_h5_kind_t kind; } gmb_obj;

struct gmb_h5_file {
    char path[512];
    int writing;
    hid_t file;
    gmb_obj *objs;
    int n, cap;
};

static void add_obj(gmb_h5_file *h, const char *name, gmb_h5_kind_t kind) {
    if (h->n >= h->cap) {
        int nc = h->cap ? h->cap * 2 : 16;
        gmb_obj *nn = (gmb_obj *)realloc(h->objs, (size_t)nc * sizeof(gmb_obj));
        if (nn == NULL) return;
        h->objs = nn; h->cap = nc;
    }
    h->objs[h->n].name = strdup(name);
    h->objs[h->n].kind = kind;
    h->n++;
}

const char *gmb_h5_version(void) {
    unsigned maj, min, rel;
    H5get_libversion(&maj, &min, &rel);
    static char buf[32];
    snprintf(buf, sizeof(buf), "%u.%u.%u", maj, min, rel);
    return buf;
}

gmb_h5_file *gmb_h5_create(const char *path) {
    if (path == NULL) return NULL;
    gmb_h5_file *h = (gmb_h5_file *)calloc(1, sizeof(*h));
    if (h == NULL) return NULL;
    strncpy(h->path, path, sizeof(h->path) - 1);
    h->writing = 1;
    h->file = H5Fcreate(path, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if (h->file < 0) { free(h); return NULL; }
    return h;
}

static hid_t native_type_for(gmb_h5_type t) {
    return (t == GMB_H5_F64) ? H5T_IEEE_F64LE : H5T_STD_I64LE;
}

int gmb_h5_write_compressed(gmb_h5_file *h, const char *name, gmb_h5_type type, int rank,
                            const int64_t *dims, const void *data, int compress) {
    if (h == NULL || !h->writing || name == NULL || rank < 1 || data == NULL) return -1;
    hsize_t hdims[8];
    for (int i = 0; i < rank; i++) hdims[i] = (hsize_t)dims[i];
    hid_t space = H5Screate_simple(rank, hdims, NULL);
    hid_t t = native_type_for(type);
    hid_t dcpl = H5P_DEFAULT;
    if (compress > 0) {
        dcpl = H5Pcreate(H5P_DATASET_CREATE);
        hsize_t chunk[8];
        for (int i = 0; i < rank; i++) chunk[i] = hdims[i] < 1024 ? hdims[i] : 1024;
        H5Pset_chunk(dcpl, rank, chunk);
        H5Pset_shuffle(dcpl);
        H5Pset_deflate(dcpl, (unsigned)compress);
    }
    hid_t dset = H5Dcreate2(h->file, name, t, space, H5P_DEFAULT, dcpl, H5P_DEFAULT);
    if (dcpl != H5P_DEFAULT) H5Pclose(dcpl);
    if (dset < 0) { H5Sclose(space); return -2; }
    int rc = H5Dwrite(dset, t, H5S_ALL, H5S_ALL, H5P_DEFAULT, data);
    H5Dclose(dset); H5Sclose(space);
    if (rc >= 0) add_obj(h, name, GMB_H5_DATASET);
    return (rc >= 0) ? 0 : -3;
}

int gmb_h5_write(gmb_h5_file *h, const char *name, gmb_h5_type type, int rank,
                 const int64_t *dims, const void *data) {
    return gmb_h5_write_compressed(h, name, type, rank, dims, data, 0);
}

int gmb_h5_make_group(gmb_h5_file *h, const char *path) {
    if (h == NULL || !h->writing || path == NULL) return -1;
    hid_t g = H5Gcreate2(h->file, path, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if (g < 0) return -2;
    H5Gclose(g);
    add_obj(h, path, GMB_H5_GROUP);
    return 0;
}

static int write_attr_str_on(hid_t loc, const char *attr, const char *val) {
    hsize_t dims[1] = { 1 };
    hid_t space = H5Screate_simple(1, dims, NULL);
    hid_t t = H5Tcopy(H5T_C_S1);
    H5Tset_size(t, strlen(val) + 1);
    H5Tset_strpad(t, H5T_STR_NULLTERM);
    hid_t a = H5Acreate2(loc, attr, t, space, H5P_DEFAULT, H5P_DEFAULT);
    int rc = (a >= 0) ? H5Awrite(a, t, val) : -1;
    if (a >= 0) H5Aclose(a);
    H5Tclose(t); H5Sclose(space);
    return (rc >= 0) ? 0 : -2;
}

int gmb_h5_write_attr_str(gmb_h5_file *h, const char *obj, const char *attr, const char *val) {
    if (h == NULL || !h->writing || obj == NULL || attr == NULL || val == NULL) return -1;
    hid_t loc = H5Oopen(h->file, obj, H5P_DEFAULT);
    if (loc < 0) return -3;
    int rc = write_attr_str_on(loc, attr, val);
    H5Oclose(loc);
    return rc;
}

int gmb_h5_write_attr_f64(gmb_h5_file *h, const char *obj, const char *attr, double val) {
    if (h == NULL || !h->writing || obj == NULL || attr == NULL) return -1;
    hid_t loc = H5Oopen(h->file, obj, H5P_DEFAULT);
    if (loc < 0) return -3;
    hid_t space = H5Screate(H5S_SCALAR);
    hid_t a = H5Acreate2(loc, attr, H5T_IEEE_F64LE, space, H5P_DEFAULT, H5P_DEFAULT);
    int rc = (a >= 0) ? H5Awrite(a, H5T_NATIVE_DOUBLE, &val) : -1;
    if (a >= 0) H5Aclose(a);
    H5Sclose(space); H5Oclose(loc);
    return (rc >= 0) ? 0 : -2;
}

/* --- enumerazione ricorsiva --- */
static herr_t visit_cb(hid_t obj, const char *name, const H5O_info2_t *info, void *opdata) {
    (void)obj;
    gmb_h5_file *h = (gmb_h5_file *)opdata;
    if (name == NULL || strcmp(name, ".") == 0) return 0;
    gmb_h5_kind_t k = GMB_H5_OTHER;
    if (info->type == H5O_TYPE_DATASET) k = GMB_H5_DATASET;
    else if (info->type == H5O_TYPE_GROUP) k = GMB_H5_GROUP;
    add_obj(h, name, k);
    return 0;
}

gmb_h5_file *gmb_h5_open(const char *path) {
    if (path == NULL) return NULL;
    gmb_h5_file *h = (gmb_h5_file *)calloc(1, sizeof(*h));
    if (h == NULL) return NULL;
    strncpy(h->path, path, sizeof(h->path) - 1);
    h->writing = 0;
    h->file = H5Fopen(path, H5F_ACC_RDONLY, H5P_DEFAULT);
    if (h->file < 0) { free(h); return NULL; }
    H5Ovisit3(h->file, H5_INDEX_NAME, H5_ITER_NATIVE, visit_cb, h, H5O_INFO_BASIC);
    return h;
}

int gmb_h5_count(const gmb_h5_file *h) { return h ? h->n : 0; }
const char *gmb_h5_name(const gmb_h5_file *h, int k) {
    if (h == NULL || k < 0 || k >= h->n) return NULL;
    return h->objs[k].name;
}
gmb_h5_kind_t gmb_h5_kind(const gmb_h5_file *h, int k) {
    if (h == NULL || k < 0 || k >= h->n) return GMB_H5_OTHER;
    return h->objs[k].kind;
}

/* --- lettura --- */
int gmb_h5_read(const gmb_h5_file *h, const char *name, gmb_h5_type *type, int *rank,
                int64_t *dims, void **data, int64_t *nelem) {
    if (h == NULL || h->writing || name == NULL || data == NULL) return -1;
    hid_t dset = H5Dopen2(h->file, name, H5P_DEFAULT);
    if (dset < 0) return -3;
    hid_t dt = H5Dget_type(dset);
    H5T_class_t cls = H5Tget_class(dt);
    hid_t space = H5Dget_space(dset);
    int r = H5Sget_simple_extent_ndims(space);
    hsize_t hdims[8];
    H5Sget_simple_extent_dims(space, hdims, NULL);
    int64_t total = 1;
    for (int i = 0; i < r; i++) total *= (int64_t)hdims[i];
    gmb_h5_type t = (cls == H5T_FLOAT) ? GMB_H5_F64 : GMB_H5_I64;
    if (type) *type = t;
    if (rank) *rank = r;
    if (dims) for (int i = 0; i < r; i++) dims[i] = (int64_t)hdims[i];
    if (nelem) *nelem = total;
    void *buf = malloc(((size_t)total * 8) ? (size_t)total * 8 : 1);
    if (buf == NULL) { H5Sclose(space); H5Tclose(dt); H5Dclose(dset); return -2; }
    hid_t mt = (t == GMB_H5_F64) ? H5T_NATIVE_DOUBLE : H5T_NATIVE_LLONG;
    herr_t rc = H5Dread(dset, mt, H5S_ALL, H5S_ALL, H5P_DEFAULT, buf);
    H5Sclose(space); H5Tclose(dt); H5Dclose(dset);
    if (rc < 0) { free(buf); return -4; }
    *data = buf;
    return 0;
}

int gmb_h5_read_doubles(const gmb_h5_file *h, const char *name, double **out, int64_t *dims,
                        int *rank, int64_t *nelem) {
    if (h == NULL || h->writing || name == NULL || out == NULL) return -1;
    hid_t dset = H5Dopen2(h->file, name, H5P_DEFAULT);
    if (dset < 0) return -3;
    hid_t dt = H5Dget_type(dset);
    H5T_class_t cls = H5Tget_class(dt);
    hid_t space = H5Dget_space(dset);
    int r = H5Sget_simple_extent_ndims(space);
    hsize_t hdims[8];
    H5Sget_simple_extent_dims(space, hdims, NULL);
    int64_t total = 1;
    for (int i = 0; i < r; i++) total *= (int64_t)hdims[i];
    if (rank) *rank = r;
    if (dims) for (int i = 0; i < r; i++) dims[i] = (int64_t)hdims[i];
    if (nelem) *nelem = total;
    double *buf = (double *)malloc((size_t)total * sizeof(double));
    if (buf == NULL) { H5Sclose(space); H5Tclose(dt); H5Dclose(dset); return -2; }
    int rc = 0;
    if (cls == H5T_FLOAT) {
        rc = H5Dread(dset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, buf);
    } else if (cls == H5T_INTEGER) {
        H5T_sign_t sign = H5Tget_sign(dt);
        if (sign == H5T_SGN_NONE) {
            unsigned long long *tmp = (unsigned long long *)malloc((size_t)total * 8);
            if (!tmp) rc = -1; else { rc = H5Dread(dset, H5T_NATIVE_ULLONG, H5S_ALL, H5S_ALL, H5P_DEFAULT, tmp);
                                      for (int64_t i = 0; i < total; i++) buf[i] = (double)tmp[i]; free(tmp); }
        } else {
            long long *tmp = (long long *)malloc((size_t)total * 8);
            if (!tmp) rc = -1; else { rc = H5Dread(dset, H5T_NATIVE_LLONG, H5S_ALL, H5S_ALL, H5P_DEFAULT, tmp);
                                      for (int64_t i = 0; i < total; i++) buf[i] = (double)tmp[i]; free(tmp); }
        }
    } else {
        rc = -1;
    }
    H5Sclose(space); H5Tclose(dt); H5Dclose(dset);
    if (rc < 0) { free(buf); return -4; }
    *out = buf;
    return 0;
}

int gmb_h5_read_string(const gmb_h5_file *h, const char *name, char *out, int cap) {
    if (h == NULL || h->writing || name == NULL || out == NULL || cap <= 0) return -1;
    hid_t dset = H5Dopen2(h->file, name, H5P_DEFAULT);
    if (dset < 0) return -3;
    hid_t dt = H5Dget_type(dset);
    if (H5Tget_class(dt) != H5T_STRING) { H5Tclose(dt); H5Dclose(dset); return -2; }
    size_t sz = H5Tget_size(dt);
    if ((int)sz > cap) sz = (size_t)cap - 1;
    char *tmp = (char *)calloc(1, H5Tget_size(dt) + 1);
    if (tmp == NULL) { H5Tclose(dt); H5Dclose(dset); return -4; }
    int rc = 0;
    hid_t space = H5Dget_space(dset);
    hsize_t one[1] = { 1 };
    hid_t mspace = H5Screate_simple(1, one, NULL);
    hsize_t mstart[1] = { 0 };
    H5Sselect_hyperslab(space, H5S_SELECT_SET, mstart, NULL, one, NULL);
    if (H5Dread(dset, dt, mspace, space, H5P_DEFAULT, tmp) < 0) rc = -1;
    else { strncpy(out, tmp, (size_t)cap - 1); out[cap - 1] = '\0'; }
    H5Sclose(mspace); H5Sclose(space); H5Tclose(dt); H5Dclose(dset); free(tmp);
    return rc;
}

int gmb_h5_read_attr_str(const gmb_h5_file *h, const char *obj, const char *attr, char *out,
                         int cap) {
    if (h == NULL || obj == NULL || attr == NULL || out == NULL || cap <= 0) return -1;
    hid_t loc = H5Oopen(h->file, obj, H5P_DEFAULT);
    if (loc < 0) return -3;
    hid_t a = H5Aopen(loc, attr, H5P_DEFAULT);
    if (a < 0) { H5Oclose(loc); return -2; }
    hid_t dt = H5Aget_type(a);
    size_t sz = H5Tget_size(dt) + 1;
    char *tmp = (char *)calloc(1, sz);
    int rc = -1;
    if (tmp != NULL && H5Aread(a, dt, tmp) >= 0) {
        strncpy(out, tmp, (size_t)cap - 1); out[cap - 1] = '\0'; rc = 0;
    }
    free(tmp); H5Tclose(dt); H5Aclose(a); H5Oclose(loc);
    return rc;
}

void gmb_h5_free(void *data) { free(data); }

int gmb_h5_close(gmb_h5_file *h) {
    if (h == NULL) return -1;
    if (h->file >= 0) H5Fclose(h->file);
    for (int i = 0; i < h->n; i++) free(h->objs[i].name);
    free(h->objs);
    free(h);
    return 0;
}
