# III.13 — GPM e concorrenza

## Cosa ottieni da questo capitolo

Task paralleli, canali, gruppi di attesa, lock e select — con la garanzia che il codice BASIC
gira sempre su un solo esecutore alla volta. Il modello è quello di Go, il vocabolario è lo
stesso, ma le divergenze sono dichiarate e volute.

## 1. Avviare un task

| Forma | Effetto |
|---|---|
| `GO SUB Nome(args)` | avvia la SUB come task **senza gruppo** |
| `GO SUB Nome(args) IN wg` | avvia nel gruppo `wg` |
| `GO FUNCTION F(args) INTO v` | avvia e riceve il risultato in `v` (future) |
| `SLEEP ms` | sospende **solo** il task corrente |
| `YIELD` | cede il turno senza attendere |

```basic
GO SUB Produttore()
WAIT                 ' attende i figli senza IN
PRINT "fatto"
```

`WAIT` nuda attende i figli **senza** `IN`; `WAIT wg` attende il gruppo `wg` e non i senza-`IN`.
Senza figli mai lanciati la `WAIT` nuda è un no-op: non si blocca.

## 2. Canali

```basic
DIM ch AS CHANNEL
CHANNEL CREATE ch, 4
CHANNEL PUBLISH ch, 42
v = CHANNEL SUBSCRIBE(ch)
```

Il canale è un buffer circolare (capacità predefinita 10). `SEND` su canale pieno e `RECEIVE` su
canale vuoto **parcheggiano** il task: non fermano il worker e non fermano gli altri task.

Eseguito (`shared/samples/Concurrency/ChannelBasics.bas`):

```
Val= 42
```

## 3. Un programma completo

`shared/samples/Concurrency/GoRoutinesMinimal.bas`, backend C99Native:

```
[MAIN] Avvio GO SUB Produttore...
[MAIN] Ricevo primo valore...
[PRODUTTORE] Avviato.
[PRODUTTORE] Invio 10...
[MAIN] Ricevuto:  10
[MAIN] Ricevo secondo valore...
[PRODUTTORE] Invio 20...
[MAIN] Ricevuto:  20
[MAIN] Ricevo terzo valore...
[PRODUTTORE] Invio 30...
[PRODUTTORE] Finito. Eseguo DONE.
[MAIN] Ricevuto:  30
[MAIN] Attendo il completamento del produttore...
[MAIN] Produttore terminato.

--- TEST COMPLETATO ---
```

L'ordine delle righe è la lezione del capitolo: `[PRODUTTORE] Avviato.` compare **dopo**
`[MAIN] Ricevo primo valore...`. Il main è arrivato alla ricezione prima che il figlio partisse.
Non è un bug e non è riproducibile a mano: due task procedono in parallelo e lo scheduler decide.

## 4. Sincronizzazione

| Primitive | Uso |
|---|---|
| `WAITGROUP` | `WG ADD` / `WG DONE` / `WAIT wg` |
| `MUTEX` | `MUTEX LOCK` / `UNLOCK`; `TRYLOCK` non blocca |
| `RWLOCK` | lock lettore/scrittore |
| `CHANNEL` | passaggio di messaggi |
| `SYNCMAP` | mappa thread-safe (`STORE`/`LOAD`/`LOADORSTORE`/`DELETE`) |
| `ATOMIC` | operazioni atomiche su scalari |

`MUTEX.TRYLOCK(mu)` ritorna il booleano canonico `-1` (acquisito) / `0` (occupato), senza
parcheggiare: serve per provare e desistere, non per attendere.

## 5. `GO SELECT`

Attende su più canali e sceglie il primo pronto. La scelta è **deterministica (FCFS)**, non
pseudo-casuale come in Go: il programma che scegli è sempre lo stesso, e si può debuggare.

## 6. `DEFER`, `DOEVENTS` e il park

- `DEFER` registra un'azione da eseguire all'uscita dal blocco, anche per errore.
- `DOEVENTS` lascia girare gli eventi pendenti senza bloccare il task.
- Le operazioni SQL che riscrivono un file intero e la rete sono **park-based**: il task si
  parcheggia, il lavoro gira fuori, gli altri task proseguono. Le letture restano sincrone,
  perché parcheggiarle costerebbe più di quanto faccia risparmiare.

## 7. Divergenze da Go (deliberate)

Non sono difetti e non vanno "corrette":

- `GO SELECT` deterministico, non casuale;
- i task parcheggiati vengono cancellati al timeout, come rete di sicurezza;
- `CATCH`/`ON ERROR` cattura più di `recover()`, perché è gestione errori del linguaggio intero;
- le primitive usano lock veri, garanzia più forte del modello minimale di Go;
- la parità che conta è **fra i quattro backend**, non contro un binario Go.

## 8. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| deadlock immediato su `SEND` | nessun ricevente e buffer pieno | crea il ricevente prima, o capacita il canale |
| deadlock su `WAIT` | manca un `DONE` | mettilo in `FINALLY`, non solo nel percorso felice |
| risultato `NIL` da `GO FUNCTION` | valore consegnato per `INTO` su IL è un limite noto | scrivi il risultato in una variabile condivisa dal corpo |
| valori incoerenti fra task | scrittura concorrente senza lock | `MUTEX` o `SYNCMAP` |

## 9. Dove andare adesso

- **III.12** — come è fatto lo scheduler che rende tutto questo possibile.
- **III.14** — `TRY`/`CATCH`/`FINALLY` e gli errori.
- **IV.24** — `FETCH` e rete dentro un task.












