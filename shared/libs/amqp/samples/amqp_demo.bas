' amqp_demo.bas — pacchetto `amqp`: client AMQP 0-9-1 (RabbitMQ) in C puro (self-contained).
' Richiede un broker AMQP su localhost:5672 (es. `docker run -p 5672:5672 rabbitmq:3`).
REQUIRE "amqp"
IMPORT AMQP WITH ALIAS

SUB Main()
  h = AMQP.CONNECT("127.0.0.1", 5672, "guest", "guest")
  IF h <= 0 THEN PRINT "broker AMQP non disponibile su 127.0.0.1:5672" : EXIT SUB
  AMQP.DECLAREQUEUE(h, "gmb.demo")
  AMQP.PUBLISH(h, "", "gmb.demo", "ciao-dal-pacchetto-amqp")
  IF AMQP.GET(h, "gmb.demo", 3000) = 1 THEN
    PRINT "routing_key = " + AMQP.LASTROUTINGKEY(h)
    PRINT "body        = " + AMQP.LASTBODY(h)
    PRINT "size        = " + STR(AMQP.LASTSIZE(h))
  ELSE
    PRINT "nessun messaggio"
  END IF
  AMQP.DISCONNECT(h)
END SUB
