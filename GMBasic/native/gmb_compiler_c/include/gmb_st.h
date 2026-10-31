/* gmb_st.h — reader/writer minimale per safetensors (header JSON + data blob).
 *
 * Formato: [u64 LE header_len][header JSON][data]. `data_offsets` sono relativi all'inizio del
 * blocco dati. Supporta i dtype che servono al quantizzatore (F32/F16/BF16 + U32 per l'output).
 * Header paddato a multiplo di 8 (pratica comune). Non gestisce sharding multiplo in scrittura. */
#ifndef GMB_ST_H
#define GMB_ST_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char     name[256];
    char     dtype[8];       /* "F32","F16","BF16","U32",... */
    uint32_t ndim;
    uint64_t shape[8];   /* 5D+ (conv3d) presenti in modelli multimodali */
    uint64_t off0, off1;     /* offset nel blocco dati */
} GstEntry;

/* Elenca i tensori dell'header (esclude `__metadata__`). Caller: free(*ents).
 * K7: valida bounds data_offsets nel file, dtype/shape vs byte, nomi duplicati e overlap
 * (rifiuto esplicito su stderr, mai silenzioso). */
int gst_read_header(const char* path, GstEntry** ents, uint32_t* n_out, uint64_t* header_len_out);

/* K7: raccolta shard (index o singolo model.safetensors) con controlli ESPLICITI:
 * esistenza di OGNI shard elencato e overflow del `max`.
 * Ritorna 1 = ok, 0 = nessun safetensors, -1 = errore (messaggio su stderr). */
int gst_collect_shards(const char* src_dir, char shards[][96], int max, int* n_out);

/* K7: verifica PER-SHARD dell'index `weight_map` (tensor -> shard): ogni shard esiste e il suo
 * header contiene esattamente i tensori mappati (count + membership, un tensore una volta).
 * Senza index: valida il singolo `model.safetensors`. 0 = ok, -1 = errore (stderr). */
int gst_verify_shards(const char* src_dir);

/* Legge i byte grezzi del tensore `e` (l'intero). Caller: free(*data). */
int gst_read_tensor(const char* path, const GstEntry* e, uint8_t** data, size_t* n_out);

/* Writer: accumula i tensori in RAM e li scrive in `finish` (adatto a modelli piccoli/medi;
 * per modelli grandi serve uno streaming writer). */
typedef struct GstWriter GstWriter;
int gst_writer_create(GstWriter** out, const char* path);
int gst_writer_add(GstWriter* w, const char* name, const char* dtype,
                   uint32_t ndim, const uint64_t* shape, const void* data, size_t nbytes);
int gst_writer_finish(GstWriter* w);

#ifdef __cplusplus
}
#endif

#endif /* GMB_ST_H */