<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **ATOMIC.COMPARESWAP(x1, x2, x3)** — `sw = ATOMIC.COMPARESWAP(c, 42, 7)` (shared/samples/Concurrency/PCodeGpmAtomic.bas:26)
- **ATOMIC.FETCHVALUE(x1)** — `v = ATOMIC.FETCHVALUE(c)` (shared/samples/Concurrency/PCodeGpmAtomic.bas:24)
- **ATOMIC.INCREMENT(x1, x2)** — `ATOMIC.INCREMENT(c, 1)` (shared/samples/Concurrency/PCodeGpmAtomic.bas:10)
- **ATOMIC.STORE(x1, x2)** — `ATOMIC.STORE(c, 42)` (shared/samples/Concurrency/PCodeGpmAtomic.bas:25)
- **ATOMIC.SWAP(x1, x2)** — `prev = ATOMIC.SWAP(c, 7)` (shared/samples/Concurrency/GpmV2AtomicSwap.bas:9)
- **CONCURRENCY.SLEEPASYNC(x1)** — `AWAIT CONCURRENCY.SLEEPASYNC(20)` (shared/samples/Concurrency/AsyncHelpers.bas:12)
- **CONCURRENCY.TASKYIELD()** — `AWAIT CONCURRENCY.TASKYIELD()` (shared/samples/Concurrency/AsyncHelpers.bas:13)
- **CONTEXT.BACKGROUND()** — `root = CONTEXT.BACKGROUND()` (shared/samples/Concurrency/PCodeGpmContext.bas:26)
- **CONTEXT.CANCEL(x1)** — `CONTEXT.CANCEL(child)` (shared/samples/Concurrency/PCodeGpmContext.bas:30)
- **CONTEXT.CANCELLED(x1)** — `IF ok = 0 AND CONTEXT.CANCELLED(ctx) = 1 THEN` (shared/samples/Concurrency/PCodeGpmContext.bas:14)
- **CONTEXT.DONE(x1)** — `done = CONTEXT.DONE(ctx)` (shared/samples/Concurrency/PCodeGpmContext.bas:12)
- **CONTEXT.WITHCANCEL(x1)** — `child = CONTEXT.WITHCANCEL(root)` (shared/samples/Concurrency/PCodeGpmContext.bas:27)
- **MEMORY.DELETE(x1)** — `MEMORY.DELETE("api-key-config")` (shared/samples/Ai/MemoryDemo.bas:39)
- **MEMORY.LIST()** — `DIM allKeys$ = MEMORY.LIST()` (shared/samples/Ai/MemoryDemo.bas:34)
- **MEMORY.LOAD(x1)** — `DIM nome$ = MEMORY.LOAD("user-name")` (shared/samples/Ai/MemoryDemo.bas:19)
- **MEMORY.SEARCH(x1)** — `DIM results$ = MEMORY.SEARCH("user")` (shared/samples/Ai/MemoryDemo.bas:28)
- **MEMORY.STORE(x1, x2, x3)** — `MEMORY.STORE("user-name", "Mario Rossi", 0)` (shared/samples/Ai/MemoryDemo.bas:13)
- **MUTEX.TRYLOCK(x1)** — `DIM preso = MUTEX.TRYLOCK(mu)` (shared/samples/Math/MathAtnVectorAngle.bas:82)
- **POOL.GET(x1)** — `v = POOL.GET(p)` (shared/samples/Concurrency/GpmV2Pool.bas:10)
- **POOL.PUT(x1, x2)** — `POOL.PUT(p, "risorsa")` (shared/samples/Concurrency/GpmV2Pool.bas:9)
- **SYNCMAP.DELETE(x1, x2)** — `d = SYNCMAP.DELETE(m, "chiave")` (shared/samples/Concurrency/GpmV2SyncMap.bas:12)
- **SYNCMAP.LOAD(x1, x2)** — `v = SYNCMAP.LOAD(m, "chiave")` (shared/samples/Concurrency/GpmV2SyncMap.bas:10)
- **SYNCMAP.LOADORSTORE(x1, x2, x3)** — `v = SYNCMAP.LOADORSTORE(m, "chiave", "nuovo")` (shared/samples/Concurrency/SyncMapDemo.bas:28)
- **SYNCMAP.STORE(x1, x2, x3)** — `SYNCMAP.STORE(m, "chiave", "valore")` (shared/samples/Concurrency/GpmV2SyncMap.bas:9)
<!-- END DOMAIN TABLE -->

<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### PCodeVM-GPM-User-Manual

*Fonte: `PCodeVM-GPM-User-Manual`*


## 1. Come eseguire gli esempi

IMPORT CONCURRENCY


## 2. Il modello concettuale
| Meccanismo | Scopo | Sezione |
|---|---|---|
| `WAITGROUP` | attendere la fine di un gruppo di goroutine | §3 |
| `GO SELECT` | scegliere tra più operazioni su canali | §8 |
| `MUTEX` / `RWMUTEX` | esclusione mutua / lettori-scrittori | §9, §10, §11 |
| `DEFER` | esecuzione garantita a fine procedura | §10 |
| `ONCE` | eseguire qualcosa una sola volta | §12 |
| `ATOMIC` | operazioni atomiche su contatori | §13 |
| `CONTEXT` | cancellazione cooperativa propagata | §14 |
| `GOMAXPROCS` | numero di worker | §15 |
| `TRY/CATCH/FINALLY` | eccezioni e cleanup | §16 |
| `ON ERROR` | gestione errori classica | §17 |
| `GO EXIT` | uscita immediata della goroutine | §18 |
| `GOSUB` | salto con condivisione dei locals | §19 |
| `BYREF` | parametri per riferimento | §20 |
| `GO FUNCTION … INTO` | futuro asincrono | §21, §22 |
| Safepoint | calcoli CPU pesanti senza bloccare il worker | §23 |
| `SLEEP` | pausa e cedimento del worker | §24 |

## 3. Goroutine e WaitGroup

' Goroutine di base: GO SUB avvia un task concorrente, WAITGROUP attende.
IMPORT CONCURRENCY

SUB Saluta(Chi AS STRING, Risultati AS CHANNEL OF STRING)
  SEND "Ciao da " + Chi TO Risultati
END SUB

SUB Main()
  DIM Risultati AS CHANNEL OF STRING
  DIM W AS WAITGROUP
  DIM M AS STRING
  DIM N AS INTEGER

  GO SUB Saluta("Ada", Risultati) IN W
  GO SUB Saluta("Grace", Risultati) IN W
  GO SUB Saluta("Barbara", Risultati) IN W


## 4. Canali bloccanti (handshake)

