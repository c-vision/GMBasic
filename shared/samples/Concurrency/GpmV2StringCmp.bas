' G8 (piano ILBackend-GPM): confronto STRINGHE in condizione IF sotto la v2.
' Il path numerico dell'emitter double non gestiva un operando stringa (IF s$ = "x"): eccezione
' a metà emissione → StepV2 parziale → "Label not marked" al Save (lezione 7f2ac888, riemersa in
' NetUdpAdvanced con `IF remotePort = 0 AND response$ = ""`). Ora il confronto stringhe è emesso
' via String.Equals (parità flat). Output deterministico: 3 confronti veri.

SUB Check(n AS INTEGER)
  DIM s$ = "abc"
  SLEEP 1
  IF s$ = "abc" THEN PRINT "EQ-OK"; n
  IF s$ <> "xyz" THEN PRINT "NE-OK"; n
  IF n = 3 AND s$ = "abc" THEN PRINT "AND-STR-OK"; n
END SUB

SUB Main()
  GO SUB Check(1)
  GO SUB Check(2)
  GO SUB Check(3)
  SLEEP 30
  PRINT "GPM-V2-STRCMP-DONE"
END SUB
