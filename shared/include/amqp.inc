' FILE: amqp.inc — binding FFI AMQP 0-9-1 (namespace AMQP) per il pacchetto gmbsuite 'amqp'.
' Uso: REQUIRE "amqp" (oppure IMPORT AMQP). Firma OMOGENEA (tutti LONG), stringhe AS STRING.
NAMESPACE AMQP
PUBLIC DECLARE FUNCTION AVAILABLE LIB "gmbamqp" ALIAS "gmbamqp_available" () AS LONG
PUBLIC DECLARE FUNCTION CONNECT LIB "gmbamqp" ALIAS "gmbamqp_connect" (host AS STRING, port AS LONG, user AS STRING, pass AS STRING) AS LONG
PUBLIC DECLARE FUNCTION DECLAREQUEUE LIB "gmbamqp" ALIAS "gmbamqp_declare_queue" (h AS LONG, queue AS STRING) AS LONG
PUBLIC DECLARE FUNCTION PUBLISH LIB "gmbamqp" ALIAS "gmbamqp_publish" (h AS LONG, exchange AS STRING, routing_key AS STRING, body AS STRING) AS LONG
PUBLIC DECLARE FUNCTION GET LIB "gmbamqp" ALIAS "gmbamqp_get" (h AS LONG, queue AS STRING, timeout_ms AS LONG) AS LONG
PUBLIC DECLARE FUNCTION LASTBODY LIB "gmbamqp" ALIAS "gmbamqp_last_body" (h AS LONG) AS STRING
PUBLIC DECLARE FUNCTION LASTROUTINGKEY LIB "gmbamqp" ALIAS "gmbamqp_last_routingkey" (h AS LONG) AS STRING
PUBLIC DECLARE FUNCTION LASTSIZE LIB "gmbamqp" ALIAS "gmbamqp_last_size" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION DISCONNECT LIB "gmbamqp" ALIAS "gmbamqp_disconnect" (h AS LONG) AS LONG
