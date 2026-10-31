' Minitest MESSAGE over TCP — conferma di consegna (broker esterno su :9215).
' Due CONNECT allo stesso broker (due connessioni): si PUBBLICA sul primo bus e si
' SOTTOSCRIVE sul secondo. La consegna cross-connessione esiste SOLO col broker: sul bus
' locale i messaggi restano dentro il singolo handle, quindi se il TCP non è disponibile il
' programma stampa "timeout" e il gate fallisce — niente falso verde da fallback locale.
' Conferme: il broker conferma la PUB al mittente (ACK <seq>); il client conferma ogni
' consegna (ACK) o la rifiuta (NACK) se nessun handler la prende in carico; il broker
' ri-consegna finché non arriva la conferma (retry-after).
' EXPECT: RX s2 t hello|delivered
IMPORT MESSAGE
DIM got AS LONG = 0
SUB OnMsg(id AS STRING, topic AS STRING, payload AS STRING)
  PRINT "RX "; id; " "; topic; " "; payload
  got = 1
END SUB
SUB Main()
  DIM pub AS LONG
  DIM sub AS LONG
  DIM tries AS LONG
  ' Ordine voluto: la connessione che si SOTTOSCRIVE è creata per prima. Il broker serve
  ' gli eventi degli slot in ordine di accettazione, quindi SUB precede sempre la PUB
  ' (su due connessioni diverse l'ordine non è garantito dal trasporto).
  MESSAGE CONNECT "tcp://127.0.0.1:9215" AS sub
  MESSAGE CONNECT "tcp://127.0.0.1:9215" AS pub
  MESSAGE SUBSCRIBE sub TOPIC("t") AS "s2"
  ON MESSAGE "s2" CALL OnMsg
  MESSAGE PUBLISH pub TOPIC("t") DATA("hello")
  WHILE got = 0 AND tries < 400
    SLEEP 5
    tries = tries + 1
  WEND
  IF got = 1 THEN PRINT "delivered" ELSE PRINT "timeout"
  MESSAGE CLOSE pub
  MESSAGE CLOSE sub
END SUB
