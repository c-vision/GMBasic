' PCodeVM: parità Go runtime.GOMAXPROCS(n) — ridimensionamento dei P a RUNTIME.
' Con GOMAXPROCS=2 all'avvio, la funzione SYSTEM.GOMAXPROCS riduce a 1 DURANTE
' l'esecuzione di 50 goroutine concorrenti: lo scheduler migra i task delle code
' locali dei P rimossi alla coda globale (nessun task perso — invariante Go).
' Il totale deterministico (50 * sum(1..100) = 252500) verifica la correttezza.

IMPORT CONCURRENCY

SUB Worker(Id AS INTEGER, N AS INTEGER, Results AS CHANNEL OF INTEGER)
  DIM i AS INTEGER
  DIM acc AS INTEGER = 0
  FOR i = 1 TO N
    acc = acc + i
  NEXT i
  SEND acc TO Results
END SUB

SUB Main()
  DIM Results AS CHANNEL OF INTEGER
  DIM wg AS WAITGROUP
  DIM cur AS LONG
  DIM prev AS LONG
  DIM i AS INTEGER
  DIM Total AS LONG = 0

  cur = SYSTEM.GOMAXPROCS(0)
  PRINT "GMP-CURRENT="; cur

  ' Ridimensiona a 2, poi torna a 1 durante l'esecuzione delle goroutine.
  prev = SYSTEM.GOMAXPROCS(2)
  PRINT "GMP-SET-2-PREV="; prev

  FOR i = 1 TO 50
    GO SUB Worker(i, 100, Results) IN wg
  NEXT i

  ' Riduzione a runtime mentre le goroutine girano: i P eccedenti vengono rimossi
  ' e i loro task locali migrati alla coda globale — MAI persi.
  prev = SYSTEM.GOMAXPROCS(1)
  PRINT "GMP-SET-1-PREV="; prev

  FOR i = 1 TO 50
    DIM v AS INTEGER
    RECEIVE v FROM Results
    Total = Total + v
  NEXT i

  WAIT wg
  ' Ogni Worker(i,100) fa sum(1..100) = 5050; totale = 50 * 5050 = 252500
  IF Total = 252500 THEN
    PRINT "PCODE-GMP-RESIZE-OK"
  ELSE
    PRINT "FAIL:GMP-TOTAL="; Total
  END IF
END SUB
