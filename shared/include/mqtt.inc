' FILE: mqtt.inc — binding FFI MQTT 3.1.1 (namespace MQTT) per il pacchetto gmbsuite 'mqtt'.
' Uso: REQUIRE "mqtt"   (oppure IMPORT MQTT)   ->  chiamate MQTT.<Membro>(...)
' Convenzione FFI GMBasic: firma OMOGENEA (tutti LONG), stringhe AS STRING. Ultimo messaggio
' ricevuto in stato interno (LASTTOPIC/LASTPAYLOAD).
NAMESPACE MQTT
PUBLIC DECLARE FUNCTION AVAILABLE LIB "gmbmqtt" ALIAS "gmbmqtt_available" () AS LONG
PUBLIC DECLARE FUNCTION CONNECT LIB "gmbmqtt" ALIAS "gmbmqtt_connect" (host AS STRING, port AS LONG, cid AS STRING) AS LONG
PUBLIC DECLARE FUNCTION PUBLISH LIB "gmbmqtt" ALIAS "gmbmqtt_publish" (h AS LONG, topic AS STRING, payload AS STRING) AS LONG
PUBLIC DECLARE FUNCTION SUBSCRIBE LIB "gmbmqtt" ALIAS "gmbmqtt_subscribe" (h AS LONG, topic AS STRING) AS LONG
PUBLIC DECLARE FUNCTION RECEIVE LIB "gmbmqtt" ALIAS "gmbmqtt_receive" (h AS LONG, timeout_ms AS LONG) AS LONG
PUBLIC DECLARE FUNCTION LASTTOPIC LIB "gmbmqtt" ALIAS "gmbmqtt_last_topic" (h AS LONG) AS STRING
PUBLIC DECLARE FUNCTION LASTPAYLOAD LIB "gmbmqtt" ALIAS "gmbmqtt_last_payload" (h AS LONG) AS STRING
PUBLIC DECLARE FUNCTION DISCONNECT LIB "gmbmqtt" ALIAS "gmbmqtt_disconnect" (h AS LONG) AS LONG
