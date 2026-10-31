' grpc_demo.bas — pacchetto `grpc`: client gRPC unario in C puro (self-contained).
' Richiede un server gRPC h2c (es. `docker run -p 9000:9000 moul/grpcbin`). Chiama
' GRPCBin.DummyUnary con un DummyMessage (field 1 = stringa) e ne legge l'eco.
' I messaggi sono in HEX (complementare al pacchetto `protobuf`).
REQUIRE "grpc"
IMPORT GRPC WITH ALIAS

SUB Main()
  ' field 1 (string) "ciao" = tag 0x0A, len 0x04, "ciao" -> 0a046369616f
  st = GRPC.INVOKE("127.0.0.1", 9000, "/grpcbin.GRPCBin/DummyUnary", "0a046369616f")
  PRINT "grpc-status = " + STR(st)
  PRINT "response    = " + GRPC.LASTRESPONSE()
END SUB
