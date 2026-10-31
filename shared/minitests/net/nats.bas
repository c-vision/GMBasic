' EXPECT: available= 1
' Lib INTERNA gmbsuite/net/nats: .inc auto-inclusa (IMPORT senza REQUIRE), dylib da <install>/libs.
IMPORT NATS WITH ALIAS
PRINT "available="; NATS.AVAILABLE()