' Canale bloccante unbuffered: SEND e RECEIVE si sincronizzano (handshake).
IMPORT CONCURRENCY

SUB Produttore(ch AS CHANNEL OF INTEGER)
  SEND 7 TO ch
END SUB

SUB Main()
  DIM ch AS CHANNEL OF INTEGER
  DIM v AS INTEGER
  GO SUB Produttore(ch)
  RECEIVE v FROM ch
  IF v = 7 THEN PRINT "CHANNEL-HANDSHAKE-OK" ELSE PRINT "FAIL:v="; v END IF
END SUB


## 5. Canali con buffer esplicito

' Canale con buffer esplicito: DIM ch AS CHANNEL OF INTEGER (n).
' Il producer puo' inviare fino a n valori senza che il consumer sia pronto.
IMPORT CONCURRENCY

SUB Main()
  DIM ch AS CHANNEL OF INTEGER (2)
  DIM v AS INTEGER
  DIM N AS INTEGER
  SEND 1 TO ch
  SEND 2 TO ch
  FOR N = 1 TO 2
    RECEIVE v FROM ch
    PRINT v
  NEXT N
  PRINT "CHANNEL-BUFFER-OK"
END SUB


## 6. Chiusura di un canale e flag `OK`

' CHANNEL CLOSE + RECEIVE ... OK: il flag ok diventa %CHCLOSED (0) quando il
' canale e' chiuso e svuotato. Pattern di shutdown go-idiomatico.
IMPORT CONCURRENCY

SUB Main()
  DIM jobs AS CHANNEL OF INTEGER (3)
  DIM v AS INTEGER
  DIM ok AS BIT
  DIM Tot AS LONG = 0
  SEND 1 TO jobs
  SEND 2 TO jobs
  SEND 3 TO jobs
  CHANNEL CLOSE jobs
  DO
    RECEIVE v FROM jobs OK ok
    IF ok = %CHCLOSED THEN EXIT DO


## 7. Canali direzionali
| Dichiarazione | Direzione consentita |
|---|---|
| `CHANNEL OF T` | sia invio sia ricezione |
| `CHANNEL SEND OF T` | solo `SEND` |
| `CHANNEL RECEIVE OF T` | solo `RECEIVE` |

' Canali direzionali: il tipo del parametro fissa la direzione.
' SEND su un canale RECEIVE-only (o viceversa) e' errore di compilazione.
IMPORT CONCURRENCY

SUB Produttore(outCh AS CHANNEL SEND OF INTEGER, W AS WAITGROUP)
  SEND 10 TO outCh
  SEND 20 TO outCh
  CHANNEL CLOSE outCh
END SUB

SUB Consumatore(inCh AS CHANNEL RECEIVE OF INTEGER, W AS WAITGROUP)
  DIM v AS INTEGER
  DIM Tot AS LONG = 0
  DIM ok AS BIT
  DO
    RECEIVE v FROM inCh OK ok


## 8. GO SELECT: scegliere tra più casi

### 8.1 Timeout

' GO SELECT con TIMEOUT: sceglie il case pronto, oppure scatta il timeout.
IMPORT CONCURRENCY

SUB Silenzioso(ch AS CHANNEL OF INTEGER)
  SLEEP 5000
END SUB

SUB Main()
  DIM ch AS CHANNEL OF INTEGER
  DIM v AS INTEGER
  GO SUB Silenzioso(ch)
  GO SELECT
    CASE RECEIVE v FROM ch:
      PRINT "FAIL:RECV-INESPERATO"
    CASE TIMEOUT 200:
      PRINT "SELECT-TIMEOUT-OK"


### 8.2 Canale nil (case disabilitato)

' Case su canale nil (mai assegnato): disabilitato, mai scelto.
IMPORT CONCURRENCY

SUB Produttore(pronto AS CHANNEL OF INTEGER)
  SLEEP 50
  SEND 42 TO pronto
END SUB

SUB Main()
  DIM pronto AS CHANNEL OF INTEGER (1)
  DIM nilCh AS CHANNEL OF INTEGER
  DIM v AS INTEGER
  DIM scelto AS INTEGER = 0
  GO SUB Produttore(pronto)
  GO SELECT
    CASE RECEIVE v FROM nilCh:


## 9. MUTEX: sezione critica

' MUTEX LOCK/UNLOCK: sezione critica esclusiva.
IMPORT CONCURRENCY

DIM Contatore AS INTEGER = 0

SUB Incrementa(mu AS MUTEX, W AS WAITGROUP)
  DIM v AS INTEGER
  MUTEX LOCK mu
  v = Contatore
  v = v + 1
  Contatore = v
  MUTEX UNLOCK mu
END SUB

SUB Main()
  DIM mu AS MUTEX


## 10. DEFER: rilascio garantito

