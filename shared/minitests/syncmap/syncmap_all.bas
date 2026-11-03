' MiniTest NAMESPACE SYNCMAP — tutte le 4 funzioni della superficie.
' STORE, LOAD, LOADORSTORE, DELETE su CONCURRENTMAP (parità Go sync.Map).
' Contratto ownership verificato: il valore torna alla variabile e la mappa resta valida.
' Solo valori STRING: il LOAD di un valore numerico è un gap del backend IL
' (InvalidProgramException, vedi Bugs/(open) - SYNCMAP-VMC-Crash.md -> nota numerica).
' EXPECT: store-load-ok|loadorstore-ok|delete-ok|missing=[]
IMPORT CONCURRENCY
IMPORT SYNCMAP

SUB Main()
  DIM m AS CONCURRENTMAP
  DIM v AS STRING
  SYNCMAP.STORE(m, "chiave", "valore")
  v = SYNCMAP.LOAD(m, "chiave")
  IF v = "valore" THEN PRINT "store-load-ok"
  v = SYNCMAP.LOADORSTORE(m, "chiave", "nuovo")
  IF v = "valore" THEN PRINT "loadorstore-ok"
  v = SYNCMAP.DELETE(m, "chiave")
  IF v = "valore" THEN PRINT "delete-ok"
  DIM manca AS STRING
  manca = SYNCMAP.LOAD(m, "chiave")
  PRINT "missing=["; manca; "]"
END SUB
