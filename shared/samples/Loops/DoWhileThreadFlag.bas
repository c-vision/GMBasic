' FILE: Loops/DoWhileThreadFlag.bas
' Replica del bug: DO WHILE NOT flag con flag scritta da un thread (GPM goroutine).
' Sintomo: il loop NON esce anche quando flag diventa vera (la scrittura del thread
' non è visibile al loop, o la condizione emessa non rilegge la variabile globale).

IMPORT CONCURRENCY

DIM flag AS BIT
DIM wg AS WAITGROUP

SUB SetFlag()
  SLEEP 300
  flag = 1
  DONE wg
END SUB

SUB Main()
  DIM n AS INTEGER

  GO SUB SetFlag() IN wg

  n = 0
  DO WHILE NOT flag
    SLEEP 50
    n = n + 1
    IF n > 200 THEN
      PRINT "TIMEOUT: il DO WHILE non esce (bug) — flag="; flag
      EXIT DO
    END IF
  LOOP

  ' Il NUMERO esatto di iterazioni dipende dallo scheduling (SLEEP vs 300ms del thread)
  ' → non è deterministico fra backend. Si stampa il FATTO deterministico: il loop è
  ' USCITO (n>0) e flag è diventata vera (visibilità del globale scritto dalla goroutine).
  ' `uscito` è assegnato -1 dentro un IF (canone -1/0) perché il VALORE di un confronto
  ' usato come espressione differisce (1 vs -1) su questa gamba storica.
  DIM uscito AS INTEGER = 0
  IF n > 0 THEN uscito = -1
  PRINT "flag="; flag; " uscito="; uscito
  WAIT wg

  ' Regressione NOT type-directed: BIT logico, numerico bitwise.
  flag = 1
  IF (NOT flag) <> 0 THEN PRINT "FAIL: NOT BIT 1" ELSE PRINT "PASS: NOT BIT 1"
  flag = -1
  IF (NOT flag) <> 0 THEN PRINT "FAIL: NOT BIT -1" ELSE PRINT "PASS: NOT BIT -1"
  flag = 0
  IF (NOT flag) = 0 THEN PRINT "FAIL: NOT BIT 0" ELSE PRINT "PASS: NOT BIT 0"
  IF (NOT 5) <> -6 THEN PRINT "FAIL: NOT numerico" ELSE PRINT "PASS: NOT numerico"
END SUB
