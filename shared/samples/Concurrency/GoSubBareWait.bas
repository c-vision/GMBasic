' FILE: Concurrency/GoSubBareWait.bas
' ============================================================================
' WAIT NUDA E GO SUB SENZA IN — cantiere parallelo con verifica deterministica
' ============================================================================
' Esercita TUTTE le forme di lancio/attesa su ogni backend (IL/PCode/VM-C/C99/C99Native):
' - `GO SUB Nome(args)` senza IN + `WAIT` nuda (attende tutti i senza-IN)
' - `GO SUB Nome(args) IN wg` + `WAIT wg` (gruppo esplicito, non atteso dalla nuda)
' - nidificazione (un figlio lancia un nipote e lo attende con WAIT nuda)
' - `WAIT` nuda senza figli pendenti (no-op, non si blocca)
'
' L'output è DETERMINISTICO: i worker scrivono in variabili condivise dedicate
' e solo Main stampa, DOPO le attese, in ordine fisso (mai PRINT concorrenti).
' (Nota: store su array condiviso da task non usati — gap stepper IL tracciato a parte.)
' MiniTest: minitests/concurrency/gosub_bare_wait.bas
' Guida: Guide/utente/Concorrenza-GPM-Guida-Utente.md (sezione "WAIT nuda").
' ============================================================================

DIM r0 AS INTEGER
DIM r1 AS INTEGER
DIM r2 AS INTEGER
DIM r3 AS INTEGER
DIM r4 AS INTEGER
DIM grandDone AS INTEGER
DIM slowDone AS INTEGER

SUB Worker0()
    SLEEP 100
    r0 = 0
END SUB

SUB Worker1()
    SLEEP 200
    r1 = 10
END SUB

SUB Worker2()
    SLEEP 300
    r2 = 20
END SUB

SUB Worker3()
    SLEEP 100
    r3 = 30
END SUB

SUB GrandChild()
    SLEEP 150
    grandDone = -1
END SUB

SUB Foreman()
    GO SUB GrandChild()
    WAIT
    r4 = 400
END SUB

SUB Slow()
    SLEEP 800
    slowDone = -1
END SUB

SUB Main()
    DIM w AS WAITGROUP
    DIM ok AS INTEGER = -1
    GO SUB Worker0()
    GO SUB Worker1()
    GO SUB Worker2()
    GO SUB Worker3()
    GO SUB Foreman()
    WAIT
    PRINT r0
    PRINT r1
    PRINT r2
    PRINT r3
    PRINT r4
    IF grandDone <> -1 THEN ok = 0
    GO SUB Slow() IN w
    WAIT
    IF slowDone <> 0 THEN ok = 0
    WAIT w
    IF slowDone <> -1 THEN ok = 0
    WAIT
    IF ok THEN PRINT "yard-ok"
END SUB
