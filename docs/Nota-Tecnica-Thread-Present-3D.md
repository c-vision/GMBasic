# Nota tecnica — thread e present del motore 3D

> **Prerequisito di**: **P2** (primo renderer, settori) e **P3** (mesh + target/present). Senza queste regole si scrive codice che funziona sul portatile di chi lo scrive e va in crash su macOS.
> **Stato**: le regole descrivono **ciò che il codice fa già** (present SDL, GPM, input) e ciò che il 3D deve aggiungere. Le funzioni marcate **DA AGGIUNGERE** non esistono.
> **Riferimenti nel codice**: `gmb_runtime_c/include/gmb_gfx_present.h`, `src/gmb_gfx_present_sdl.c`, `include/gmb_c99gpm.h`, `src/gmb_gfx_input.c`, `include/gmb_tui.h`.

---

## 1. Il contratto che c'è già

`gmb_gfx_present.h` lo dice in testa, e non è un'opinione:

> «SDL/Cocoa impone che TUTTE le chiamate a questo modulo (start/tick/stop) avvengano sul vero thread main del processo — mai da un task worker della VM GPM (crash nativo garantito su macOS altrimenti). Il chiamante deve eseguire la VM su un thread worker dedicato e pompare questo modulo in loop sul thread chiamante originale.»

Il precedente lato C# è lo stesso schema (`IlRuntime.RunGpmDriverWithGraphicsPump`). Quindi non stiamo inventando un modello: stiamo **estendendo** un modello già in produzione.

Nel codice:

| Fatto | Dove |
|---|---|
| `gmb_gfx_present_start/tick/stop`, `window_set_visible`, cursore, clipboard: **solo main thread** | `gmb_gfx_present.h`; guardia `gmb_gfx_present_on_main_thread()` |
| `gmb_gfx_present_tick()` fa **due cose insieme**: pompa gli eventi SDL e presenta la pagina | `gmb_gfx_present_sdl.c` (`while (SDL_PollEvent(...))` → `process_input_event`) |
| L'**input** è una coda scritta dal main thread e letta dal GPM (`gmb_gfx_input_poll_key/mouse`) | `gmb_gfx_present_sdl.c` («main thread = unico scrittore delle code input»), `gmb_gfx_input.c` |
| La finestra si crea **pigramente** al primo tick con pagina visibile | `ensure_window()` dentro `tick` |
| Il present è **limitato a ~30 fps** (33 ms) e **salta se la pagina non è cambiata** (hash a 32 bit sulle word) | `gmb_gfx_present_sdl.c` |
| `gmb_gfx_present_capture()` è disponibile **solo con video reale e su main thread**, altrimenti `-1` | `gmb_gfx_present_sdl.c` |
| Il GPM ha park su canali (`gmb_chan_send_or_park`, `gmb_select_park`), job su thread OS con `gmb_c99gpm_extjob_begin/end`, attese non bloccanti (`gmb_key_wait_park`) | `gmb_c99gpm.h` |

---

## 2. Attori e thread

| Thread | Chi è | Cosa può fare | Cosa **non** può fare |
|---|---|---|---|
| **T0 — main/UI** | il thread che ha chiamato `gmb_gfx_present_start` | tutto il modulo present: finestra, eventi SDL, present, cursore, clipboard, `capture` | eseguire BASIC; toccare lo stato di `SCENE`/`WORLD`; renderizzare |
| **T1 — worker GPM** (1…8) | i thread OS avviati da `gmb_gpm_start` (`GOMAXPROCS`, default `min(ncpu, 8)`); eseguono i task BASIC, e un task **può migrare** da un worker all'altro fra due park | eseguire BASIC, chiamare `RENDER`, mutare lo stato 3D, leggere l'input (`gmb_gfx_input_poll_*`) | chiamare qualunque funzione del modulo present (start/tick/stop/visible/cursore/clipboard/capture) |
| **T2 — job OS** | thread per rete/SQL/DNS (`pthread_create` in `gmb_network.c`, `gmb_c99gpm.c`) | lavoro esterno, poi risolvere future e svegliare il GPM (`extjob_begin/end`) | eseguire BASIC; toccare lo stato 3D |
| **T3 — test/headless** | il processo di MiniTest o la cattura offline | leggere memoria, generare golden | contare su una finestra |

