' FILE: protobuf.inc — binding FFI Protocol Buffers (namespace PROTOBUF) per il pacchetto gmbsuite 'protobuf'.
' Uso: REQUIRE "protobuf" (oppure IMPORT PROTOBUF). Firma OMOGENEA (tutti LONG), stringhe AS STRING.
' I byte serializzati sono HEX (SERIALIZE/PARSE): nessun problema di NUL.
NAMESPACE PROTOBUF
PUBLIC DECLARE FUNCTION AVAILABLE LIB "gmbprotobuf" ALIAS "gmbprotobuf_available" () AS LONG
PUBLIC DECLARE FUNCTION NEWMSG LIB "gmbprotobuf" ALIAS "gmbprotobuf_new" () AS LONG
PUBLIC DECLARE FUNCTION ADDINT LIB "gmbprotobuf" ALIAS "gmbprotobuf_addint" (h AS LONG, field AS LONG, value AS LONG) AS LONG
PUBLIC DECLARE FUNCTION ADDSTRING LIB "gmbprotobuf" ALIAS "gmbprotobuf_addstring" (h AS LONG, field AS LONG, s AS STRING) AS LONG
PUBLIC DECLARE FUNCTION SERIALIZE LIB "gmbprotobuf" ALIAS "gmbprotobuf_serialize" (h AS LONG) AS STRING
PUBLIC DECLARE FUNCTION PARSEHEX LIB "gmbprotobuf" ALIAS "gmbprotobuf_parse" (hex AS STRING) AS LONG
PUBLIC DECLARE FUNCTION HASFIELD LIB "gmbprotobuf" ALIAS "gmbprotobuf_hasfield" (h AS LONG, field AS LONG) AS LONG
PUBLIC DECLARE FUNCTION GETINT LIB "gmbprotobuf" ALIAS "gmbprotobuf_getint" (h AS LONG, field AS LONG) AS LONG
PUBLIC DECLARE FUNCTION GETSTRING LIB "gmbprotobuf" ALIAS "gmbprotobuf_getstring" (h AS LONG, field AS LONG) AS STRING
PUBLIC DECLARE FUNCTION FREEMSG LIB "gmbprotobuf" ALIAS "gmbprotobuf_free" (h AS LONG) AS LONG
