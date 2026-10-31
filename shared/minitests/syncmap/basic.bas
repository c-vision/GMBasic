' EXPECT: store-load-ok|loadorstore-ok|delete-ok
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
END SUB
