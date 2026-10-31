' PubSubDemo.bas
' ---------------------------------------------------------------------------
' MESSAGE (namespace MESSAGE) — messaging generico, event-driven, bus in-process.
' Usa TUTTI i comandi: CONNECT, SUBSCRIBE (TOPIC), ON MESSAGE, PUBLISH,
' UNSUBSCRIBE, PING, CLOSE. L'handler gira come task GPM con (id, topic, payload);
' nessun loop utente. Deterministico (output identico sui 4 backend).
'
' Endpoint: "local" = bus in-process. Per remoto usare un URI (tcp://, amqp://, ibmmq://, ...).
' ---------------------------------------------------------------------------
IMPORT MESSAGE

SUB OnOrders(id AS STRING, topic AS STRING, payload AS STRING)
  PRINT "[orders:"; id; "] "; payload
END SUB

SUB OnTrades(id AS STRING, topic AS STRING, payload AS STRING)
  PRINT "[trades:"; id; "] "; payload
END SUB

SUB Main()
  DIM bus AS LONG
  MESSAGE CONNECT "local" AS bus

  MESSAGE SUBSCRIBE bus TOPIC("orders") AS "o"
  MESSAGE SUBSCRIBE bus TOPIC("trades") AS "t"
  ON MESSAGE "o" CALL OnOrders
  ON MESSAGE "t" CALL OnTrades

  MESSAGE PUBLISH bus TOPIC("orders") DATA("buy 100 AAPL")
  SLEEP 120
  MESSAGE PUBLISH bus TOPIC("trades") DATA("fill 100 AAPL @ 200")
  SLEEP 120

  MESSAGE PING bus

  ' dopo UNSUBSCRIBE il topic "trades" non consegna più
  MESSAGE UNSUBSCRIBE "t"
  MESSAGE PUBLISH bus TOPIC("trades") DATA("ignored")
  SLEEP 120

  MESSAGE PUBLISH bus TOPIC("orders") DATA("sell 50 AAPL")
  SLEEP 120

  MESSAGE CLOSE bus
  PRINT "done"
END SUB