' DEFER MUTEX UNLOCK: il rilascio e' garantito a fine procedura (parita' Go).
IMPORT CONCURRENCY

DIM Contatore AS INTEGER = 0

SUB Incrementa(mu AS MUTEX, W AS WAITGROUP)
  DEFER MUTEX UNLOCK mu
  MUTEX LOCK mu
  DIM v AS INTEGER
  v = Contatore
  v = v + 1
  Contatore = v
END SUB

SUB Main()
  DIM mu AS MUTEX


## 11. RWMUTEX: lettori condivisi, scrittore esclusivo
| Operazione | Significato |
|---|---|
| `MUTEX LOCK rw` / `MUTEX UNLOCK rw` | scrittura esclusiva |
| `MUTEX RLOCK rw` / `MUTEX RUNLOCK rw` | lettura condivisa |

' RWMUTEX: lettori condivisi (RLOCK), scrittore esclusivo (LOCK).
IMPORT CONCURRENCY

DIM Dati AS INTEGER = 0

SUB Scrittore(rw AS RWMUTEX, W AS WAITGROUP)
  MUTEX LOCK rw
  Dati = 99
  MUTEX UNLOCK rw
END SUB

SUB Lettore(rw AS RWMUTEX, W AS WAITGROUP, n AS INTEGER)
  MUTEX RLOCK rw
  PRINT "LETTO-"; n; "="; Dati
  MUTEX RUNLOCK rw
END SUB


## 12. ONCE: esegui una sola volta

' ONCE RUN: la procedura viene eseguita una sola volta anche da piu' goroutine.
IMPORT CONCURRENCY

DIM Counter AS LONG = 0

SUB Inizializza()
  Counter = Counter + 1
  SLEEP 1
END SUB

SUB Chiamante(o AS ONCE, W AS WAITGROUP)
  ONCE RUN o, Inizializza
END SUB

SUB Main()
  DIM o AS ONCE


## 13. ATOMIC: contatori senza mutex
| Funzione | Effetto |
|---|---|
| `ATOMIC.INCREMENT(c, n)` | aggiunge `n` al contatore |
| `ATOMIC.FETCHVALUE(c)` | legge il valore |
| `ATOMIC.STORE(c, v)` | scrive il valore |
| `ATOMIC.COMPARESWAP(c, atteso, nuovo)` | se il valore è `atteso`, lo porta a `nuovo`; restituisce 1/0 |

' ATOMIC: incrementi senza mutex, totali deterministici.
IMPORT CONCURRENCY
IMPORT ATOMIC

SUB Lavoratore(c AS ATOMIC, W AS WAITGROUP, n AS LONG)
  DIM i AS LONG
  FOR i = 1 TO n
    ATOMIC.INCREMENT(c, 1)
  NEXT i
END SUB

SUB Main()
  DIM c AS ATOMIC
  DIM W AS WAITGROUP
  DIM v AS LONG
  DIM sw AS BIT


## 14. CONTEXT: cancellazione propagata
| Funzione | Effetto |
|---|---|
| `CONTEXT.BACKGROUND()` | contesto radice, mai cancellato |
| `CONTEXT.WITHCANCEL(parent)` | contesto figlio cancellabile |
| `CONTEXT.DONE(ctx)` | canale che si chiude alla cancellazione |
| `CONTEXT.CANCELLED(ctx)` | 1 se il contesto è cancellato, 0 altrimenti |
| `CONTEXT.CANCEL(ctx)` | cancella il contesto |

' CONTEXT: cancellazione propagata ai task figli.
IMPORT CONCURRENCY
IMPORT CONTEXT

SUB Lavoratore(ctx AS CONTEXT, W AS WAITGROUP)
  DIM done AS CHANNEL
  DIM x AS INTEGER
  DIM ok AS BIT
  done = CONTEXT.DONE(ctx)
  RECEIVE x FROM done OK ok
  IF ok = 0 AND CONTEXT.CANCELLED(ctx) = 1 THEN
    PRINT "CONTEXT-OK"
  ELSE
    PRINT "FAIL:ok="; ok
  END IF
END SUB


## 15. GOMAXPROCS: ridimensionare i worker

' SYSTEM.GOMAXPROCS(n): ridimensiona i worker a runtime, nessun task perso.
IMPORT CONCURRENCY

SUB Lavoratore(n AS INTEGER, Ris AS CHANNEL OF INTEGER, W AS WAITGROUP)
  DIM i AS INTEGER
  DIM acc AS INTEGER = 0
  FOR i = 1 TO n
    acc = acc + i
  NEXT i
  SEND acc TO Ris
END SUB

SUB Main()
  DIM Ris AS CHANNEL OF INTEGER
  DIM W AS WAITGROUP
  DIM i AS INTEGER


## 16. TRY / CATCH / FINALLY

' TRY/CATCH/FINALLY: eccezioni e cleanup. FINALLY sospendibile (SLEEP).
IMPORT CONCURRENCY

SUB Main()
  DIM e AS ERR
  TRY
    PRINT "TRY-BODY"
    THROW 703
    PRINT "FAIL:DOPO-THROW"
  CATCH e AS ERR
    PRINT "CATCH-NUMERO="; e.Number
  FINALLY
    SLEEP 1
    PRINT "FINALLY-CLEANUP"
  END TRY
  PRINT "TRY-CATCH-FINALLY-DONE"


## 17. ON ERROR: gestione classica

' ON ERROR GOTO + RESUME NEXT: gestione errori classica.
IMPORT CONCURRENCY

SUB Main()
  DIM Passi AS INTEGER = 0
  ON ERROR GOTO Gestore
  ERROR 801, "primo"
  Passi = Passi + 1
  PRINT "DOPO-PRIMO"
  ERROR 802, "secondo"
  Passi = Passi + 1
  PRINT "DOPO-SECONDO"
  ON ERROR GOTO 0
  IF Passi = 2 THEN PRINT "ON-ERROR-OK" ELSE PRINT "FAIL:Passi="; Passi END IF
  END SUB



## 18. GO EXIT: esci dalla goroutine

' GO EXIT: termina la goroutine attraversando i FINALLY di tutti i frame.
IMPORT CONCURRENCY

SUB Profondo()
  TRY
    PRINT "BODY"
    GO EXIT
    PRINT "FAIL:DOPO-GOEXIT"
  FINALLY
    PRINT "FINALLY-INTERNO"
  END TRY
END SUB

SUB Lavoratore()
  TRY
    Profondo()


## 19. GOSUB e RETURN

' GOSUB/RETURN: salto a etichetta, i locals sono gli stessi del chiamante.
IMPORT CONCURRENCY

SUB Main()
  DIM valore AS LONG = 1
  GOSUB Aggiorna
  IF valore = 2 THEN PRINT "GOSUB-OK" ELSE PRINT "FAIL:valore="; valore END IF

Aggiorna:
  valore = valore + 1
  RETURN
END SUB


## 20. BYREF: parametri per riferimento

' BYREF: il parametro e' un riferimento alla variabile del chiamante.
IMPORT CONCURRENCY

SUB Raddoppia(BYREF x AS LONG)
  x = x * 2
END SUB

SUB Main()
  DIM v AS LONG = 21
  Raddoppia(v)
  IF v = 42 THEN PRINT "BYREF-OK" ELSE PRINT "FAIL:v="; v END IF
END SUB


## 21. Future: GO FUNCTION … INTO

' GO FUNCTION INTO: avvia una funzione in una goroutine e ne attende il valore.
IMPORT CONCURRENCY

FUNCTION Calcola() AS LONG
  SLEEP 10
  RETURN 42
END FUNCTION

SUB Main()
  DIM valore AS LONG
  GO FUNCTION Calcola() INTO valore
  IF valore = 42 THEN PRINT "FUTURE-OK" ELSE PRINT "FAIL:valore="; valore END IF
END SUB


## 22. Future fallito: l'errore si propaga

' Future fallito: l'errore della funzione si propaga all'ON ERROR del chiamante.
' Nota: il future va LETTO (qui nel PRINT) per attivare l'implicit-await;
' il LOAD_LOCAL vede la FailureException e propaga all'ON ERROR del chiamante.
IMPORT CONCURRENCY

FUNCTION Fallisce() AS LONG
  ERROR 951, "guasto"
  RETURN 0
END FUNCTION

SUB Main()
  ON ERROR GOTO Gestore
  DIM valore AS LONG
  GO FUNCTION Fallisce() INTO valore
  PRINT "FAIL:NON-PROPAGATO:"; valore
  ON ERROR GOTO 0


## 23. Safepoint: calcoli pesanti senza bloccare il worker

' Safepoint: un calcolo CPU pesante (KMEANS) si parcheggia e lascia
' progredire un'altra goroutine sullo stesso worker.
IMPORT CONCURRENCY
IMPORT MULTIVARIATE

SUB CalcoloPesante(W AS WAITGROUP)
  DIM data(99999) AS DOUBLE
  DIM i AS LONG
  DIM risultato AS OBJECT
  FOR i = 0 TO 99999
    IF i < 50000 THEN data(i) = 1 ELSE data(i) = 10
  NEXT i
  risultato = MULTIVARIATE.KMeans(data, 2)
  IF risultato.Centroids(0) > 0 THEN
    PRINT "KMEANS-RESULT-OK"
  ELSE


## 24. SLEEP: pausa e cedimento del worker

' SLEEP 0 cede il worker (parita' runtime.Gosched).
IMPORT CONCURRENCY

