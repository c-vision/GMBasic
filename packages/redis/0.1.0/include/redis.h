/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * redis.h — client Redis (RESP/2 su TCP). Libreria C99 con UNA sola dipendenza di
 * sistema: i socket BSD (nessun hiredis). I simboli esportati hanno lo STESSO NOME del
 * membro FFI: nessun prefisso, nessun ALIAS (il namespace `REDIS` copre già il contesto).
 * Handle = long (puntatore opaco); stringhe = const char*; interi = long. */
#ifndef REDIS_H
#define REDIS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

long        AVAILABLE(void);                                          /* 1 = lib presente */
long        CONNECT(const char* host, long port);                     /* handle, 0 = errore */
long        ATTACH(long fd);                                          /* da un socket aperto (host/test) */
long        CLOSE(long h);                                            /* 0 */
long        ALIVE(long h);                                            /* 1 connesso */
const char* ERR(long h);                                              /* ultimo errore ("" se nessuno) */

long        AUTH(long h, const char* password);                       /* 1 ok */
long        SELECT(long h, long db);                                  /* 1 ok */
long        PING(long h);                                             /* 1 ok */

long        SET(long h, const char* key, const char* val);            /* 1 ok */
long        SETEX(long h, const char* key, long secs, const char* val);
const char* GET(long h, const char* key);                             /* NULL se nil/errore */
long        DEL(long h, const char* key);                             /* n. cancellate, -1 err */
long        EXISTS(long h, const char* key);                          /* 0/1 */
long        INCR(long h, const char* key, long by);                   /* nuovo valore, -1 err */
long        EXPIRE(long h, const char* key, long secs);               /* 0/1 */
long        TTL(long h, const char* key);                             /* secondi, -1 err */
long        STRLEN(long h, const char* key);                          /* len, -1 err */

long        HSET(long h, const char* key, const char* field, const char* val);
const char* HGET(long h, const char* key, const char* field);
long        HDEL(long h, const char* key, const char* field);
long        HEXISTS(long h, const char* key, const char* field);
long        HLEN(long h, const char* key);

long        LPUSH(long h, const char* key, const char* val);
long        LLEN(long h, const char* key);
const char* LPOP(long h, const char* key);

long        SADD(long h, const char* key, const char* member);
long        SISMEMBER(long h, const char* key, const char* member);
long        SCARD(long h, const char* key);

long        PUBLISH(long h, const char* channel, const char* msg);
long        DBSIZE(long h);
long        FLUSHDB(long h);

/* comando generico: cmdline = "CMD arg1 arg2" (split su spazio). 1 ok, -1 errore. */
long        DO(long h, const char* cmdline);
/* introspezione dell'ultima risposta: tipo 0=none 1=status 2=error 3=int 4=bulk 5=nil 6=array */
long        REPLYTYPE(long h);
const char* REPLYSTR(long h);
long        REPLYINT(long h);
long        REPLYCOUNT(long h);

#ifdef __cplusplus
}
#endif
#endif /* REDIS_H */
