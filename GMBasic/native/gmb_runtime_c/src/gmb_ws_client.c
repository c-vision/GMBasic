/* ============================================================================
 * File: native/gmb_runtime_c/src/gmb_ws_client.c
 * Descrizione: receive-loop LATO CLIENT dei WebSocket (parità VM/C).
 *
 * Quando il BASIC fa `ws.OnMessage = "Sub"` (o `ws.OnClose = ...`) su una variabile
 * WEBSOCKET creata da `WEBSOCKET CONNECT`, il C generato chiama `gmb_ws_client_start(ws)`.
 * La funzione:
 *   1. legge dal dict l'handle della socket e i fn-ptr dei wrapper
 *      (__onmsg_fn/__onclose_fn, emessi dal transpiler C99);
 *   2. avvia un thread dedicato che riceve i frame (gmb_ws_recv_frame, timeout 200ms);
 *   3. per ogni frame TEXT/BINARY spawna un TASK GPM (gmb_task_spawn) che invoca la SUB
 *      dell'utente tramite il wrapper; su CLOSE spawna OnClose.
 *
 * REGOLA GPM: il thread di ricezione NON esegue BASIC — accoda un task GPM e ritorna.
 * È lo stesso schema del receive-loop del server (HTTP ROUTE upgrade).
 *
 * Solo le socket LATO CLIENT hanno `__ws_client` nel dict: quelle lato server hanno già
 * il receive-loop del route wrapper e un secondo lettore ruberebbe i frame.
 * Il flag `__recv_on` impedisce due ricevitori sulla stessa socket.
 * ========================================================================== */
#include "gmb_runtime.h"
#include "gmb_network.h"
#include "gmb_c99gpm.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Layout IDENTICO a `gmb_ws_cb_args` emesso dal transpiler C99:
 *   typedef struct { gmb_val_t ws; char* msg; } gmb_ws_cb_args;
 * Il wrapper dell'utente fa il cast a quel tipo: mantenere l'ordine dei campi. */
typedef struct {
    gmb_val_t ws;
    char* msg;
} gmb_ws_client_args_t;

typedef struct {
    gmb_val_t ws;
    int64_t handle;
    void (*onmsg)(void*);
    void (*onclose)(void*);
} gmb_ws_client_t;

static void ws_client_spawn(void (*fn)(void*), gmb_val_t ws, char* msg) {
    if (!fn) {
        if (msg) free(msg);
        return;
    }
    gmb_ws_client_args_t* a = (gmb_ws_client_args_t*)calloc(1, sizeof(*a));
    if (!a) {
        if (msg) free(msg);
        return;
    }
    a->ws = ws;
    a->msg = msg;
    gmb_task_spawn(fn, a);
}

static void* ws_client_loop(void* p) {
    gmb_ws_client_t* c = (gmb_ws_client_t*)p;
    for (;;) {
        int op = 0;
        char* payload = NULL;
        size_t plen = 0;
        if (!gmb_ws_is_open(c->handle)) break;
        if (gmb_ws_recv_frame(c->handle, &op, &payload, &plen, 200) != 0) {
            /* timeout o errore transitorio: si continua finché l'handle è vivo */
            continue;
        }
        if (op == GMB_WS_CLOSE) {
            if (payload) free(payload);
            if (c->onclose) ws_client_spawn(c->onclose, c->ws, NULL);
            break;
        }
        if (op == GMB_WS_TEXT || op == GMB_WS_BINARY) {
            ws_client_spawn(c->onmsg, c->ws, payload); /* payload passa al task */
        } else if (payload) {
            free(payload);
        }
    }
    free(c);
    return NULL;
}

void gmb_ws_client_start(gmb_val_t ws) {
    gmb_dict_t d = gmb_val_as_dict(ws);
    if (!d.items) return;
    if (gmb_val_to_int(gmb_dict_get(d, "__ws_client")) == 0) return;
    int64_t handle = (int64_t)gmb_val_to_int(gmb_dict_get(d, "__ws_handle"));
    if (handle <= 0) return;

    void (*onmsg)(void*) = (void (*)(void*))(uintptr_t)gmb_val_to_int(gmb_dict_get(d, "__onmsg_fn"));
    void (*onclose)(void*) = (void (*)(void*))(uintptr_t)gmb_val_to_int(gmb_dict_get(d, "__onclose_fn"));
    /* Nessun handler (es. CONNECT prima di OnMessage): non si avvia nulla. La chiamata
     * successiva, dopo l'assegnazione di OnMessage, avvierà il loop. */
    if (!onmsg && !onclose) return;
    if (gmb_val_to_int(gmb_dict_get(d, "__recv_on")) != 0) return;
    gmb_val_dict_set(ws, "__recv_on", gmb_make_val_int(1));

    gmb_ws_client_t* c = (gmb_ws_client_t*)calloc(1, sizeof(*c));
    if (!c) return;
    c->ws = ws;
    c->handle = handle;
    c->onmsg = onmsg;
    c->onclose = onclose;

    gmb_thread_t th;
    if (GMB_THREAD_CREATE(&th, ws_client_loop, c)) {
        GMB_THREAD_DETACH(&th);
    } else {
        free(c);
    }
}
