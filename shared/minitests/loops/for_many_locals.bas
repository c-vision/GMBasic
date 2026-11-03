' EXPECT: formanyloc-ok
' MiniTest: FOR dentro una procedura con MOLTI slot locali (2026-11-03).
'
' Bug VM/C (corretto): gmb_for_state_t aveva counter/end/step/active dimensionati a 64,
' indicizzati dallo slot del locale che fa da contatore. In una procedura grande gli slot
' superano 63 anche con pochi DIM, perche' il compilatore (cc_local_add) assegna uno slot
' nuovo a ogni temp che genera e non lo riusa mai. Con slot >= 64 il FOR_PREPARE non
' marcava active e il FOR_LOOP usciva subito: UNA sola iterazione, SENZA errore.
' Fix: array su GMB_FRAME_LOCALS_CAP (gmb_gpm.h) e bound allineati in gmb_vm_core.c.
' Dettaglio e repro: vault Bugs/(solved) - VMC-For-Solo-Una-Iterazione-Con-Slot-Locale-Oltre-63.md
'
' Forma reale che l'ha scoperto: SUB PcfSave dell'app GlyphSmith (PCF con una proprieta'
' invece di nove, encoding massimo 65 invece di 66).
SUB Many()
  DIM ok AS INTEGER = 0
  DIM i AS INTEGER, j AS INTEGER, n AS INTEGER, m AS INTEGER, mx AS INTEGER
  DIM d00 AS INTEGER, d01 AS INTEGER, d02 AS INTEGER, d03 AS INTEGER, d04 AS INTEGER
  DIM d05 AS INTEGER, d06 AS INTEGER, d07 AS INTEGER, d08 AS INTEGER, d09 AS INTEGER
  DIM d10 AS INTEGER, d11 AS INTEGER, d12 AS INTEGER, d13 AS INTEGER, d14 AS INTEGER
  DIM d15 AS INTEGER, d16 AS INTEGER, d17 AS INTEGER, d18 AS INTEGER, d19 AS INTEGER
  DIM d20 AS INTEGER, d21 AS INTEGER, d22 AS INTEGER, d23 AS INTEGER, d24 AS INTEGER
  DIM d25 AS INTEGER, d26 AS INTEGER, d27 AS INTEGER, d28 AS INTEGER, d29 AS INTEGER
  DIM d30 AS INTEGER, d31 AS INTEGER, d32 AS INTEGER, d33 AS INTEGER, d34 AS INTEGER
  DIM d35 AS INTEGER, d36 AS INTEGER, d37 AS INTEGER, d38 AS INTEGER, d39 AS INTEGER
  DIM d40 AS INTEGER, d41 AS INTEGER, d42 AS INTEGER, d43 AS INTEGER, d44 AS INTEGER
  DIM d45 AS INTEGER, d46 AS INTEGER, d47 AS INTEGER, d48 AS INTEGER, d49 AS INTEGER
  DIM d50 AS INTEGER, d51 AS INTEGER, d52 AS INTEGER, d53 AS INTEGER, d54 AS INTEGER
  DIM d55 AS INTEGER, d56 AS INTEGER, d57 AS INTEGER, d58 AS INTEGER, d59 AS INTEGER
  DIM d60 AS INTEGER, d61 AS INTEGER, d62 AS INTEGER, d63 AS INTEGER, d64 AS INTEGER
  DIM d65 AS INTEGER, d66 AS INTEGER, d67 AS INTEGER, d68 AS INTEGER, d69 AS INTEGER
  DIM enc(4) AS INTEGER
  enc(0) = 32
  enc(1) = 65
  enc(2) = 66
  enc(3) = 90
  enc(4) = 120
  n = 0
  FOR i = 1 TO 3
    n = n + 1
  NEXT i
  m = 0
  FOR i = 1 TO 2
    FOR j = 1 TO 3
      m = m + 1
    NEXT j
  NEXT i
  ' scansione del massimo: l'ultimo elemento deve contare (sintomo "65 invece di 66")
  mx = -1
  FOR i = 0 TO 4
    IF enc(i) > mx THEN mx = enc(i)
  NEXT i
  IF n = 3 THEN
    IF m = 6 THEN
      IF mx = 120 THEN ok = 1
      IF mx <> 120 THEN PRINT "max="; mx
    END IF
    IF m <> 6 THEN PRINT "m="; m
  END IF
  IF n <> 3 THEN PRINT "n="; n
  IF ok THEN PRINT "formanyloc-ok" ELSE PRINT "formanyloc-fail"
END SUB
SUB Main()
  Many()
END SUB
