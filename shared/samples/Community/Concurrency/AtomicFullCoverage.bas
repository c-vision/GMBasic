' FILE: samples/Community/Concurrency/AtomicFullCoverage.bas
' ======================================================================================================================
' ATOMIC — copertura completa del namespace (5/5) in uno scenario deterministico.
'
' Le 5 funzioni del namespace ATOMIC (equivalente Go sync/atomic sui long):
'   ATOMIC.INCREMENT(x, n)     ritorna il valore PRECEDENTE (AddInt64)
'   ATOMIC.FETCHVALUE(x)       ritorna il valore corrente  (LoadInt64)
'   ATOMIC.STORE(x, n)         imposta il valore           (StoreInt64)
'   ATOMIC.COMPARESWAP(x,o,n)  CAS: BIT -1 se riuscito, 0 altrimenti (CompareAndSwapInt64)
'   ATOMIC.SWAP(x, n)          ritorna il valore PRECEDENTE (SwapInt64)
'
' Scenario (deterministico, niente rete/timing): un contatore condiviso viene
' incrementato concorrentemente da worker, poi verificato con read/STORE/CAS/SWAP.
' ======================================================================================================================

IMPORT CONCURRENCY
IMPORT ATOMIC

' Worker: somma un contributo deterministico (es. 1+2+...+n = n(n+1)/2).
SUB Worker(c AS ATOMIC, seed AS LONG)
    DIM i AS LONG
    DIM total AS LONG
    FOR i = 1 TO seed
        ATOMIC.INCREMENT(c, i)
        total = total + 1
    NEXT i
END SUB

SUB Main()
    DIM c AS ATOMIC
    DIM wg AS WAITGROUP
    DIM i AS LONG
    DIM prev AS LONG
    DIM sw AS BIT

    ' ── 1) Concorrenza: 4 worker sommano 1+2+...+10 ciascuno = 4 * 55 = 220 ──
    GO SUB Worker(c, 10) IN wg
    GO SUB Worker(c, 10) IN wg
    GO SUB Worker(c, 10) IN wg
    GO SUB Worker(c, 10) IN wg
    WAIT wg
    PRINT "counter="; ATOMIC.FETCHVALUE(c)          ' 220

    ' ── 2) STORE + FETCHVALUE round-trip ──
    ATOMIC.STORE(c, 42)
    IF ATOMIC.FETCHVALUE(c) = 42 THEN PRINT "store= ok" ELSE PRINT "store= fail"

    ' ── 3) COMPARESWAP: CAS riuscito (42 -> 7) e fallito (42 -> 99) ──
    sw = ATOMIC.COMPARESWAP(c, 42, 7)
    IF sw <> 0 AND ATOMIC.FETCHVALUE(c) = 7 THEN PRINT "cas= ok" ELSE PRINT "cas= fail"
    sw = ATOMIC.COMPARESWAP(c, 42, 99)
    IF sw = 0 AND ATOMIC.FETCHVALUE(c) = 7 THEN PRINT "cas-fail= ok" ELSE PRINT "cas-fail= fail"

    ' ── 4) SWAP ritorna il valore precedente ──
    prev = ATOMIC.SWAP(c, 123)
    IF prev = 7 AND ATOMIC.FETCHVALUE(c) = 123 THEN PRINT "swap= ok" ELSE PRINT "swap= fail"

    ' ── 5) INCREMENT ritorna il NUOVO valore (parità Go atomic.AddInt64) ──
    prev = ATOMIC.INCREMENT(c, 1)
    IF prev = 124 AND ATOMIC.FETCHVALUE(c) = 124 THEN PRINT "inc-prev= ok" ELSE PRINT "inc-prev= fail"

    PRINT "done"
END SUB