**Regola d'oro**: *un oggetto appartiene al task che lo esegue*. Lo stato 3D appartiene a un **task GPM**; la finestra appartiene a **T0**. Nessun oggetto è condiviso in scrittura.

**Correzione del 2026-11-19 — «il GPM» non è un thread** (misurato nel codice, non dedotto): `gmb_gpm_start` (`gmb_c99gpm.c`) avvia `nw = min(ncpu, 8)` worker, `gmb_gomaxprocs(n)` ne aggiunge altri, e i task **migrano** fra worker (`gmb_console.c` lo dà per assodato: buffer di stampa `__thread` perché «i task GPM possono girare su thread diversi»). Due conseguenze che valgono più di questa nota:

1. Un'identità **di thread** del GPM non esiste e non va inventata: l'identità utile è **del task**, e c'è già — `gmb_cur()` (`__thread`, dichiarata `noinline` perché il compilatore cachea la variabile TLS e dopo il resume ripristinerebbe un valore stantio).
2. Due task possono toccare lo stato 3D **in parallelo** su due worker. Quindi `R3` non è una regola di thread ma di **proprietà**: un solo task proprietario alla volta, e il secondo riceve un errore (non un'attesa: un lock bloccherebbe un worker e renderebbe il frame non deterministico).

**La catena degli eventi è già quella giusta** (verificato nel codice il 2026-11-19, non dedotto): `gmb_gfx_present_tick` sul main thread **non chiama mai BASIC** — traduce gli eventi SDL in `gmb_gfx_input_push_*`; il consumatore è `gmb_form_poll()`, che gira **sul task GPM** ed è l'unico lettore delle code (`gmb_form.h`: «lock-free 1-scrittore main / 1-lettore worker», «il worker NON tocca SDL», callback «invocate INLINE sul worker»). Il 3D non deve aggiungere una seconda via di dispatch: il bug del 2026-08-28 (WebSocket che invocava `OnMessage` sul thread pool, commit `faa0ae27`) è esattamente la classe da non ripetere. Conseguenza per `RENDER`: non serve alcun bridge, perché il frame si disegna **dentro il task** che ha già drenato gli eventi.

---

## 3. Invarianti (numerati, citabili nelle review)

| ID | Invariante | Come è garantito / da garantire |
|---|---|---|
| `R1` | **BASIC gira solo dentro un task GPM.** Nessun handler, callback, hook o job esegue codice BASIC fuori: si accoda un task GPM. | Test: `gmb_cur() != NULL` (`gmb_gpm_in_task()`); regola di progetto (AGENTS §0ter) + `gmb_task_spawn`. |
| `R2` | **SDL gira solo su T0.** Nessuna chiamata al modulo present da T1/T2. | Guardia esistente `gmb_gfx_present_on_main_thread()`; per il 3D: nessuna inclusione di header SDL in `gmb_3d_*.c`. |
| `R3` | **Lo stato 3D appartiene a un task GPM alla volta.** Toccare lo stato (comandi o `RENDER`) fuori da un task è un errore; un secondo task che entra mentre uno renderizza riceve `RENDER: un altro task sta renderizzando`, **mai un'attesa**. La pubblicazione del frame è l'unico passaggio verso T0, per **puntatore + contatore atomico**. **Claim unico atomico (P1, 2026-11-19)**: un solo word in `gmb_3d_frame.c` (0 = libero, sentinella = nessun task/T3, altrimenti il task proprietario), così acquisizione, chiusura e recupero non hanno finestre intermedie. Riferimenti GPM **forti**: un binario che non linka lo scheduler non si costruisce — la guardia non può tornare inerte in silenzio. `RENDER END` da un non-proprietario non tocca il claim di un task **vivo**; se il proprietario è **finito senza `END`** (o lo scheduler è spento) libera il claim: il renderer non resta mai occupato per sempre. `gmb_3d_frame_publish()`/`frame_acquire()` restano **DA AGGIUNGERE** (P2). `gmb_gpm_in_task()` è un macro sull'esistente `gmb_cur()`, non un simbolo nuovo. |
| `R4` | **`RENDER` è sincrono**: nessuna attesa, nessun I/O, nessuna chiamata SDL, **nessuna allocazione per frame**. | Contatore di allocazioni in `SCENE.STATS("allocs")` nei test. |
| `R5` | **`RENDER` non è ricorsivo**: una chiamata dentro un'altra è un errore, non un annidamento. | Guardia `gmb_3d_render_depth`; errore `RENDER: chiamata ricorsiva`. |
| `R6` | **`RENDER` è ammesso dagli handler** (stesso task), ma è sconsigliato: rende uno stato a metà aggiornamento e raddoppia il costo. | Contatore `renders` per frame in `SCENE.STATS`; documentato in guida. |
| `R7` | **Il resize della finestra non deve riallocare la pagina sotto al renderer**: T0 registra la richiesta, T1 la applica a inizio frame. | **DA AGGIUNGERE**: `gmb_gfx_screen_request_resize(w,h)` (T0) + `gmb_gfx_screen_apply_pending_resize()` (T1, inizio frame). Oggi `tick` chiama `gmb_gfx_resize_screen` **direttamente**: pericolo reale, non teorico. |
| `R8` | **Il present non è un requisito del renderer**: se T0 non c'è (headless), `RENDER` funziona identico. | Null mode; nessun `#ifdef` SDL nel modulo 3D. |
| `R9` | **Nessun ambiente**: il cap del present, la scala, la golden mode sono **opzioni del registro**, non `getenv`. | Opzioni `engine.present.max_fps`, `engine.3d.scale`. |
| `R10` | **Il ciclo di gioco è park-based, come `gmb_form_frame`**: input non bloccante (`gmb_form_poll` / `gmb_gfx_input_poll_*`) e poi **park** (`gmb_yield_ticks`); niente busy-spin, **niente `gmb_sleep_ms` dentro un task**, nessuna attesa del present. Un eventuale `VSYNC`/`WAIT` del 3D parca, non addormenta il worker. | Contratto già in produzione (`gmb_form.h`, `gmb_form_frame`); `grep gmb_sleep_ms` sui nuovi file = 0; contatore di park > 0 in un ciclo di prova. |
| `R11` | **Ciò che aspetta una risorsa esterna parca il task** (`WORLD LOAD`/`SAVE`, `SCENE LOAD`, texture, modelli) con `gmb_c99gpm_extjob_begin/end`, e **ripiega sul sincrono quando `gmb_cur()==NULL`** (tool nativi, oracolo P1, selfcheck C). La validazione `W-INV-*` gira **nel task dopo il resume**: un mondo invalido è un errore BASIC normale (`ON ERROR`, `F-ERRORS`) e **lo stato resta quello di prima** (load atomico). | Stesso schema di `gmb_form_frame` (`gmb_cur()` → park, altrimenti `gmb_sleep_ms`); test: load invalido → errore nominato e query successive identiche a prima. |

---

## 4. Il ciclo di un frame

```
T0 (main, ~1 ms per giro)                          T1 (GPM worker)
─────────────────────────────────────────────      ────────────────────────────────────────
gmb_gfx_present_tick()
 ├─ SDL_PollEvent × N  →  code input  ──────────►  il programma polla l'input fra due istruzioni
 ├─ resize? → registra richiesta (R7)
 ├─ (frame pubblicato nuovo?)  ◄────────────────  RENDER: disegna nel target di T1
 │      sì → UpdateTexture + RenderCopy                └─ gmb_3d_frame_publish(seq)
 │      no → salta il blit (hash/seq)
 └─ SDL_RenderPresent
```

Il frame vive **una volta sola**, in un buffer di proprietà del 3D. T0 non disegna: copia. T1 non presenta: pubblica.

---

## 5. `RENDER`: sincrono, e perché non è park

**Decisione: sincrono.** Il render è CPU pura, limitata e deterministica: si esegue nel turno del GPM, come qualunque altra istruzione.

Perché non park:

1. **Determinismo**: con il park, fra la richiesta e il completamento altre goroutine BASIC potrebbero mutare la scena. Il frame diventerebbe una miscela di due stati e le golden image non sarebbero riproducibili (`T-FIXED` della specifica).
2. **Latenza**: park = almeno un frame di ritardo, e un meccanismo in più da sincronizzare per un'operazione che dura ~1–4 ms.
3. **Costo misurato**: il riferimento dichiara **0,5 ms** a 320×150 e **2 ms** a 640×300 per i settori, **~1,6 ms** a 320×150 per le mesh. Su un budget di 16,7 ms è lavoro, non attesa.
4. **Nessuna risorsa esterna**: il render non aspetta disco, rete o vsync. Non c'è niente da parkare.

**Il park serve altrove**, e la tabella dice dove:

| Operazione | Modello | Motivo |
|---|---|---|
| `RENDER` | **sincrono su T1** | CPU pura, determinismo |
| hit-test / collisioni / raggi | sincrono su T1 | CPU pura, stato locale |
| caricamento texture/modelli da file | **park** (`gmb_c99gpm_extjob_*`) | I/O, non deve bloccare il worker |
| audio posizionale | sincrono su T1 (mixer), consegna al device fuori | il mixer non aspetta |
| present | **T0**, mai su T1 | vincolo SDL |
| hardware GL (`PH`) | **park verso T0** (command list) | il contesto GL appartiene a T0 — §9 |

Regole dentro `RENDER` (R4): niente `malloc` per frame (i buffer si allocano al primo uso e si riusano), niente `printf` di debug, niente `SDL_*`, niente chiamate al filesystem. Se una texture manca, si disegna il colore piatto (`C-NOPIC`) e si annota in `SCENE.STATS`, non si carica un file dentro il frame.

---

## 6. `RENDER` dentro un handler: cosa succede davvero

Gli handler (`ON KEY`, `ON MOUSE`, `ON COLLISION`, …) girano **sul GPM**, fra due istruzioni: stesso task di `RENDER`. Quindi **non c'è rischio di crash, deadlock o violazione di thread**. Ci sono però tre conseguenze da mettere per iscritto:

1. **Semantica**: il frame reso dentro un handler è lo stato *a metà* dell'aggiornamento. Se il programma renderizza anche a fine ciclo, il frame presentato può non corrispondere a nessuno stato coerente del gioco.
2. **Costo**: un handler che renderizza a ogni evento (mouse move = centinaia di eventi/s) trasforma un frame in centinaia. Non è un errore: è un programma che va 100× più lento.
3. **Ricorsione**: `RENDER` dentro `RENDER` (es. un callback chiamato dal renderer) corromperebbe target e z-buffer a metà scrittura. **Vietato** (R5): errore esplicito `RENDER: chiamata ricorsiva`, non un no-op silenzioso.

**Regola consigliata, e documentata in guida**: negli handler si **cambia stato** e si imposta un flag; `RENDER` si chiama **una volta per frame** nel ciclo principale. Per accorgersi delle violazioni: `SCENE.STATS("renders")` conta le chiamate nell'ultimo frame e `SCENE.STATS("renders_max")` il massimo osservato; un valore > 1 in un gioco che non lo vuole è un bug del programma, non del motore. Il frame lo chiude **un punto solo**, `gmb_scene_present_done()`: `gmb_scene_present()` lo fa da sé, le gambe a due immagini (IL/PCode, VM/C) lo chiamano dopo la copia riuscita — è l'unica parte di `PRESENT` che il modulo 3D non può dedurre dal target.

**Cosa non facciamo**: nessun auto-render (il motore non disegna da solo a fine frame), nessun render differito implicito, nessuna coda di richieste di render. Un comportamento implicito qui renderebbe impossibile capire chi ha disegnato cosa.

---

## 7. Headless e null mode (`P1`)

- Senza finestra non esiste T0: il processo di test è **T1** (o un thread qualsiasi) e legge la memoria.
- `RENDER` in null mode: aggiorna lo stato, non disegna. Serve a testare logica, collisioni, raggi e tempo a costo zero.
- **Attenzione, correzione al piano**: `gmb_gfx_present_capture()` **non è utilizzabile in headless** — restituisce `-1` se non c'è video reale o non si è su T0. Le golden image devono leggere il **target 3D** (o `gmb_gfx_visible_pixels()` per il 2D), non la capture della finestra.
- **La cattura dal target esiste dal 2026-11-19 (P1)**: `gmb_3d_target_capture_argb` / `gmb_3d_target_capture_png` in `src/gmb_3d_frame.c`. È una copia (nessuna scalatura), quindi funziona identica con e senza finestra (`R8`), e le guardie del modulo degradano a «nessun task» solo quando lo scheduler non è **attivo** (T3: tool e oracolo) — il GPM è riferito **forte**, quindi un binario senza scheduler non si costruisce e la guardia non può diventare inerte.
- La cattura su T0 serve solo a verificare che il present mostri il frame atteso (test visivo), non come sorgente delle golden.

---

## 8. Present: cap, hash, tearing

Il `tick` attuale:

* pompa gli eventi **ogni giro** (~1 kHz): l'input resta reattivo anche se il blit è limitato;
* limita il blit a **~30 fps** (33 ms, costante nel codice);
* **salta il blit** se la pagina non è cambiata (hash a 32 bit sulle word).

Per il 3D:

1. **Il cap: una sola opzione, `engine.present.max_fps`, default `auto`** (deciso il 2026-11-19). `auto` = **60** appena esiste un renderer 3D (`SCENE`/`WORLD`), **30** altrimenti: il 2D resta identico a oggi (nessuna regressione) e non serve una seconda opzione "legata al renderer" che dica la stessa cosa. Valori `1…240` lo sovrascrivono; `0` = nessun cap, per le misure, non per i giochi. Il cap limita **il blit**, non il pump degli eventi né `RENDER`: un programma che disegna a 120 fps con cap 60 presenta un frame sì e uno no, **senza coda e senza crescita di memoria**, perché il frame pubblicato è sempre l'ultimo (`seq`). Niente env (R9). **Come T0 sa che il 3D esiste senza toccarne lo stato** (vincolo `R2`/`R3`): non lo chiede al modulo 3D — è il task proprietario a pubblicare un flag atomico (`gmb_3d_active`) insieme al frame, e T0 legge **solo** quell'atomico e l'ultimo frame pubblicato. Se T0 interrogasse `SCENE`/`WORLD`, leggerebbe stato di proprietà di un task da un thread che non lo possiede: violazione di `R3`, e con la migrazione dei task anche lettura senza sincronizzazione. Stessa regola per l'opzione: il valore effettivo del cap che T0 usa è una **copia atomica** pubblicata dal task quando l'opzione cambia (`gmb_present_max_fps`), non una lettura del registro opzioni dal main thread nel ciclo caldo — il registro è condiviso con i task e non è un canale di comunicazione fra thread.
2. **L'hash va saltato quando il frame pubblicato non è cambiato**: con `seq` basta confrontare un intero, invece di rileggere l'intera pagina. Il risparmio dichiarato nel codice (17 MB × 60/s) con un frame 3D a 640×300 (192k pixel = 768 KB) resta sensato, ma il confronto su `seq` è gratis.
3. **Tearing residuo, dichiarato**: T0 copia la pagina mentre T1 può scriverla. È un comportamento **già presente nel 2D**; per il 3D significa che un frame può mostrare una miscela di due frame (non corruzione: scritture di word allineate). L'escalation è il **triple buffer** (tre target, swap atomico degli indici) — descritto qui, **non implementato**: **deciso il 2026-11-19** che si implementa solo se la misura qui sotto lo giustifica.

### 8.3 Misura del tearing (definita il 2026-11-19, eseguita in P3)

Chi: `tools/sh/check_3d_present.sh`, scena `shared/minitests/3d/present_tear.bas`.

* **Come**: T1 alterna due frame che qualunque miscela rende riconoscibile — frame A tutto `0x00100000`, frame B tutto `0x00001000` (un canale per frame) — per **600 frame** consecutivi al cap; T0, subito dopo il blit, cattura il frame presentato con `gmb_gfx_present_capture()` e conta le word che non sono né A né B.
* **Soglia**: media **> 0,5 %** dei pixel sui 600 frame, oppure **un singolo frame > 5 %**.
* **Senza video reale o fuori da T0** la cattura ritorna `-1` (§7): la misura si dichiara **SKIP**, mai verde.
* **Perché il triple buffer e non il double**: col double, se T1 finisce il back buffer mentre T0 sta ancora blittando, dovrebbe **aspettare** — e `R4` vieta di aspettare dentro il render. Col triple, T1 scrive il terzo buffer e non tocca né quello in lettura né quello in attesa; T0 scambia `read_idx` e **rilascia** il buffer solo a blit finito. Tre target a 640×300 costano 2,3 MB in più: il costo è la memoria e la complessità dello swap, non la CPU. **Nessuna attesa nemmeno col triple**: lo swap è solo atomi e se T0 tiene ancora in lettura il buffer che servirebbe, il task continua a scrivere il proprio e il frame non presentato si perde — nessuna coda, nessun worker fermo.
* **Compatibilità GPM della misura**: la scena che alterna i frame è un **task GPM** che rende e poi **parka** (`gmb_yield_ticks`); in busy-spin occuperebbe un worker e falserebbe il conteggio. T1 **non attende mai** il blit (altrimenti si misura la coda, non il tearing); T0 cattura dopo il blit e non chiama BASIC; il conteggio dei pixel misti resta nel task.

---

## 9. Hardware (`PH`) e il vincolo GL

Se un giorno il renderer hardware usa OpenGL/Metal, il contesto grafico appartiene al thread che possiede la finestra (**T0**). Due strade:

| Opzione | Come | Costo | Rischio |
|---|---|---|---|
| **(a) command list + park** *(raccomandata)* | T1 costruisce la lista (vertici, materiali, ordine) e si parka; T0 esegue le chiamate GL e presenta; T1 riprende | una copia dei dati per frame, latenza 1 frame | nessun vincolo di thread violato |
| (b) contesto GL su T1 | contesto separato creato su T1 | zero copie | su macOS il contesto fuori dal main thread è fragile; il riferimento stesso tiene il contesto sul thread del programma **solo** perché il loro programma gira sul thread della finestra |

Raccomandazione: **(a)**, con la lista costruita in memoria riusata (niente alloc per frame). Decisione finale in `PH`, non prima: il path software non ne ha bisogno.

---

## 10. Guardie e diagnostica da aggiungere

| Funzione | Ruolo | Comportamento |
|---|---|---|
| `gmb_3d_frame_publish(void)` | T1 → T0 | scrive `seq` atomico e lo indice del buffer pubblicato |
| `gmb_3d_frame_acquire(const void**, int* w, int* h, uint32_t* seq)` | T0 | legge l'ultimo frame pubblico senza bloccare; `NULL` se nessuno |
| `gmb_3d_render_depth` + errore `RENDER: chiamata ricorsiva` | R5 | errore, non no-op |
| `gmb_gpm_in_task()` | R1/R3 | **Nessun simbolo nuovo**: `#define gmb_gpm_in_task() (gmb_cur() != NULL)` in `gmb_c99gpm.h`, sopra l'esistente `gmb_cur()` (`__thread`, `noinline`: il commento nel file spiega la trappola TLS + cache dopo il resume). Serve a rifiutare `RENDER`/mutazioni fuori da un task (hook, main thread, job T2). **Un tid sarebbe sbagliato**: il GPM gira su 1…8 worker e i task migrano |
| Claim del renderer (`g_render_claim`) + `gmb_task_finished()` + errore `RENDER: un altro task sta renderizzando` | R3 | **FATTO in P1** (2026-11-19): un solo word atomico (0 / sentinella T3 / task), niente `busy` + `owner` separati; liveness del proprietario via `gmb_task_finished()` (nuova API GPM in `gmb_c99gpm.h`) per liberare un claim orfano; al secondo task si risponde con l'errore, non con un'attesa (§3) |
| `gmb_gfx_screen_request_resize(w,h)` / `apply_pending_resize()` | R7 | il resize non rialloca sotto al renderer |
| `SCENE.STATS("renders" \| "renders_max" \| "allocs")` | R4/R6 | misure per frame, non impressioni. Nessun `frame_ms`: il tempo lo misura fuori il banco (`bench_3d.sh`), §0septies |
| `gmb_3d_active` (atomico, pubblicato dal task) | R2/R3 + §8.1 | T0 sa se c'è un renderer 3D senza leggere stato di proprietà di un task; decide il default `auto` del cap |
| `gmb_present_max_fps` (copia atomica del cap) | R2/R9 + §8.1 | T0 non legge il registro opzioni nel ciclo caldo: il task pubblica il valore quando l'opzione cambia |
| `gmb_3d_io_load()` con ripiego su `gmb_cur()==NULL` | R11 | park dentro un task, sincrono fuori (tool `world_check`, oracolo P1, selfcheck C) |
| coda eventi audio → task GPM (`SCENE SOUND`, P6) | R1 | il callback SDL audio **non** esegue BASIC: accoda, il dispatch avviene nel task |

Errori con testo esplicito, mai silenziosi: `RENDER fuori dal task GPM`, `RENDER: chiamata ricorsiva`, `RENDER: nessun target`, `present: chiamata fuori dal main thread` (quest'ultimo già impossibile per costruzione, ma va asserito nei test).

---

## 11. Invarianti verificabili (checklist per P2/P3)

1. `R2`: un test compila i moduli `gmb_3d_*.c` **senza** includere header SDL (grep + link senza SDL).
2. `R4`: 1000 frame di una scena fissa → `allocs` per frame = 0 (contatore, non impressione).
3. `R5`: `RENDER` chiamato dentro un callback di test → errore atteso, nessuna scrittura parziale (target invariato).
4. `R7`: richiesta di resize durante un frame → nessun crash, il frame successivo ha la nuova dimensione.
5. `R8`: stessa scena in null mode e con finestra → **stesso** contenuto nel target (il present non altera i pixel).
6. `R6`: handler che chiama `RENDER` 10 volte → `renders_max` = 10, nessun crash, frame coerente con l'ultima chiamata.
7. `R9`: `grep getenv` sui file nuovi = 0.
8. `R10`: un ciclo di gioco di prova non contiene `gmb_sleep_ms` né spin (grep) e il task **cede** il worker (contatore di park > 0).
9. `R11`: `LOAD` di un mondo invalido → errore nominato e stato precedente intatto; lo stesso codice eseguito dal tool `world_check` **senza** scheduler (`gmb_gpm_start` mai chiamato).

---

## 12. Cosa cambia nelle fasi

- **P1 (oracolo)**: nessun T0. Golden lette dal target; **non** da `present_capture`. La nota serve solo per `R8`.
- **P2 (settori)**: servono `R3`, `R4`, `R5`, `R7`, `R10`, `R11` (il `LOAD` dei mondi arriva qui). Il frame si pubblica a fine `RENDER`. `R3` è nella forma corretta dal 2026-11-19: **proprietà del task**, non del thread (il GPM è multi-worker); il caso "due task" si rifiuta con l'errore, la ricorsione nello stesso task la copre `R5`.
- **P3 (mesh + target/present)**: si aggiungono il consumo su T0 (`frame_acquire`), il cap — **deciso** (§8: una sola opzione, default `auto`) — e il formato del buffer pubblicato, **deciso**: `0x00RRGGBB` con alpha 0, come le pagine 2D (specifica §11). Il frame pubblicato si copia nella pagina con `& 0xFFFFFF`. Il tearing si **misura** (§8.3) prima di parlare di triple buffer.
- **`PH` (hardware)**: si decide (a)/(b) del §9.

---

## 13. Domande chiuse (2026-11-19)

> Erano le tre domande che P2/P3 ereditavano. Restano qui perché questa nota è la sede del thread/present; nel piano sono richiamate in §3.1 e §12.

1. **Cap del present — una sola opzione `engine.present.max_fps`, default `auto`** (60 con un renderer 3D, 30 senza). Motivo: una cosa sola da configurare; il "legato al renderer" si ottiene da un fatto che il runtime conosce (`SCENE`/`WORLD` esistono), non da una seconda opzione; il 2D resta a 30, quindi nessuna regressione. Conseguenza dichiarata: il cap limita il blit, non `RENDER` (§8).
2. **Triple buffer — non ora, e solo dietro misura.** La misura è definita in §8.3: due frame a canale alternato, 600 frame, conteggio dei pixel misti; soglia 0,5 % di media o 5 % su un frame; `SKIP` dichiarato senza video. Il double non basta perché T1 dovrebbe aspettare, cosa che `R4` vieta.
3. **Identità del thread GPM — non esiste e non va aggiunta.** `gmb_gpm_start` avvia `min(ncpu, 8)` worker e i task migrano; l'identità utile è **del task** e c'è già (`gmb_cur()`). Si aggiungono solo il macro `gmb_gpm_in_task()` e la proprietà `gmb_3d_render_owner` per il caso di due task in parallelo (§3, §10).

---

## 14. Compatibilità GPM: cosa ogni decisione deve rispettare (2026-11-19)

> Richiesta esplicita: **tutto il motore 3D deve essere compatibile GPM**. Le sei decisioni chiuse restano quelle che sono; qui c'è l'obbligazione GPM che ognuna porta con sé, con il motivo. Il metro è AGENTS §0ter: BASIC solo dentro un task, park per ciò che aspetta, eventi dispatchati dal GPM, nessuna scorciatoia su un thread esterno.

| Decisione | Obbligazione GPM | Motivo |
|---|---|---|
| **Schema JSON unico** (specifica §12.1) | `WORLD LOAD`/`SAVE`, `SCENE LOAD`, caricamento texture: **park** (`gmb_c99gpm_extjob_begin/end`), con ripiego sincrono se `gmb_cur()==NULL`; la validazione `W-INV-*` gira **nel task** dopo il resume, quindi `ON ERROR` la vede come un errore normale | il file è I/O: un worker che legge un file blocca gli altri task, e un errore emesso su un job T2 non arriverebbe mai a `ON ERROR` |
| **`WORLD CHECK` = tool** (specifica §12.2) | `tools/c/world_check.c` è **T3**: non avvia `gmb_gpm_start`, non esegue BASIC, non richiede un task; `gmb_3d_io.c`/`gmb_3d_check.c` devono essere linkabili e usabili **fuori** dal GPM | il validatore serve alla CI, dove scheduler e finestra non esistono; se il load pretendesse un task, il tool non potrebbe esistere |
| **`SEES` 1/0/−1** (specifica §12.3) | nessuna: funzione pura sullo stato, eseguita nel task che possiede lo stato | nessun I/O, nessuna risorsa esterna |
| **Cap `engine.present.max_fps` = `auto`** (§8.1) | T0 **non interroga** `SCENE`/`WORLD`: il "3D attivo" è un flag atomico pubblicato dal task (`gmb_3d_active`); il cap limita solo il blit e **non crea backpressure** — se il blit è saltato non si accumula nulla e T1 non attende | leggere lo stato 3D da T0 sarebbe la violazione di `R2`/`R3`: stato di un task letto da un thread che non lo possiede |
| **Triple buffer solo dietro misura** (§8.3) | swap solo con **atomi**; T1 non attende **mai** un buffer: se T0 tiene ancora in lettura quello che servirebbe, il task continua a scrivere il proprio e il frame non presentato si perde (nessuna coda); la scena di misura è un task che **parka** fra i frame | un mutex o un'attesa sul buffer farebbe aspettare un worker per colpa di T0: `R4` lo vieta, e la coda falserebbe la misura |
| **Identità del task** (§3, §13) | `gmb_gpm_in_task()` = `gmb_cur() != NULL`; comandi e `RENDER` fuori da un task sono un **errore esplicito**; `R3` = proprietà con rifiuto, mai lock | è la forma già in produzione (`gmb_form.h`): il worker è l'unico lettore dell'input, le callback girano inline sul task, SDL resta su T0 |

**Le tre regole che valgono per tutto il codice 3D, senza eccezioni.**

1. **Nessuna seconda via di dispatch.** Gli eventi entrano in coda su T0 e vengono drenati dal task (`gmb_form_poll`); il 3D non aggiunge callback invocate da T0/T2. Precedente da non ripetere: il WebSocket che invocava `OnMessage` sul thread pool (2026-08-28, `faa0ae27`).
2. **Niente attese dentro un task.** `gmb_yield_ticks`/park sì; `gmb_sleep_ms`, spin e attese del present no (`R10`).
3. **Ciò che aspetta una risorsa esterna parca** (file, modelli, decode texture) e riprende nel task (`R11`).
