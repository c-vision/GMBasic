' Minitest MESSAGE topic filtering — due sottoscrizioni, ogni PUBLISH va solo al topic giusto.
IMPORT MESSAGE
SUB H(id AS STRING, topic AS STRING, payload AS STRING)
  PRINT "RX "; id; " "; topic
END SUB
SUB Main()
  DIM b AS LONG
  MESSAGE CONNECT "local" AS b
  MESSAGE SUBSCRIBE b TOPIC("x") AS "sx"
  MESSAGE SUBSCRIBE b TOPIC("y") AS "sy"
  ON MESSAGE "sx" CALL H
  ON MESSAGE "sy" CALL H
  MESSAGE PUBLISH b TOPIC("x") DATA("1")
  SLEEP 150
  MESSAGE PUBLISH b TOPIC("y") DATA("2")
  SLEEP 150
  PRINT "DONE"
END SUB
