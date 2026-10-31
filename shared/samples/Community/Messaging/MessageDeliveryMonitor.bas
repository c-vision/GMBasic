' MessageDeliveryMonitor.bas
' ---------------------------------------------------------------------------
' MESSAGE.MESSAGE DELIVERY — contatori di consegna del bus (MESSAGE.DELIVERY).
'
' MESSAGE.DELIVERY(bus) è una FUNZIONE (parentesi obbligatorie, §1/§2): LEGGE e non scrive.
' Ritorna una riga "sent=S recv=R ack=A nack=N dup=D last=L":
'   sent  PUB scritte verso il broker (solo trasporto tcp: il bus locale non conferma)
'   recv  messaggi ricevuti dal bus
'   ack   conferme di consegna inviate (almeno un handler ha preso in carico il messaggio)
'   nack  rifiuti inviati (messaggio orfano): il broker lo ri-consegna
'   dup   messaggi con id già visto, cioè le RI-CONSEGNE del broker
'   last  massimo id visto (0 se nessuno)
'
' Il campione usa il bus LOCALE ("local"): la consegna è in-process e sincrona, quindi NON
' ci sono messaggi da confermare e TUTTI i contatori restano a zero — è il caso "in attesa di
' broker", e serve a mostrare la forma della riga. Con un bus tcp:// i contatori riflettono il
' dialogo col broker (vedi minitest messaging/message_delivery_parity.bas, dove un messaggio
' orfano produce nack>0 e dup>0: la ri-consegna del broker).
' Deterministico e senza I/O di rete: output identico sui 4 backend.
' ---------------------------------------------------------------------------
IMPORT MESSAGE

DIM deliveries AS LONG = 0

SUB OnPrices(id AS STRING, topic AS STRING, payload AS STRING)
  deliveries = deliveries + 1
  PRINT "RX "; payload; " ("; deliveries; ")"
END SUB

SUB Main()
  DIM bus AS LONG
  DIM before AS STRING
  DIM after AS STRING

  MESSAGE CONNECT "local" AS bus
  MESSAGE SUBSCRIBE bus TOPIC("prices") AS "p"

  ' Prima di registrare l'handler: nessuna consegna ancora avvenuta.
  before = MESSAGE.DELIVERY(bus)
  PRINT "before="; before

  ON MESSAGE "p" CALL OnPrices
  MESSAGE PUBLISH bus TOPIC("prices") DATA("AAPL 200.10")
  SLEEP 120
  MESSAGE PUBLISH bus TOPIC("prices") DATA("MSFT 410.55")
  SLEEP 120

  after = MESSAGE.DELIVERY(bus)
  PRINT "after="; after
  ' Sul bus locale la consegna è sincrona: niente conferme da contare.
  IF INSTR(after, " ack=0") > 0 THEN PRINT "confirms=none" ELSE PRINT "confirms=some"
  PRINT "deliveries="; deliveries

  MESSAGE CLOSE bus
  PRINT "done"
END SUB
