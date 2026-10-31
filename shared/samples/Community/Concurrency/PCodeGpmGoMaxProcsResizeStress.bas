' PCodeVM: stress del ridimensionamento runtime dei P (parità Go GOMAXPROCS).
' 5 round: ognuno lancia 40 goroutine, poi fa resize 4→2→1 MENTRE girano.
' La migrazione dei task dei P rimossi alla coda globale non deve perdere nulla;
' il totale deterministico (5 * 40 * sum(1..50)=1275 = 255000) verifica la
' correttezza — un task perso -> hang (CTS 30s) o conteggio errato.

IMPORT CONCURRENCY

SUB Worker(Id AS INTEGER, Results AS CHANNEL OF INTEGER)
  DIM i AS INTEGER
  DIM acc AS INTEGER = 0
  FOR i = 1 TO 50
    acc = acc + i
  NEXT i
  SEND acc TO Results
END SUB

SUB Main()
  DIM Results AS CHANNEL OF INTEGER
  DIM wg AS WAITGROUP
  DIM r AS INTEGER
  DIM i AS INTEGER
  DIM Total AS LONG = 0

  FOR r = 1 TO 5
    FOR i = 1 TO 40
      GO SUB Worker(i, Results) IN wg
    NEXT i
    ' Resize multipli (up e down) durante l'esecuzione delle goroutine.
    SYSTEM.GOMAXPROCS(4)
    SYSTEM.GOMAXPROCS(2)
    SYSTEM.GOMAXPROCS(1)
    FOR i = 1 TO 40
      DIM v AS INTEGER
      RECEIVE v FROM Results
      Total = Total + v
    NEXT i
    WAIT wg
  NEXT r

  ' 5 round * 40 worker * sum(1..50)=1275 = 255000
  IF Total = 255000 THEN
    PRINT "PCODE-GMP-RESIZE-STRESS-OK"
  ELSE
    PRINT "FAIL:GMP-STRESS="; Total
  END IF
END SUB
