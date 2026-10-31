' nats_demo.bas — pacchetto `nats`: client NATS core (C puro, self-contained).
' Richiede un server NATS su localhost:4222 (es. `docker run -p 4222:4222 nats:latest`).
REQUIRE "nats"
IMPORT NATS WITH ALIAS

SUB Main()
  h = NATS.CONNECT("127.0.0.1", 4222, "gmb-demo")
  IF h <= 0 THEN PRINT "server NATS non disponibile su 127.0.0.1:4222" : EXIT SUB
  NATS.SUBSCRIBE(h, "gmb.demo")
  NATS.PUBLISH(h, "gmb.demo", "ciao-dal-pacchetto-nats")
  IF NATS.RECEIVE(h, 3000) = 1 THEN
    PRINT "subject=" + NATS.LASTSUBJECT(h)
    PRINT "payload=" + NATS.LASTPAYLOAD(h)
  ELSE
    PRINT "nessun messaggio"
  END IF
  NATS.DISCONNECT(h)
END SUB
