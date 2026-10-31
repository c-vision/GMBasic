' FILE: nats.inc — binding FFI NATS (namespace NATS) per il pacchetto gmbsuite 'nats'.
' Uso: REQUIRE "nats" (oppure IMPORT NATS). Firma OMOGENEA (tutti LONG), stringhe AS STRING.
NAMESPACE NATS
PUBLIC DECLARE FUNCTION AVAILABLE LIB "gmbnats" ALIAS "gmbnats_available" () AS LONG
PUBLIC DECLARE FUNCTION CONNECT LIB "gmbnats" ALIAS "gmbnats_connect" (host AS STRING, port AS LONG, name AS STRING) AS LONG
PUBLIC DECLARE FUNCTION SUBSCRIBE LIB "gmbnats" ALIAS "gmbnats_subscribe" (h AS LONG, subject AS STRING) AS LONG
PUBLIC DECLARE FUNCTION PUBLISH LIB "gmbnats" ALIAS "gmbnats_publish" (h AS LONG, subject AS STRING, payload AS STRING) AS LONG
PUBLIC DECLARE FUNCTION RECEIVE LIB "gmbnats" ALIAS "gmbnats_receive" (h AS LONG, timeout_ms AS LONG) AS LONG
PUBLIC DECLARE FUNCTION LASTSUBJECT LIB "gmbnats" ALIAS "gmbnats_last_subject" (h AS LONG) AS STRING
PUBLIC DECLARE FUNCTION LASTPAYLOAD LIB "gmbnats" ALIAS "gmbnats_last_payload" (h AS LONG) AS STRING
PUBLIC DECLARE FUNCTION DISCONNECT LIB "gmbnats" ALIAS "gmbnats_disconnect" (h AS LONG) AS LONG
