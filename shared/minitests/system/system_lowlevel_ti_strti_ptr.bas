' MiniTest: canone SYSTEM low-level (2026-09-30, bug SYSTEM-LowLevel risolto).
'  - TI() = millisecondi interi (stessa unita su IL/PCode/VM-C/C99/C99Native).
'  - STRTI() = "HH:MM:SS" (8 caratteri) su tutte le gambe.
'  - VARPTR/STRPTR = token opaco di identita, mai 0 (sulle gambe native coincide
'    con un indirizzo reale, su quelle gestite con un handle: non dereferenziabile).
'  Asserzioni solo su proprieta portabili (niente valori assoluti).
' EXPECT: syslow-ok
IMPORT SYSTEM
IMPORT CONCURRENCY
SUB Main()
  DIM ok AS INTEGER = -1
  DIM i AS INTEGER = 123
  DIM s AS STRING = "Hello"
  IF SYSTEM.VARPTR(i) = 0 THEN ok = 0
  IF SYSTEM.VARPTR(i) <> SYSTEM.VARPTR(i) THEN ok = 0
  IF SYSTEM.STRPTR(s) = 0 THEN ok = 0
  IF SYSTEM.STRPTR(s) <> SYSTEM.STRPTR(s) THEN ok = 0
  DIM t1 = SYSTEM.TI()
  SLEEP 100
  DIM t2 = SYSTEM.TI()
  IF t2 < t1 THEN ok = 0
  IF (t2 - t1) < 50 THEN ok = 0
  IF LEN(SYSTEM.STRTI()) <> 8 THEN ok = 0
  IF ok THEN PRINT "syslow-ok"
END SUB
