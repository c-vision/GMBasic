' kafka_demo.bas — pacchetto `kafka`: client Kafka (wire protocol) in C puro (self-contained).
' Richiede un broker Kafka su localhost:9092 (es. `docker run -p 9092:9092 apache/kafka:latest`).
REQUIRE "kafka"
IMPORT KAFKA WITH ALIAS

SUB Main()
  h = KAFKA.CONNECT("127.0.0.1", 9092, "gmb-demo")
  IF h <= 0 THEN PRINT "broker Kafka non disponibile su 127.0.0.1:9092" : EXIT SUB
  PRINT "produce = " + STR(KAFKA.PRODUCE(h, "gmb.demo", 0, "ciao-dal-pacchetto-kafka"))
  IF KAFKA.CONSUME(h, "gmb.demo", 0, 0, 3000) = 1 THEN
    PRINT "value   = " + KAFKA.LASTVALUE(h)
    PRINT "next    = " + STR(KAFKA.NEXTOFFSET(h))
  ELSE
    PRINT "nessun record"
  END IF
  KAFKA.DISCONNECT(h)
END SUB
