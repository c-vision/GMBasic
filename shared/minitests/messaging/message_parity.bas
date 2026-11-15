' Minitest MESSAGE (namespace MESSAGE) — parità 4 backend.
' Event-driven: SUBSCRIBE apre il bus implicito, ON MESSAGE registra l'handler (task GPM),
' PUBLISH consegna (id, topic, payload). Output identico sui 4 backend.
' EXPECT: MSG=s1 T=t1 D=hello
IMPORT MESSAGE
SUB OnMsg(id AS STRING, topic AS STRING, payload AS STRING)
  PRINT "MSG="; id; " T="; topic; " D="; payload
END SUB
SUB Main()
  DIM bus AS LONG
  MESSAGE CONNECT "local" AS bus
  MESSAGE SUBSCRIBE bus TOPIC("t1") AS "s1"
  ON MESSAGE "s1" CALL OnMsg
  MESSAGE PUBLISH bus TOPIC("t1") DATA("hello")
  SLEEP 200
END SUB
