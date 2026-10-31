' GO SELECT con rendezvous: senza DEFAULT il select parcheggia finché il
' produttore non invia (parità Go: select { case v := <-ch: } blocca).
' Un CASE DEFAULT qui lascerebbe il produttore parcheggiato in SEND per sempre
' (il WAIT WG successivo deadloccerebbe) — semantica Go corretta, non un gap.

SUB Produttore(C AS CHANNEL OF INTEGER, WG AS WAITGROUP)
  SEND 42 TO C
  DONE WG
END SUB

SUB Main()
  DIM C AS CHANNEL OF INTEGER
  DIM WG AS WAITGROUP
  GO SUB Produttore(C, WG) IN WG
  
  DIM Valore AS INTEGER
  GO SELECT
    CASE RECEIVE Valore FROM C:
      PRINT "Ricevuto:"; Valore
  END SELECT
  
  WAIT WG
  PRINT "Fine"
END SUB
