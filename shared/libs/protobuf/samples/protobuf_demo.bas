' protobuf_demo.bas — pacchetto `protobuf`: codec wire-format Protocol Buffers (C puro, self-contained).
' Nessun server: costruisce un messaggio, lo serializza (hex) e lo rilegge.
REQUIRE "protobuf"
IMPORT PROTOBUF WITH ALIAS

SUB Main()
  w = PROTOBUF.NEWMSG()
  PROTOBUF.ADDINT(w, 1, 150)
  PROTOBUF.ADDSTRING(w, 2, "testing")
  PROTOBUF.ADDINT(w, 3, 42)
  hx$ = PROTOBUF.SERIALIZE(w)
  PRINT "serialized (hex) = " + hx$

  r = PROTOBUF.PARSEHEX(hx$)
  PRINT "field1 (int)    = " + STR(PROTOBUF.GETINT(r, 1))
  PRINT "field2 (string) = " + PROTOBUF.GETSTRING(r, 2)
  PRINT "field3 (int)    = " + STR(PROTOBUF.GETINT(r, 3))
  PRINT "ha field4?      = " + STR(PROTOBUF.HASFIELD(r, 4))
  PROTOBUF.FREEMSG(w) : PROTOBUF.FREEMSG(r)
END SUB
