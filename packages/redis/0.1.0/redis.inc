' SPDX-License-Identifier: LicenseRef-PUL-1.1
' Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
' Licenza: Personal Use License (PUL) v1.1.
'
' FILE: redis.inc — binding FFI Redis (namespace REDIS) per il pacchetto gmbsuite 'redis'.
' Uso: REQUIRE "redis"  +  IMPORT REDIS WITH ALIAS   ->   REDIS.<Membro>(...)
' Convenzione FFI: firma omogenea a 8 byte (handle = LONG/puntatore, stringhe AS STRING, interi = LONG).
' Il simbolo nativo ha lo STESSO NOME del membro: nessun ALIAS, nessun prefisso.
' Handle > 0 = connessione valida, 0 = errore. La lib e' un client RESP/2 self-contained (solo socket).

NAMESPACE REDIS

' 1 = lib presente.
PUBLIC DECLARE FUNCTION AVAILABLE LIB "redis" () AS LONG
' Apre una connessione TCP a host:port. Ritorna handle (>0) o 0 in errore.
PUBLIC DECLARE FUNCTION CONNECT LIB "redis" (host AS STRING, port AS LONG) AS LONG
' Chiude la connessione. 0 ok.
PUBLIC DECLARE FUNCTION CLOSE LIB "redis" (h AS LONG) AS LONG
' 1 se la connessione e' viva.
PUBLIC DECLARE FUNCTION ALIVE LIB "redis" (h AS LONG) AS LONG
' Ultimo messaggio d'errore ("" se nessuno).
PUBLIC DECLARE FUNCTION ERR LIB "redis" (h AS LONG) AS STRING

' Autenticazione / selezione DB / ping.
PUBLIC DECLARE FUNCTION AUTH LIB "redis" (h AS LONG, password AS STRING) AS LONG
PUBLIC DECLARE FUNCTION SELECT LIB "redis" (h AS LONG, db AS LONG) AS LONG
PUBLIC DECLARE FUNCTION PING LIB "redis" (h AS LONG) AS LONG

' Stringhe.
PUBLIC DECLARE FUNCTION SET LIB "redis" (h AS LONG, key AS STRING, val AS STRING) AS LONG
PUBLIC DECLARE FUNCTION SETEX LIB "redis" (h AS LONG, key AS STRING, secs AS LONG, val AS STRING) AS LONG
PUBLIC DECLARE FUNCTION GET LIB "redis" (h AS LONG, key AS STRING) AS STRING
PUBLIC DECLARE FUNCTION DEL LIB "redis" (h AS LONG, key AS STRING) AS LONG
PUBLIC DECLARE FUNCTION EXISTS LIB "redis" (h AS LONG, key AS STRING) AS LONG
PUBLIC DECLARE FUNCTION INCR LIB "redis" (h AS LONG, key AS STRING, by AS LONG) AS LONG
PUBLIC DECLARE FUNCTION EXPIRE LIB "redis" (h AS LONG, key AS STRING, secs AS LONG) AS LONG
PUBLIC DECLARE FUNCTION TTL LIB "redis" (h AS LONG, key AS STRING) AS LONG
PUBLIC DECLARE FUNCTION STRLEN LIB "redis" (h AS LONG, key AS STRING) AS LONG

' Hash.
PUBLIC DECLARE FUNCTION HSET LIB "redis" (h AS LONG, key AS STRING, field AS STRING, val AS STRING) AS LONG
PUBLIC DECLARE FUNCTION HGET LIB "redis" (h AS LONG, key AS STRING, field AS STRING) AS STRING
PUBLIC DECLARE FUNCTION HDEL LIB "redis" (h AS LONG, key AS STRING, field AS STRING) AS LONG
PUBLIC DECLARE FUNCTION HEXISTS LIB "redis" (h AS LONG, key AS STRING, field AS STRING) AS LONG
PUBLIC DECLARE FUNCTION HLEN LIB "redis" (h AS LONG, key AS STRING) AS LONG

' Liste.
PUBLIC DECLARE FUNCTION LPUSH LIB "redis" (h AS LONG, key AS STRING, val AS STRING) AS LONG
PUBLIC DECLARE FUNCTION LLEN LIB "redis" (h AS LONG, key AS STRING) AS LONG
PUBLIC DECLARE FUNCTION LPOP LIB "redis" (h AS LONG, key AS STRING) AS STRING

' Set.
PUBLIC DECLARE FUNCTION SADD LIB "redis" (h AS LONG, key AS STRING, member AS STRING) AS LONG
PUBLIC DECLARE FUNCTION SISMEMBER LIB "redis" (h AS LONG, key AS STRING, member AS STRING) AS LONG
PUBLIC DECLARE FUNCTION SCARD LIB "redis" (h AS LONG, key AS STRING) AS LONG

' Server / pub-sub generico.
PUBLIC DECLARE FUNCTION PUBLISH LIB "redis" (h AS LONG, channel AS STRING, msg AS STRING) AS LONG
PUBLIC DECLARE FUNCTION DBSIZE LIB "redis" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION FLUSHDB LIB "redis" (h AS LONG) AS LONG

' Comando generico: cmdline = "CMD arg1 arg2" (split su spazio). 1 ok, -1 errore.
PUBLIC DECLARE FUNCTION DO LIB "redis" (h AS LONG, cmdline AS STRING) AS LONG
' Introspezione dell'ultima risposta: tipo 0=none 1=status 2=error 3=int 4=bulk 5=nil 6=array.
PUBLIC DECLARE FUNCTION REPLYTYPE LIB "redis" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION REPLYSTR LIB "redis" (h AS LONG) AS STRING
PUBLIC DECLARE FUNCTION REPLYINT LIB "redis" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION REPLYCOUNT LIB "redis" (h AS LONG) AS LONG