SUB Cede(Ris AS CHANNEL OF STRING)
  SLEEP 0
  SEND "A-DOPO-YIELD" TO Ris
END SUB

SUB Pronto(Ris AS CHANNEL OF STRING)
  SEND "B-GIRA" TO Ris
END SUB

SUB Main()
  DIM Ris AS CHANNEL OF STRING
  DIM W AS WAITGROUP
  DIM Primo AS STRING


## 25. Feature Go aggiuntive (completamento parità, 2026-08-27)

### 25.1 FOR EACH su canale (parità `for v := range ch`)

' FOR EACH v IN ch (parita' Go for v := range ch): itera finche' chiuso.
IMPORT CONCURRENCY

SUB Main()
  DIM ch AS CHANNEL OF INTEGER (3)
  DIM v AS INTEGER
  DIM Tot AS LONG = 0
  SEND 1 TO ch
  SEND 2 TO ch
  SEND 3 TO ch
  CHANNEL CLOSE ch
  FOR EACH v IN ch
    Tot = Tot + v
  NEXT
  IF Tot = 6 THEN PRINT "FOR-EACH-CH-OK" ELSE PRINT "FAIL:Tot="; Tot END IF
END SUB


### 25.2 GO SELECT con case SEND (parità `select { case ch <- v: }`)

' GO SELECT con CASE SEND (parita' Go select { case ch <- v: }).
IMPORT CONCURRENCY

SUB Receiver(ch AS CHANNEL OF INTEGER)
  DIM v AS INTEGER
  RECEIVE v FROM ch
  IF v = 42 THEN PRINT "RECEIVER-GOT-42"
END SUB

SUB Main()
  DIM ch AS CHANNEL OF INTEGER
  DIM scelto AS INTEGER = 0
  GO SUB Receiver(ch)
  GO SELECT
    CASE SEND 42 TO ch:
      PRINT "SELECT-SEND-OK"


### 25.3 ATOMIC.SWAP (parità `atomic.SwapInt64`)

' ATOMIC.SWAP: ritorna il valore precedente (parita' Go atomic.SwapInt64).
IMPORT CONCURRENCY
IMPORT ATOMIC

SUB Main()
  DIM c AS ATOMIC
  DIM prev AS LONG
  ATOMIC.STORE(c, 42)
  prev = ATOMIC.SWAP(c, 7)
  IF prev = 42 AND ATOMIC.FETCHVALUE(c) = 7 THEN
    PRINT "ATOMIC-SWAP-OK"
  ELSE
    PRINT "FAIL:prev="; prev
  END IF
END SUB


### 25.4 POOL (parità `sync.Pool`)

' POOL: riuso di oggetti (parita' Go sync.Pool: Get/Put).
IMPORT CONCURRENCY
IMPORT POOL

SUB Main()
  DIM p AS POOL
  DIM v AS STRING
  DIM w AS STRING
  POOL.PUT(p, "risorsa")
  v = POOL.GET(p)
  w = POOL.GET(p)
  IF v = "risorsa" AND w = "" THEN PRINT "POOL-OK" ELSE PRINT "FAIL:v="; v END IF
END SUB


### 25.5 COND (parità `sync.Cond`)

' COND: variabile di condizione (parita' Go sync.Cond: Wait/Signal).
IMPORT CONCURRENCY

DIM Ready AS BIT = 0

SUB Consumatore(c AS COND, mu AS MUTEX, W AS WAITGROUP)
  MUTEX LOCK mu
  WHILE Ready = 0
    COND WAIT c, mu
  WEND
  PRINT "CONSUMATORE-SVEGLIATO"
  MUTEX UNLOCK mu
END SUB

SUB Main()
  DIM c AS COND


### 25.6 SYNCMAP (parità `sync.Map`)

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


### 25.7 MUTEX.TRYLOCK (parità `mu.TryLock()`, Go 1.18)

