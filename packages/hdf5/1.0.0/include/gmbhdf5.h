/* gmbhdf5 — wrapper completo sulla libreria HDF5 ufficiale.
 *
 * Non reimplementa il formato: usa libhdf5 (vendorizzata in
 * <progetto>/Resources/libs/<rid>/) e ne espone un'API comoda:
 *  - lettura/scrittura dataset F64/I64 (anche compressi deflate+shuffle)
 *  - gruppi, attributi scalari (numerici/stringa)
 *  - listing ricorsivo (nomi completi + tipo)
 *  - lettura "type-agnostic" in double e lettura di stringhe
 */
#ifndef GMBHDF5_H
#define GMBHDF5_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { GMB_H5_F64 = 0, GMB_H5_I64 = 1 } gmb_h5_type;
typedef enum { GMB_H5_DATASET = 0, GMB_H5_GROUP = 1, GMB_H5_OTHER = 2 } gmb_h5_kind_t;

typedef struct gmb_h5_file gmb_h5_file;

/* --- ciclo di vita --- */
gmb_h5_file *gmb_h5_create(const char *path);
gmb_h5_file *gmb_h5_open(const char *path);
int gmb_h5_close(gmb_h5_file *h);

/* --- scrittura --- */
int gmb_h5_write(gmb_h5_file *h, const char *name, gmb_h5_type type, int rank,
                 const int64_t *dims, const void *data);
/* come write, con compressione deflate (0 = nessuna, 1..9) + shuffle. */
int gmb_h5_write_compressed(gmb_h5_file *h, const char *name, gmb_h5_type type, int rank,
                            const int64_t *dims, const void *data, int compress);
int gmb_h5_make_group(gmb_h5_file *h, const char *path);
/* attributi scalari su un oggetto (dataset/gruppo). */
int gmb_h5_write_attr_str(gmb_h5_file *h, const char *obj, const char *attr, const char *val);
int gmb_h5_write_attr_f64(gmb_h5_file *h, const char *obj, const char *attr, double val);

/* --- enumerazione (ricorsiva) --- */
int gmb_h5_count(const gmb_h5_file *h);
const char *gmb_h5_name(const gmb_h5_file *h, int k);
gmb_h5_kind_t gmb_h5_kind(const gmb_h5_file *h, int k);

/* --- lettura --- */
/* Dati grezzi (F64 come double, I64 come int64_t). *data va liberato con gmb_h5_free. */
int gmb_h5_read(const gmb_h5_file *h, const char *name, gmb_h5_type *type, int *rank,
                int64_t *dims, void **data, int64_t *nelem);
/* Converte qualunque dataset numerico (int/uint/float) in double. */
int gmb_h5_read_doubles(const gmb_h5_file *h, const char *name, double **out, int64_t *dims,
                        int *rank, int64_t *nelem);
/* Legge il primo elemento di un dataset di stringhe. */
int gmb_h5_read_string(const gmb_h5_file *h, const char *name, char *out, int cap);
/* Legge un attributo stringa. */
int gmb_h5_read_attr_str(const gmb_h5_file *h, const char *obj, const char *attr, char *out,
                         int cap);

void gmb_h5_free(void *data);
const char *gmb_h5_version(void);

#ifdef __cplusplus
}
#endif
#endif /* GMBHDF5_H */
