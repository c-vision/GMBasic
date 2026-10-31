' Minitest MESSAGE.DELIVERY — contatori di consegna del bus e RI-CONSEGNA dopo NACK.
' Scenario at-least-once: ci si sottoscrive al broker senza registrare un handler, quindi ogni
' consegna resta ORFANA → il client risponde NACK → il broker ri-consegna (`retry_ms`) fino a
' `max_attempts`. MESSAGE.DELIVERY(bus) rende osservabile proprio questo ramo:
'   "sent=S recv=R ack=A nack=N dup=D last=L"
' con sent = PUB scritte al broker, recv = MSG ricevute, ack/nack = conferme inviate,
' dup = MSG con id già visto (le ri-consegne), last = massimo id visto.
' Asserzioni (parole, non numeri: il numero di retry dipende da retry_ms del broker):
'   bus del publisher  sent=1 (la PUB è arrivata al broker);
'   bus del subscriber recv>0 (ha ricevuto), ack=0 (nessun handler ha consumato),
'   nack>0 e dup>0  ⇒ la ri-consegna è stata OSSERVATA. Senza il dup il test non ha visto il
'   retry: senza broker il CONNECT ripiega sul bus locale (contatori a zero) → "nodups",
'   niente falso verde da fallback locale.
' EXPECT: retried
IMPORT MESSAGE

SUB Main()
  DIM subb AS LONG
  DIM pubb AS LONG
  DIM tries AS LONG
  DIM ok AS LONG = 0
  DIM st AS STRING
  DIM stp AS STRING
  MESSAGE CONNECT "tcp://127.0.0.1:9215" AS subb
  MESSAGE CONNECT "tcp://127.0.0.1:9215" AS pubb
  ' Nessun ON MESSAGE su "s2": il messaggio resta orfano → NACK → il broker lo ri-consegna.
  MESSAGE SUBSCRIBE subb TOPIC("d") AS "s2"
  MESSAGE PUBLISH pubb TOPIC("d") DATA("x")
  WHILE ok = 0 AND tries < 400
    SLEEP 5
    tries = tries + 1
    st = MESSAGE.DELIVERY(subb)
    stp = MESSAGE.DELIVERY(pubb)
    IF INSTR(stp, "sent=1 ") > 0 THEN
      IF INSTR(st, " recv=0") = 0 THEN
        IF INSTR(st, " nack=0") = 0 THEN
          IF INSTR(st, " dup=0") = 0 THEN
            IF INSTR(st, " ack=0") > 0 THEN ok = 1
          END IF
        END IF
      END IF
    END IF
  WEND
  IF ok = 1 THEN PRINT "retried" ELSE PRINT "nodups"
  MESSAGE CLOSE pubb
  MESSAGE CLOSE subb
END SUB