' MUTEX.TRYLOCK: acquisizione non bloccante (parita' Go mu.TryLock(), Go 1.18).
IMPORT CONCURRENCY
IMPORT MUTEX

SUB Main()
  DIM mu AS MUTEX
  DIM ok AS BIT
  ok = MUTEX.TRYLOCK(mu)
  IF ok = 1 THEN PRINT "TRYLOCK-1-OK"
  ok = MUTEX.TRYLOCK(mu)
  IF ok = 0 THEN PRINT "TRYLOCK-BLOCCATO-OK"
  MUTEX UNLOCK mu
END SUB


### 25.8 SYSTEM.NUMGOROUTINE / NUMCPU (parità `runtime.NumGoroutine`/`NumCPU`)

' SYSTEM.NUMGOROUTINE / NUMCPU (parita' Go runtime.NumGoroutine/NumCPU).
IMPORT CONCURRENCY

SUB Lavoratore(W AS WAITGROUP)
  SLEEP 10
END SUB

SUB Main()
  DIM W AS WAITGROUP
  DIM ng AS LONG
  DIM nc AS LONG
  GO SUB Lavoratore(W) IN W
  GO SUB Lavoratore(W) IN W
  ng = SYSTEM.NUMGOROUTINE()
  nc = SYSTEM.NUMCPU()
  WAIT W


## 26. Riepilogo dei casi gestiti e dei sample canonici
| Caso | Esempio del manuale | Sample canonico (`shared/samples/Concurrency/`) |
|---|---|---|
| Goroutine + WaitGroup | §3 | `GoRoutinesWorkers.bas`, `GoRoutinesMinimal.bas` |
| Canali bloccanti | §4 | `ChannelBasics.bas`, `ChanTest.bas` |
| Buffer esplicito | §5 | `PCodeGpmChannelCapacity.bas` |
| Chiusura canale + OK | §6 | `PCodeGpmChannelClose.bas` |
| Canali direzionali | §7 | `PCodeGpmChannelDirection.bas` |
| SELECT timeout / nil / loser | §8 | `PCodeGpmSelectTimeout.bas`, `PCodeGpmSelectNil.bas`, `PCodeGpmSelectLoserCleanup.bas` |
| MUTEX | §9 | `PCodeGpmMutexDefer.bas` |
| DEFER | §10 | `PCodeGpmMutexDefer.bas` |
| RWMUTEX | §11 | `PCodeGpmRwMutex.bas` |
| ONCE | §12 | `PCodeGpmOnce.bas` |
| ATOMIC | §13 | `PCodeGpmAtomic.bas` |
| CONTEXT | §14 | `PCodeGpmContext.bas` |
| GOMAXPROCS resize | §15 | `PCodeGpmGoMaxProcsResize.bas`, `PCodeGpmGoMaxProcsResizeStress.bas` |
| TRY/CATCH/FINALLY | §16 | `PCodeGpmSuspendableFinally.bas`, `PCodeGpmExtremeUnwind.bas` |
| ON ERROR | §17 | `PCodeGpmOnErrorContinuations.bas`, `PCodeGpmOnErrorFramePropagation.bas` |
| GO EXIT | §18 | `PCodeGpmGoExitFinally.bas` |
| GOSUB | §19 | `PCodeGpmGosubFinally.bas` |
| BYREF | §20 | `PCodeGpmGosubFinally.bas`, `PCodeGpmFutureErrorFramePropagation.bas` |
| Future | §21 | `PCodeGpmFutureErrorFramePropagation.bas` |
| Future fallito | §22 | `PCodeGpmFutureErrorFramePropagation.bas` |
| Safepoint | §23 | `PCodeGpmAnalysisSafepoint.bas`, `PCodeGpmCpuBuiltinSafepoint.bas` |
| SLEEP / yield | §24 | `PCodeGpmSleepZeroYield.bas` |
| FOR EACH su canale | §25.1 | `PCodeGpmForEachChannel.bas` |
| SELECT case SEND | §25.2 | `PCodeGpmSelectSend.bas` |
| ATOMIC.SWAP | §25.3 | `PCodeGpmAtomicSwap.bas` |
| POOL | §25.4 | `PCodeGpmPool.bas` |
| COND | §25.5 | `PCodeGpmCond.bas` |
| SYNCMAP | §25.6 | `PCodeGpmSyncMap.bas` |
| MUTEX.TRYLOCK | §25.7 | `PCodeGpmTryLock.bas` |
| NUMGOROUTINE/NUMCPU | §25.8 | `PCodeGpmRuntimeIntrospection.bas` |

### Altri casi gestiti (stress e bordi)

## 27. Note finali e limiti noti

### ATOMIC-Guida-Utente

*Fonte: `ATOMIC-Guida-Utente`*


## 1. Panoramica
| Funzione | Ritorna | Parità Go |
|---|---|---|
| `ATOMIC.INCREMENT(c, n)` | il **NUOVO** valore dopo l'addizione | `atomic.AddInt64` |
| `ATOMIC.FETCHVALUE(c)` | il valore corrente | `atomic.LoadInt64` |
| `ATOMIC.STORE(c, n)` | 0 (nessun valore) | `atomic.StoreInt64` |
| `ATOMIC.COMPARESWAP(c, old, new)` | **1** se il CAS è riuscito, **0** altrimenti | `atomic.CompareAndSwapInt64` |
| `ATOMIC.SWAP(c, n)` | il valore **PRECEDENTE** | `atomic.SwapInt64` |

## 2. Dichiarazione

DIM c AS ATOMIC


## 3. Le 5 funzioni

### `ATOMIC.INCREMENT(c, n)`

DIM c AS ATOMIC
PRINT ATOMIC.INCREMENT(c, 5)     ' 5
PRINT ATOMIC.INCREMENT(c, 3)     ' 8
PRINT ATOMIC.FETCHVALUE(c)       ' 8


### `ATOMIC.FETCHVALUE(c)`

v = ATOMIC.FETCHVALUE(c)


### `ATOMIC.STORE(c, n)`

ATOMIC.STORE(c, 42)
PRINT ATOMIC.FETCHVALUE(c)       ' 42


