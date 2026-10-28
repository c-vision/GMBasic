' SPDX-License-Identifier: LicenseRef-PUL-1.1
' Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
' Licenza: Personal Use License (PUL) v1.1.
'
' FILE: netlb.inc — binding FFI load balancing (namespace LB) per il pacchetto gmbsuite 'netlb'.
' Uso: REQUIRE "netlb"   (oppure IMPORT NETLB WITH ALIAS)   ->  chiamate LB.<Membro>(...)
'
' Convenzione FFI: firma OMOGENEA all-LONG (handle = LONG/puntatore, stringhe AS STRING).
' Scalari scalati a interi: weight_milli 5000 = 5.0 ; lat_ms/now_ms in millisecondi ; ratio_milli 200 = 0.2.
' Nomi membri SENZA underscore; il simbolo C (con underscore) va in ALIAS.
' La lib e' caricata a runtime dalla cache pacchetti (~/.gmb/packages/netlb/<ver>/lib/<platform>/).

' Politiche (per POOLNEW / SETPOLICY). Costanti GLOBALI: le `PUBLIC CONST` di un namespace
' non risolvono il valore (verificato) -> si usano globali prefisse `NETLB_`.
CONST NETLB_RR = 0
CONST NETLB_WRR = 1
CONST NETLB_RANDOM = 2
CONST NETLB_LC = 3
CONST NETLB_WLC = 4
CONST NETLB_LRT = 5
CONST NETLB_P2C = 6
CONST NETLB_CHASH = 7
CONST NETLB_HRW = 8
CONST NETLB_MAGLEV = 9
CONST NETLB_JIQ = 10

NAMESPACE NETLB

' 1 = lib presente, 0 = no-op.
PUBLIC DECLARE FUNCTION AVAILABLE LIB "netlb" () AS LONG
' Crea un pool. policy = LB.<...>; seed 0 = default. Ritorna handle LONG (>0), 0 = errore.
PUBLIC DECLARE FUNCTION POOLNEW LIB "netlb" (policy AS LONG, seed AS LONG) AS LONG
' Aggiunge un backend (name, weight_milli, cap; cap 0 = illimitata). Ritorna id (>=0) o -1.
PUBLIC DECLARE FUNCTION BACKENDADD LIB "netlb" (h AS LONG, name AS STRING, weight_milli AS LONG, cap AS LONG) AS LONG
' Rimuove un backend (draining: nessun nuovo pick). 0 ok.
PUBLIC DECLARE FUNCTION BACKENDREMOVE LIB "netlb" (h AS LONG, id AS LONG) AS LONG
' Numero di backend nel pool.
PUBLIC DECLARE FUNCTION BACKENDCOUNT LIB "netlb" (h AS LONG) AS LONG
' 1 se almeno un backend sano.
PUBLIC DECLARE FUNCTION READY LIB "netlb" (h AS LONG) AS LONG
' Cambia policy del pool.
PUBLIC DECLARE FUNCTION SETPOLICY LIB "netlb" (h AS LONG, policy AS LONG) AS LONG
PUBLIC DECLARE FUNCTION SETWEIGHT LIB "netlb" (h AS LONG, id AS LONG, weight_milli AS LONG) AS LONG
' Limite pending (circuit breaking): max richieste in corso sul backend (0 = illimitato).
PUBLIC DECLARE FUNCTION SETCAP LIB "netlb" (h AS LONG, id AS LONG, cap AS LONG) AS LONG
' Slow-start (ramp del peso su window_ms). 0 = off.
PUBLIC DECLARE FUNCTION SLOWSTART LIB "netlb" (h AS LONG, window_ms AS LONG) AS LONG
' Budget di retry (ratio_milli; default 200 = 20%).
PUBLIC DECLARE FUNCTION RETRYBUDGET LIB "netlb" (h AS LONG, ratio_milli AS LONG) AS LONG
' Orologio host (ms monotoni) — serve SOLO allo slow-start.
PUBLIC DECLARE FUNCTION SETNOW LIB "netlb" (h AS LONG, now_ms AS LONG) AS LONG
' (Ri)costruisce la tabella Maglev (m primo consigliato, es. 4099). 0 ok.
PUBLIC DECLARE FUNCTION MAGLEVBUILD LIB "netlb" (h AS LONG, m AS LONG) AS LONG
' Sticky soft: affinita' cookie->backend con fallback (ttl_ms; 0 = off).
PUBLIC DECLARE FUNCTION SETSTICKY LIB "netlb" (h AS LONG, ttl_ms AS LONG) AS LONG
' HRW gerarchico (skeleton) O(log n): m = cluster, f = fanout (0 = HRW piatto). Per N molto grande.
PUBLIC DECLARE FUNCTION SETHRWSKEL LIB "netlb" (h AS LONG, m AS LONG, f AS LONG) AS LONG
' Numero di hash per l'ultimo pick HRW (introspezione: O(log n)).
PUBLIC DECLARE FUNCTION HRWHASHES LIB "netlb" (h AS LONG) AS LONG
' Scegle un backend. key "" = policy dinamica; key non vuota = policy d'affinita'. Ritorna id o -1.
PUBLIC DECLARE FUNCTION PICK LIB "netlb" (h AS LONG, key AS STRING) AS LONG
' Come PICK ma esclude gli id gia' tentati (retry), in excl = "1,2,3" (o "").
PUBLIC DECLARE FUNCTION PICKEXCL LIB "netlb" (h AS LONG, key AS STRING, excl AS STRING) AS LONG
' Eventi: richiesta entrata / uscita (ok 1/0, lat in ms) / probe di salute.
PUBLIC DECLARE FUNCTION BEGINREQ LIB "netlb" (h AS LONG, id AS LONG) AS LONG
PUBLIC DECLARE FUNCTION ENDREQ LIB "netlb" (h AS LONG, id AS LONG, ok AS LONG, lat_ms AS LONG) AS LONG
PUBLIC DECLARE FUNCTION PROBE LIB "netlb" (h AS LONG, id AS LONG, ok AS LONG) AS LONG
' 1 se il backend e' sano.
PUBLIC DECLARE FUNCTION HEALTHY LIB "netlb" (h AS LONG, id AS LONG) AS LONG
' 1 se conviene ritentare (budget + jitter) per il tentativo n.
PUBLIC DECLARE FUNCTION SHOULDRETRY LIB "netlb" (h AS LONG, id AS LONG, attempt AS LONG) AS LONG
' Metriche: connessioni in corso / latenza EWMA (ms) / attesa stimata / n. pick.
PUBLIC DECLARE FUNCTION INFLIGHT LIB "netlb" (h AS LONG, id AS LONG) AS LONG
PUBLIC DECLARE FUNCTION EWMA LIB "netlb" (h AS LONG, id AS LONG) AS LONG
PUBLIC DECLARE FUNCTION LOAD LIB "netlb" (h AS LONG, id AS LONG) AS LONG
PUBLIC DECLARE FUNCTION PICKCOUNT LIB "netlb" (h AS LONG, id AS LONG) AS LONG
' Backend attualmente idle (coda JIQ) / voci sticky soft correnti.
PUBLIC DECLARE FUNCTION IDLECOUNT LIB "netlb" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION STICKYCOUNT LIB "netlb" (h AS LONG) AS LONG
' Libera il pool.
PUBLIC DECLARE FUNCTION RELEASE LIB "netlb" (h AS LONG) AS LONG
