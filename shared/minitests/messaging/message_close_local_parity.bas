' Minitest MESSAGE CLOSE sul bus LOCALE — dopo la chiusura non consegna più (parità 4 backend).
' Bus in-process, nessun broker: si sottoscrive, si consegna una volta, si chiude il bus e si
' ripubblica. Il canarino è il contatore degli arrivi: se CLOSE libera lo slot del bus ma non
' le sue sottoscrizioni, la seconda PUB viene ancora consegnata → "leaked" (+ riga RX in più).
' (Parole e non numeri: PRINT antepone uno spazio al segno dei numeri.)
' EXPECT: RX s t a|closed
IMPORT MESSAGE
DIM got AS LONG = 0

SUB H(id AS STRING, topic AS STRING, payload AS STRING)
  PRINT "RX "; id; " "; topic; " "; payload
  got = got + 1
END SUB

SUB Main()
  DIM b AS LONG
  MESSAGE CONNECT "local" AS b
  MESSAGE SUBSCRIBE b TOPIC("t") AS "s"
  ON MESSAGE "s" CALL H
  MESSAGE PUBLISH b TOPIC("t") DATA("a")
  SLEEP 50
  MESSAGE CLOSE b
  MESSAGE PUBLISH b TOPIC("t") DATA("b")
  SLEEP 150
  IF got = 1 THEN PRINT "closed" ELSE PRINT "leaked"
END SUB
