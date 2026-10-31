' EXPECT: available= 1
' Lib INTERNA gmbsuite/net/kafka: .inc auto-inclusa (IMPORT senza REQUIRE), dylib da <install>/libs.
IMPORT KAFKA WITH ALIAS
PRINT "available="; KAFKA.AVAILABLE()