### `ATOMIC.COMPARESWAP(c, old, new)`

sw = ATOMIC.COMPARESWAP(c, 42, 7)      ' 1 (riuscito, c ora = 7)
sw = ATOMIC.COMPARESWAP(c, 42, 99)     ' 0 (non riuscito, c resta 7)


### `ATOMIC.SWAP(c, new)`

ATOMIC.STORE(c, 42)
prev = ATOMIC.SWAP(c, 7)         ' prev = 42, c ora = 7


## 4. Concorrenza (GPM) — esempio con worker

IMPORT CONCURRENCY
IMPORT ATOMIC

SUB Worker(c AS ATOMIC, seed AS LONG)
    DIM i AS LONG
    FOR i = 1 TO seed
        ATOMIC.INCREMENT(c, i)
    NEXT i
END SUB

SUB Main()
    DIM c AS ATOMIC
    DIM wg AS WAITGROUP
    GO SUB Worker(c, 10) IN wg
    GO SUB Worker(c, 10) IN wg
    GO SUB Worker(c, 10) IN wg


## 5. Backend coverage
| Funzionalità | IL | PCodeVM | VM/C | C99Native |
|---|---|---|---|---|
| `INCREMENT` (ritorna nuovo) | ✅ | ✅ | ✅ | ✅ |
| `FETCHVALUE` | ✅ | ✅ | ✅ | ✅ |
| `STORE` | ✅ | ✅ | ✅ | ✅ |
| `COMPARESWAP` (1/0) | ✅ | ✅ | ✅ | ✅ |
| `SWAP` (ritorna precedente) | ✅ | ✅ | ✅ | ✅ |

### Fix di parità applicati (2026-09-19, blocco ATOMIC)

## 6. MiniTest e sample

### Concorrenza-GPM-Guida-Utente

*Fonte: `Concorrenza-GPM-Guida-Utente`*


## Task (goroutine)

### Helper asincroni (`AWAIT`)

ASYNC FUNCTION Lavoro() AS STRING
  AWAIT CONCURRENCY.SLEEPASYNC(20)   ' sospensione park-based (ms)
  AWAIT CONCURRENCY.TASKYIELD()      ' cede il turno (no attesa)
  RETURN "done"
END FUNCTION


## Sincronizzazione

### `WAIT` nuda (senza gruppo) — attende i `GO SUB` senza `IN`

GO SUB Scarica("a")
GO SUB Scarica("b")
WAIT                 ' attende a e b (non serve un gruppo)
PRINT "fatto"

| Forma | Parametri | Attende | Note |
|---|---|---|---|
| `GO SUB Nome(args)` | `Nome` SUB esistente; `args` argomenti posizionali | — (lancio) | figlio senza gruppo |
| `GO SUB Nome(args) IN wg` | + `wg` variabile `WAITGROUP` (dichiarata `DIM wg AS WAITGROUP`) | — (lancio) | figlio nel gruppo `wg` |
| `GO FUNCTION F(args) INTO v` | `F` FUNCTION; `v` variabile risultato (future) | — (lancio) | `IN` non ammesso su FUNCTION: solo gruppo implicito |
| `WAIT` (nuda) | nessuno | figli senza `IN` del task corrente | no-op se mai lanciati |
| `WAIT wg` | `wg` variabile `WAITGROUP` | gruppo `wg` | non attende i senza-`IN` |
| `WAIT n` | `n` numero | niente (no-op legacy) | per i ritardi usare `SLEEP ms` |

### `MUTEX.TRYLOCK(mu)` — acquisizione non bloccante

IMPORT CONCURRENCY
IMPORT MUTEX
DIM mu AS MUTEX
PRINT MUTEX.TRYLOCK(mu)   ' -1  (libero → acquisito)
PRINT MUTEX.TRYLOCK(mu)   '  0  (già tenuto → occupato)
MUTEX UNLOCK mu


## Mappa concorrente (`SYNCMAP`)

IMPORT CONCURRENCY
IMPORT SYNCMAP

DIM m AS CONCURRENTMAP
SYNCMAP.STORE(m, "k", "v")            ' scrive (sovrascrive)
PRINT SYNCMAP.LOAD(m, "k")            ' v   ("" se assente)
PRINT SYNCMAP.LOADORSTORE(m, "k", "x") ' v  (carica se esiste, altrimenti memorizza)
PRINT SYNCMAP.DELETE(m, "k")          ' v   (rimuove e ritorna il precedente)


## I/O DB pesante (SQL) e park

## Divergenze deliberate da Go (accettate, non bug)

## Regola d'oro (utente)

## Esempio

DIM ch AS CHANNEL
GO Produttore(ch)
GO Consumatore(ch)
SLEEP 1000


SUB Produttore(ch AS CHANNEL)
  CHANNEL PUBLISH ch, "dato"
END SUB


### DOEVENTS-Guida-Utente

*Fonte: `DOEVENTS-Guida-Utente`*


## 1. Sintassi

DOEVENTS


## 2. Parametri

## 3. Semantica

### Ordine e rientranza

## 4. Esempi

### Loop grafico

FUNCTION Main() AS INTEGER
  DIM i AS INTEGER
  SCREEN 12, 320, 200, 32
  FOR i = 1 TO 100
    PSET (i, 50), 14
    DOEVENTS
  NEXT i
  SCREEN 0
  RETURN 0
END FUNCTION


### Loop console

DIM i AS LONG
FOR i = 1 TO 100000
  IF (i && 1023) = 0 THEN DOEVENTS
NEXT i
PRINT "lavoro terminato"


### Con exit code

FUNCTION Main() AS INTEGER
  DOEVENTS
  PRINT "pronto"
  RETURN 0
END FUNCTION


## 5. Errori comuni
| Errore | Effetto | Correzione |
|---|---|---|
| `DOEVENTS()` | errore di sintassi: è un comando, non una funzione | togliere le parentesi |
| `DOEVENTS 10` | errore: non ha argomenti | usare `SLEEP 10` per la pausa |
| loop senza `DOEVENTS` | interfaccia congelata | inserirlo nel loop |
| aspettarsi un valore | `DOEVENTS` non ritorna nulla | usare una funzione/handler per il risultato |

