/* FILE: gmb_db_internal.h — handle table + error buffer (commercial C engines) */
#ifndef GMB_DB_INTERNAL_H
#define GMB_DB_INTERNAL_H

#include "gmb_db.h"
#include "gmb_db_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#ifndef _WIN32
#include <strings.h>
#else
#define strcasecmp _stricmp
#endif

#define GMB_DB_MAX_HANDLES 64
#define GMB_DB_MAX_FIELDS  64
#define GMB_DB_MAX_INDEXES 4
#define GMB_DB_ERRCAP      256

enum {
    GMB_H_FREE = 0,
    GMB_H_DBF,
    GMB_H_SPEED,
    GMB_H_LEDGER,
    GMB_H_ISAM,
    GMB_H_WAL,
    GMB_H_SPEED_WAL
};

typedef struct {
    char name[12];
    char type;
    unsigned char length;
    unsigned char decimals;
    unsigned int  offset; /* within record payload (after delete flag) */
} gmb_dbf_field_t;

typedef struct {
    char key[256];
    unsigned int recno;
} gmb_idx_entry_t;

typedef struct {
    struct DbfEngine* engine;
    FILE* fp; /* unused when engine is set */
    char  path[1024];
    unsigned char version;
    unsigned int  rec_count;
    unsigned short header_len;
    unsigned short rec_len;
    int field_count;
    gmb_dbf_field_t fields[GMB_DB_MAX_FIELDS];
    int recno; /* 1-based; 0 = BOF; count+1 = EOF */
    int eof;
    int bof;
    int found;
    int index_count;
    char index_path[GMB_DB_MAX_INDEXES][1024];
    int  index_key_field[GMB_DB_MAX_INDEXES];
    int  index_key_len[GMB_DB_MAX_INDEXES];
    int  active_order; /* 0 = physical */
    char alias[64];    /* DBF USE ... ALIAS <n>: ALIAS()/SELECTED() */
    int area;          /* numero d'area 1-based (ordine di apertura), 0 = chiusa */
    int exclusive;     /* DBF SET EXCLUSIVE: flock esclusivo sul file */
    int strict_schema; /* DBF SET SCHEMA ON: validazione schema all'open */
    gmb_idx_entry_t* idx;
    int idx_n;
    int idx_pos;
} gmb_dbf_t;

typedef struct {
    struct Database* db;
    int curTable;
    int durability; /* SPEED SET DURABILITY: 0=Full, 1=Buffered */
    char filter[1024]; /* SPEED SET FILTER: expression for RuntimeExpressionEvaluator */
} gmb_speed_t;

typedef struct {
    struct LedgerContext* ctx;
    int cursor; /* 0-based indice nella lista transazioni ordinate per ID;
                   -1 = BOF (prima della prima), nIndex = EOF (dopo l'ultima) */
} gmb_ledger_t;

typedef struct {
    struct Engine* engine;
    IsamSchema schema;
    IsamFieldSchema fields[GMB_DB_MAX_FIELDS];
    IsamIndexDefinition secs[GMB_DB_MAX_INDEXES];
    char idx_field[256];   /* campo della clausola INDEXED BY (parità C#: PK+secondary) */
    char path[1024];
    char alias[64];
} gmb_isam_t;

typedef struct {
    FILE* fp;
    char  path[1024];
    int   count;
} gmb_wal_t;

typedef struct {
    int kind;
    union {
        gmb_dbf_t    dbf;
        gmb_speed_t  speed;
        gmb_ledger_t ledger;
        gmb_isam_t   isam;
        gmb_wal_t    wal;
    } u;
} gmb_handle_t;

extern gmb_handle_t g_handles[GMB_DB_MAX_HANDLES];
extern char g_db_err[GMB_DB_ERRCAP];

int64_t gmb_db_alloc(int kind);
gmb_handle_t* gmb_db_get(int64_t h, int kind);
void gmb_db_set_err(const char* msg);
void gmb_db_write_u16(FILE* f, unsigned short v);
void gmb_db_write_u32(FILE* f, unsigned int v);
unsigned short gmb_db_read_u16(const unsigned char* p);
unsigned int   gmb_db_read_u32(const unsigned char* p);
int gmb_db_split_csv(const char* csv, char out[][256], int maxn);
void gmb_db_trim_field(char* s);
void gmb_db_dbf_after_close(int64_t h);   /* ripristina l'area corrente DBF dopo la chiusura */
void gmb_dbf_sync(gmb_handle_t* hh);

#endif
