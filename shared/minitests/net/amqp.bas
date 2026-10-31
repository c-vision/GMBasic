' EXPECT: available= 1
' Lib INTERNA gmbsuite/net/amqp: .inc auto-inclusa (IMPORT senza REQUIRE), dylib da <install>/libs.
IMPORT AMQP WITH ALIAS
PRINT "available="; AMQP.AVAILABLE()
