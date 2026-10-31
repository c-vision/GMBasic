' FILE: Concurrency/SyncMapDemo.bas
' =================================================================================================
' SYNCMAP — mappa concorrente (parità Go sync.Map: Store/Load/LoadOrStore/Delete).
' =================================================================================================
' DIM m AS CONCURRENTMAP
' SYNCMAP.STORE(m, key$, value)              → scrive (sovrascrive)
' v = SYNCMAP.LOAD(m, key$)                  → legge ("" se assente)
' r = SYNCMAP.LOADORSTORE(m, key$, value)    → carica se esiste, altrimenti memorizza
' d = SYNCMAP.DELETE(m, key$)                → rimuove e ritorna il valore precedente
' =================================================================================================
IMPORT CONCURRENCY
IMPORT SYNCMAP

SUB Main()
    CLS
    PRINT "SYNCMAP — mappa concorrente"
    PRINT "==========================="
    PRINT

    DIM m AS CONCURRENTMAP
    DIM v AS STRING

    SYNCMAP.STORE(m, "chiave", "valore")
    v = SYNCMAP.LOAD(m, "chiave")
    IF v = "valore" THEN PRINT "STORE + LOAD: OK"
    PRINT

    v = SYNCMAP.LOADORSTORE(m, "chiave", "nuovo")
    IF v = "valore" THEN PRINT "LOADORSTORE (chiave esistente): ritorna il valore presente"
    v = SYNCMAP.LOAD(m, "chiave")
    IF v = "valore" THEN PRINT "  valore non sovrascritto"
    PRINT

    v = SYNCMAP.LOADORSTORE(m, "assente", "messo")
    IF v = "messo" THEN PRINT "LOADORSTORE (chiave assente): memorizza e ritorna il nuovo"
    v = SYNCMAP.LOAD(m, "assente")
    IF v = "messo" THEN PRINT "  chiave ora presente"
    PRINT

    v = SYNCMAP.DELETE(m, "chiave")
    IF v = "valore" THEN PRINT "DELETE: ritorna il valore rimosso"
    v = SYNCMAP.LOAD(m, "chiave")
    IF v = "" THEN PRINT "  chiave rimossa (LOAD vuoto)"
END SUB
