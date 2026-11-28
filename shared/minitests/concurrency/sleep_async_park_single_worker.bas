' MiniTest §0ter: CONCURRENCY.SLEEPASYNC (SLEEP come funzione) deve PARCHEGGIARE il task.
'
' PERCHÉ NON SI USA `SYSTEM.GOMAXPROCS(1)`: sulla VM/C lo shrink NON riduce il pool. Il pool nasce
' con min(NUMCPU,32) thread; GOMAXPROCS(n) mette solo `worker_live - n` token in `retire_pending`,
' ma il token è letto in cima al loop del worker e i worker inattivi sono fermi in
' pthread_cond_wait dentro gmb_sched_find_runnable: non tornano MAI in cima al loop, restano vivi
' e prendono comunque i task (misurato: con GOMAXPROCS(1) il vecchio test d'ordine restava verde
' anche col vecchio usleep). Con task in coda i token vengono poi consumati uno per worker e il
' watchdog può dichiarare deadlock.
'
' Quindi si SATURANO i thread che il pool crea davvero: 32 sleeper, cioè il CAP del pool
' (min(NUMCPU,32)) — un numero >= di qualunque pool reale. Il conteggio è una costante e non una
' query di sistema perché su IL `GOMAXPROCS(0)` restituisce un valore non inizializzato (4.6e18)
' e `NUMCPU()` in un programma con GO SUB non è affidabile.
' Con ogni worker dentro SLEEP:
'   * se SLEEP PARCHEGGIA, ogni park libera il worker: l'osservatore gira e trova tutti gli slot
'     `went` pieni e nessuno `slept` → "parked";
'   * se SLEEP DORME il worker (usleep: il caso VM/C pre-2026-11-23), l'osservatore non ottiene un
'     worker finché uno sleeper non ha finito → almeno uno `slept` → "blocked".
' La corsa sugli slot è evitata per costruzione: ogni slot ha UN solo scrittore (il suo sleeper) e
' l'osservatore li legge; niente ATOMIC (IL non mappa AtomicType) e niente contatori condivisi in
' scrittura (perderebbero incrementi). Gli slot sono azzerati a mano: su IL un DIM di array a
' livello di modulo non è zero-inizializzato. La barriera sta in un task suo, non in Main: su IL
' la ripresa di un frame con await riesegue le istruzioni precedenti (misurato: il loop di spawn
' si ripeteva, i arrivava a 65).
' Secondo segnale indipendente: col vecchio ramo usleep la guardia §0ter ferma il processo
' (exit 2, nessun output), quindi il file è rosso anche se un giorno il pool cambiasse.
' EXPECT: parked
' ---------------------------------------------------------------------------------------------------
' EX DEROGA — RISOLTA IL 2026-11-24 (difetto del subset v2 del backend IL). Fino a ieri questa
' gamba diceva "blocked" in 13.4 s: il difetto NON era SLEEPASYNC (il cui park era gia' corretto)
' ma DUE regole del subset v2, entrambe in IlCoroutineEmitter:
'   1) `x(i) = v` produce un ArrayElementTarget e il caso LetStmt di StatementIsV2 accettava solo
'      VariableTarget/FieldTarget -> la procedura usciva dal subset. Qui colpiva Main (il ciclo di
'      azzeramento) e Sleeper (went/slept): Main non v2 = entry point FLAT, e nel percorso flat il
'      GO SUB e' una chiamata INLINE, quindi i 32 figli giravano uno alla volta
'      (32 x 400 ms = 13.4 s, lineare in N x durata: 8 -> 3.9 s, 16 -> 7.1 s).
'   2) BodyAccessesGlobalArray usava un walker riflessivo che seguiva
'      FunctionExpr.ResolvedDefinition fino al CORPO DELLA PROCEDURA CHIAMATA: bastava un GO SUB
'      verso una SUB che LEGGEVA un array di modulo (Observer) per buttare fuori subset anche un
'      Main pulito.
' Fix: lo stepper v2 emette lo store su elemento delegando a IlRuntime.CollectionIndexSet (che
' legge il tipo elemento dall'istanza e converte come gli store statici: gli array di modulo
' nascono long[]/double[] dal global-init flat, i locali int[]/double[] da DIM — un conv.i2/i8
' emesso a indovinare sarebbe IL non verificabile), e il walker non attraversa piu' il bordo di
' chiamata.
' Misure (test_ilrun --engine=il): prima 13.46 s "blocked" con NESSUNO StepV2; dopo 1.0-1.05 s
' "parked" con StepV2 per Main e Sleeper (Sleeper parcheggia: SleepStep, non Task.Delay sul
' worker). `parity4` su questo file: 4/4 PASS (il == pcode == vmc == c99n). Se una gamba dice
' "blocked" e' una regressione vera.
' ---------------------------------------------------------------------------------------------------
IMPORT CONCURRENCY

CONST SLEEPERS = 32
DIM went(32) AS INTEGER     ' slot i: lo sleeper i e' entrato nell'attesa
DIM slept(32) AS INTEGER    ' slot i: lo sleeper i ha FINITO di dormire

SUB Sleeper(wg AS WAITGROUP, i AS INTEGER)
    went(i) = 1
    AWAIT CONCURRENCY.SLEEPASYNC(400)
    slept(i) = 1
    DONE wg
END SUB

SUB Observer(wg AS WAITGROUP)
    DIM i AS INTEGER
    DIM ready AS INTEGER
    DIM done AS INTEGER
    ' Barriera: si stampa appena TUTTI sono entrati nell'attesa, guardando nello stesso istante
    ' quanti ne hanno gia' finita (0 = i worker si sono liberati, quindi SLEEP ha parcheggiato).
    ' NIENTE await qui: sul backend IL la RIPRESA dopo un TASKYIELD arriva solo al timer
    ' successivo (~300 ms), e l'osservatore leggerebbe gli slot a sonno gia' finito. Il task
    ' parte subito, quindi polla senza cedere: il quanto del worker lo prerilascia comunque
    ' (§11), e sugli altri backend non cambia nulla.
    ready = 0
    WHILE ready = 0
        ready = 1
        done = 0
        FOR i = 1 TO SLEEPERS
            IF went(i) = 0 THEN ready = 0
            IF slept(i) <> 0 THEN done = 1
        NEXT
    WEND
    IF done = 0 THEN PRINT "parked" ELSE PRINT "blocked"
    DONE wg
END SUB

SUB Main()
    DIM i AS INTEGER
    DIM wg AS WAITGROUP
    FOR i = 1 TO SLEEPERS
        went(i) = 0
        slept(i) = 0
    NEXT
    FOR i = 1 TO SLEEPERS
        GO SUB Sleeper(wg, i) IN wg
    NEXT
    GO SUB Observer(wg) IN wg
    WAIT wg
END SUB
