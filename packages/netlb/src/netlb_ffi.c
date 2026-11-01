/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * netlb_ffi.c — superficie FFI. I simboli esportati hanno lo STESSO NOME del membro:
 * nessun prefisso, nessun ALIAS. Firma omogenea all-LONG (handle = puntatore, stringhe = char*). */
#include "netlb.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define POOL(h) ((netlb_pool*)(intptr_t)(h))
/* Esporta SOLO i membri FFI (tutto il resto e' -fvisibility=hidden: API C netlb_* e interni). */
#if defined(__GNUC__) && !defined(_WIN32)
#define API __attribute__((visibility("default")))
#else
#define API
#endif

API long AVAILABLE(void) { return 1; }
API long POOLNEW(long policy, long seed) { return (long)(intptr_t)netlb_pool_new((netlb_policy)policy, (uint64_t)seed); }
API long RELEASE(long h) { netlb_free(POOL(h)); return 0; }
API long BACKENDADD(long h, const char* name, long weight_milli, long cap) {
    netlb_pool* p = POOL(h); if (!p) return -1;
    netlb_backend_spec s; memset(&s, 0, sizeof s);
    snprintf(s.name, sizeof s.name, "%s", name ? name : "be");
    s.weight = weight_milli > 0 ? (double)weight_milli / 1000.0 : 1.0;
    s.cap = cap > 0 ? (uint32_t)cap : 0;
    s.healthy = 1;
    return netlb_add_backend(p, &s);
}
API long BACKENDREMOVE(long h, long id) { return netlb_remove_backend(POOL(h), (netlb_backend_id)id); }
API long BACKENDCOUNT(long h) { return (long)netlb_backend_count(POOL(h)); }

API long SETPOLICY(long h, long policy) { netlb_set_policy(POOL(h), (netlb_policy)policy); return 0; }
API long SETWEIGHT(long h, long id, long w) { netlb_set_weight(POOL(h), (netlb_backend_id)id, w > 0 ? (double)w / 1000.0 : 1.0); return 0; }
API long SETCAP(long h, long id, long cap) { netlb_set_cap(POOL(h), (netlb_backend_id)id, cap > 0 ? (uint32_t)cap : 0); return 0; }
API long SLOWSTART(long h, long ms) { netlb_set_slow_start(POOL(h), (double)ms); return 0; }
API long RETRYBUDGET(long h, long r) { netlb_set_retry_budget(POOL(h), (double)r / 1000.0); return 0; }
API long MAGLEVBUILD(long h, long m) { return netlb_maglev_build(POOL(h), (uint32_t)m); }
API long SETSTICKY(long h, long ttl) { netlb_set_sticky(POOL(h), (double)ttl); return 0; }
API long SETHRWSKEL(long h, long m, long f) { netlb_set_hrw_skeleton(POOL(h), (uint32_t)(m > 0 ? m : 0), (uint32_t)(f > 0 ? f : 0)); return 0; }
API long HRWHASHES(long h) { return netlb_hrw_hashes(POOL(h)); }
API long SETNOW(long h, long ms) { netlb_set_now(POOL(h), (double)ms); return 0; }
API long READY(long h) { return netlb_pool_ready(POOL(h)); }

API long PICK(long h, const char* key) {
    netlb_pool* p = POOL(h); if (!p) return -1;
    int has = key && key[0];
    return (long)netlb_pick(p, has ? key : NULL, has ? strlen(key) : 0);
}
API long PICKEXCL(long h, const char* key, const char* excl) {
    netlb_pool* p = POOL(h); if (!p) return -1;
    netlb_backend_id ex[64]; int n = 0;
    if (excl && excl[0]) { const char* s = excl; while (*s && n < 64) { ex[n++] = (netlb_backend_id)atoi(s); while (*s && *s != ',') s++; if (*s == ',') s++; } }
    int has = key && key[0];
    return (long)netlb_pick_ex(p, has ? key : NULL, has ? strlen(key) : 0, ex, n);
}

API long BEGINREQ(long h, long id) { netlb_begin(POOL(h), (netlb_backend_id)id); return 0; }
API long ENDREQ(long h, long id, long ok, long lat) { netlb_end(POOL(h), (netlb_backend_id)id, (int)ok, (double)lat); return 0; }
API long PROBE(long h, long id, long ok) { netlb_probe(POOL(h), (netlb_backend_id)id, (int)ok); return 0; }
API long HEALTHY(long h, long id) { return netlb_is_healthy(POOL(h), (netlb_backend_id)id); }
API long SHOULDRETRY(long h, long id, long att) { return netlb_should_retry(POOL(h), (netlb_backend_id)id, (int)att); }

API long INFLIGHT(long h, long id) { return netlb_backend_inflight(POOL(h), (netlb_backend_id)id); }
API long EWMA(long h, long id) { double e = netlb_backend_ewma(POOL(h), (netlb_backend_id)id); return e < 0 ? -1 : (long)e; }
API long LOAD(long h, long id) { return (long)netlb_backend_load(POOL(h), (netlb_backend_id)id); }
API long PICKCOUNT(long h, long id) { return (long)netlb_pick_count(POOL(h), (netlb_backend_id)id); }
API long IDLECOUNT(long h) { return netlb_idle_count(POOL(h)); }
API long STICKYCOUNT(long h) { return (long)netlb_sticky_count(POOL(h)); }
