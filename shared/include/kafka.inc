' FILE: kafka.inc — binding FFI Kafka (namespace KAFKA) per il pacchetto gmbsuite 'kafka'.
' Uso: REQUIRE "kafka" (oppure IMPORT KAFKA). Firma OMOGENEA (tutti LONG), stringhe AS STRING.
NAMESPACE KAFKA
PUBLIC DECLARE FUNCTION AVAILABLE LIB "gmbkafka" ALIAS "gmbkafka_available" () AS LONG
PUBLIC DECLARE FUNCTION CONNECT LIB "gmbkafka" ALIAS "gmbkafka_connect" (host AS STRING, port AS LONG, client_id AS STRING) AS LONG
PUBLIC DECLARE FUNCTION PRODUCE LIB "gmbkafka" ALIAS "gmbkafka_produce" (h AS LONG, topic AS STRING, partition AS LONG, value AS STRING) AS LONG
PUBLIC DECLARE FUNCTION CONSUME LIB "gmbkafka" ALIAS "gmbkafka_fetch" (h AS LONG, topic AS STRING, partition AS LONG, offset AS LONG, timeout_ms AS LONG) AS LONG
PUBLIC DECLARE FUNCTION LASTVALUE LIB "gmbkafka" ALIAS "gmbkafka_last_value" (h AS LONG) AS STRING
PUBLIC DECLARE FUNCTION NEXTOFFSET LIB "gmbkafka" ALIAS "gmbkafka_next_offset" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION DISCONNECT LIB "gmbkafka" ALIAS "gmbkafka_disconnect" (h AS LONG) AS LONG