## 6. Verifica di parità

tools/sh/check_basic_surface_parity.sh


### Defer-Guida-Utente

*Fonte: `Defer-Guida-Utente`*


## Panoramica

## Sintassi

SUB NomeProcedura(...)
    DEFER <statement>
    ...
END SUB


### Esempio — contatore LIFO

SUB Work()
    DEFER PRINT "defer-2"
    DEFER PRINT "defer-1"
    PRINT "work"
END SUB

SUB Main()
    Work()
    PRINT "main"
END SUB
' Output: work / defer-1 / defer-2 / main


### Esempio — cleanup garantito (mutex)

SUB Incrementer(mu AS MUTEX, wg AS WAITGROUP)
    DEFER MUTEX UNLOCK mu   ' garantito ANCHE se il corpo termina con un RETURN anticipato
    MUTEX LOCK mu
    Counter = Counter + 1
END SUB


### DEFER con valore di ritorno

FUNCTION Work() AS INTEGER
    DEFER PRINT "defer-2"
    DEFER PRINT "defer-1"
    PRINT "work"
    RETURN 99
END FUNCTION
' PRINT Work() → work / defer-1 / defer-2 / 99


## Limitazioni

## Come funziona (per backend)

## Parità Backend
| Backend | LIFO (SUB) | RETURN + valore attraverso i DEFER | Parità |
|---------|:-:|:-:|:-:|
| IL | ✓ (fix 2026-09-21) | ✓ (fix 2026-09-21) | 5/5 |
| PCodeVM | ✓ (fix 2026-09-21) | ✓ (fix 2026-09-21) | 5/5 |
| VM/C | ✓ | ✓ | 5/5 |
| C99Native | ✓ | ✓ | 5/5 |

## Storia (bug risolti)

## MiniTest

tools/sh/test_minitests_parity.sh constructs/statements_defer     # IL/PCode/VM-C (xUnit)
# [storico] gate C99Native rimosso il 2026-10-25 — oggi: tools/sh/test_minitests_c99native
GMB_MINI_ONLY="constructs/statements_defer" tools/sh/test_minitests_c99native


## Sample

## Riferimenti

### Http-FetchInto-Guida-Utente

*Fonte: `Http-FetchInto-Guida-Utente`*


## 1. A cosa serve

FETCH url[, opzioni] INTO Callback()


## 2. Sintassi

FETCH <url> [ , <opzioni> ] INTO <NomeSub> [ ( ) ]

| Parte | Obbligatoria | Descrizione |
|---|---|---|
| `url` | sì | L'URL della richiesta. Accetta una **espressione** qualsiasi (letterale o variabile). |
| `, <opzioni>` | no | Un `DICT` di opzioni (vedi §4). Se presente, va **prima** di `INTO`. |
| `INTO` | sì | Introduce il nome della SUB di callback. |
| `<NomeSub>` | sì | Nome della SUB (senza `PUBLIC`/`PRIVATE`: si chiama come qualsiasi SUB). |
| `( )` | no | Parentesi vuote ammesse per simmetria con le altre chiamate. |

## 3. La SUB di callback

SUB Nome(response$, error$)

| Parametro | Tipo | Contenuto |
|---|---|---|
| `response$` | STRING | Il corpo della risposta. Stringa **vuota** se la richiesta è fallita. |
| `error$` | STRING | Il testo dell'errore. Stringa **vuota** se la richiesta è riuscita. |

## 4. Opzioni (parametro `, <opzioni>`)
| Chiave | Tipo | Default | Significato |
|---|---|---|---|
| `Method` | STRING | `GET` | Metodo HTTP. |
| `Body` | STRING | — | Corpo della richiesta (con `Method` = POST/PUT). |
| `Timeout` | DOUBLE | `30000` | Timeout in millisecondi. |
| `UseCookies` | BIT | 0 | Gestione cookie. |
| `AllowRedirects` | BIT | 1 | Seguire i redirect. |
| `MaxRedirects` | DOUBLE | `10` | Tetto sui redirect. |
| `AllowInsecureSSL` | BIT | 0 | Accetta certificati non validi (solo sviluppo). |
| `BrowserPreset` | STRING | — | Profilo di browser (User-Agent e simili). |
| `PinnedCertificateSha256` | STRING | — | Pin del certificato del server. |
| `BearerToken` | STRING | — | Header `Authorization: Bearer …`. |
| `Username` / `Password` | STRING | — | Credenziali di base auth. |
| `Headers` | DICT | — | Header extra. Chiavi supportate: `ContentType`, `UserAgent`, `Authorization`. |

## 5. Semantica ed errori da conoscere

## 6. Esempio

IMPORT HTTP
IMPORT CONCURRENCY

DIM GMB_FETCH_URL AS STRING
DIM gError AS STRING
DIM gDone AS INTEGER

SUB OnFetchComplete(resp$, err$)
  gError = err$
  gDone = 1
END SUB

ASYNC SUB Starter(wg AS WAITGROUP)
  FETCH GMB_FETCH_URL INTO OnFetchComplete()
  DIM i AS INTEGER
  FOR i = 1 TO 400


## 7. Disponibilità sui backend
| Backend | Stato | Note |
|---|---|---|
| **IL** | ✅ | Callback eseguita come task GPM. |
| **P-Code** | ✅ | idem. |
| **VM/C** | ✅ | Il thread di fetch riceve `vm`, `callback_ip` e le globals del task, poi spawna la callback. |
| **C99Native** | ✅ | `case 255` di `GMB_STMT_NET_CMD` (`GMBasic/native/gmb_c99native/src/gmb_c99_emit.c`): thread + `gmb_task_spawn`. |
| **C99** | ❌ **rotta** | L'emissione del C è corretta, ma la callback **non viene eseguita** (`done` resta `0`). Bug aperto: `Bugs/(open) - C99Native-FetchInto-Callback-Non-Eseguita.md`. |

## 8. Riferimenti

## 9. Comandi correlati

<!-- END SURFACE -->
