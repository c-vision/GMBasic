' SYNCMAP: mappa concorrente (parita' Go sync.Map: Store/Load/Delete/LoadOrStore).
IMPORT CONCURRENCY
IMPORT SYNCMAP

SUB Main()
  DIM m AS CONCURRENTMAP
  DIM v AS STRING
  DIM d AS STRING
  SYNCMAP.STORE(m, "chiave", "valore")
  v = SYNCMAP.LOAD(m, "chiave")
  IF v = "valore" THEN PRINT "SYNCMAP-STORE-LOAD-OK"
  d = SYNCMAP.DELETE(m, "chiave")
  IF d = "valore" AND SYNCMAP.LOAD(m, "chiave") = "" THEN PRINT "SYNCMAP-DELETE-OK"
END SUB
