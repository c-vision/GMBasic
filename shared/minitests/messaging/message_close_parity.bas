' Minitest MESSAGE CLOSE — dopo la chiusura il bus non consegna più (parità 4 backend).
' Trasporto REALE: due connessioni allo stesso broker (:9215), si PUBBLICA su una e si
' SOTTOSCRIVE sull'altra, così la consegna passa dal broker e non dal bus locale. Senza
' broker la prima consegna non arriva e il gate fallisce (nessun verde da fallback locale).
' Canarino: il contatore degli arrivi. Se CLOSE non chiude il bus (handle sbagliato), la
' seconda PUB viene ancora consegnata: esce "leaked" (e la riga RX in più) → EXPECT non torna.
' (Parole, non numeri: PRINT antepone uno spazio al segno dei numeri.)
' EXPECT: RX s1 t before|closed
IMPORT MESSAGE
DIM got AS LONG = 0

SUB OnMsg(id AS STRING, topic AS STRING, payload AS STRING)
  PRINT "RX "; id; " "; topic; " "; payload
  got = got + 1
END SUB

SUB Main()
  DIM sub AS LONG
  DIM pub AS LONG
  DIM tries AS LONG
  ' SUB-first: il broker serve gli slot in ordine di accettazione (vedi message_tcp_parity).
  MESSAGE CONNECT "tcp://127.0.0.1:9215" AS sub
  MESSAGE CONNECT "tcp://127.0.0.1:9215" AS pub
  MESSAGE SUBSCRIBE sub TOPIC("t") AS "s1"
  ON MESSAGE "s1" CALL OnMsg
  MESSAGE PUBLISH pub TOPIC("t") DATA("before")
  WHILE got = 0 AND tries < 400
    SLEEP 5
    tries = tries + 1
  WEND
  MESSAGE CLOSE sub
  ' Margine perché il broker elabori la CLOSE (tick 20ms) prima della PUB successiva:
  ' altrimenti la PUB potrebbe essere smistata alla connessione ancora aperta.
  SLEEP 150
  MESSAGE PUBLISH pub TOPIC("t") DATA("after")
  tries = 0
  WHILE got = 1 AND tries < 200
    SLEEP 5
    tries = tries + 1
  WEND
  IF got = 1 THEN PRINT "closed" ELSE PRINT "leaked"
  MESSAGE CLOSE pub
END SUB
