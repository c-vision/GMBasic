' FILE: grpc.inc — binding FFI gRPC (namespace GRPC) per il pacchetto gmbsuite 'grpc'.
' Uso: REQUIRE "grpc" (oppure IMPORT GRPC). Firma OMOGENEA (tutti LONG), stringhe AS STRING.
' Il payload del messaggio è scambiato in HEX (complementare al pacchetto `protobuf`).
NAMESPACE GRPC
PUBLIC DECLARE FUNCTION AVAILABLE LIB "gmbgrpc" ALIAS "gmbgrpc_available" () AS LONG
PUBLIC DECLARE FUNCTION INVOKE LIB "gmbgrpc" ALIAS "gmbgrpc_call" (host AS STRING, port AS LONG, path AS STRING, req_hex AS STRING) AS LONG
PUBLIC DECLARE FUNCTION LASTRESPONSE LIB "gmbgrpc" ALIAS "gmbgrpc_last_response" () AS STRING
PUBLIC DECLARE FUNCTION LASTSTATUS LIB "gmbgrpc" ALIAS "gmbgrpc_last_status" () AS LONG
