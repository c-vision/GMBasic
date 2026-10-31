' EXPECT: available= 1
' Lib INTERNA gmbsuite/net/grpc: .inc auto-inclusa (IMPORT senza REQUIRE), dylib da <install>/libs.
IMPORT GRPC WITH ALIAS
PRINT "available="; GRPC.AVAILABLE()
