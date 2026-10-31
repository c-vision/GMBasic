' FILE: Databases/RDD_Omonime.bas
' Test comandi vs funzioni omonime nello stesso file:
'   comando legacy:   RDD DELETE ALL  /  RDD RECALL ALL  /  RDD REPLACE f WITH v
'   funzione:         RDD.DELETE(...) / RDD.RECALL(...) / RDD.REPLACE(...)
'   comando lock:     RDD RLOCK 1  /  RDD FLOCK 1
'   funzione lock:    RDD.RLOCK(...) / RDD.FLOCK(...)
' ===================================================================

IMPORT RDD
IMPORT CONSOLE

SUB Main()
    PRINT "=== RDD: comandi e funzioni omonime ==="
    PRINT

    ' --- Apri area in memoria ---
    RDD SET DEFAULT "ARRAY"
    RDD USE ":memory:" ALIAS Area1

    ' --- LOCK: comando legacy e funzione omonima ---
    PRINT "1. RLOCK/FLOCK: comando vs funzione"
    RDD RLOCK 1                       ' comando legacy
    ok1 = RDD.RLOCK(1)                ' funzione (ritorna Bit)
    PRINT "   RDD.RLOCK(1)="; ok1

    RDD FLOCK 1                       ' comando legacy
    ok2 = RDD.FLOCK(1)                ' funzione
    PRINT "   RDD.FLOCK(1)="; ok2

    RDD UNLOCK 1

    ' --- DELETE/RECALL: comando legacy e funzione omonima ---
    PRINT "2. DELETE: comando e funzione"
    RDD DELETE ALL                    ' comando legacy con scope
    n1 = RDD.DELETE(1, 1)             ' funzione (ritorna conteggio)
    PRINT "   RDD.DELETE(1,1)="; n1

    RDD RECALL ALL                    ' comando legacy con scope
    n2 = RDD.RECALL(1, 1)             ' funzione
    PRINT "   RDD.RECALL(1,1)="; n2

    ' --- REPLACE: comando legacy e funzione omonima ---
    PRINT "3. REPLACE: comando e funzione"
    RDD REPLACE Nome WITH "via comando" ALL   ' comando legacy
    n3 = RDD.REPLACE(1, "Nome", "via funzione", 1)  ' funzione
    PRINT "   RDD.REPLACE(...)="; n3

    PRINT
    PRINT "--- TEST COMPLETATO ---"
END SUB
