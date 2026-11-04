# IV.21 — Grafica, suono e giochi

## Cosa ottieni da questo capitolo

Disegnare a schermo, animare a frame rate costante, suonare, e la struttura di un gioco che non
consuma la CPU. Il capitolo è breve perché il modello è piccolo: `SCREEN`, `DRAW`, `TICK`.

## 1. Aprire lo schermo

```basic
SCREEN 12, 640, 320, 32
```

| Parametro | Significato |
|---|---|
| `12` | modalità grafica |
| `640, 320` | larghezza e altezza in pixel |
| `32` | profondità di colore in bit |

`SCREEN 0` torna alla console. Fra `SCREEN 12` e `SCREEN 0` il programma può disegnare; prima e
dopo no — è la causa classica del "non vedo niente".

## 2. Disegnare

```basic
DRAW TEXT "click: " + STR$(clicks), 60, 130
```

`DRAW` è la famiglia di primitive: testo, linee, rettangoli, cerchi, immagini, sprite. Il testo
ha coordinate in pixel, come i controlli (`x, y`). Poiché `DRAW TEXT` non è un `PRINT`, non
scrolla e non va a capo: scrive dove gli dici.

Il sample `shared/samples/graphics/ControlsAllPixel.bas` è il catalogo delle primitive in un solo
programma; `ChartDrawDemo.bas` mostra il disegno di un grafico a mano.

## 3. Animare: il ciclo `TICK`

```basic
TICK 60
    frameCount = frameCount + 1
    IF TICKS - startTime >= 1000 THEN
        fps = frameCount
        frameCount = 0
        startTime = TICKS
        LOCATE 1, 1
        PRINT "FPS: "; fps
    END IF
    SYNC
    IF NOT running THEN
        EXIT TICK
    END IF
END TICK
```

| Elemento | Ruolo |
|---|---|
| `TICK 60` | ciclo sincronizzato a 60 frame al secondo |
| `TICKS` | millisecondi trascorsi |
| `SYNC` | presenta il frame e attende il successivo |
| `EXIT TICK` | esce dal ciclo |

La logica sta dentro, il `SYNC` chiude il frame. Senza `SYNC` il ciclo gira a vuoto e consuma un
core; con `SYNC` il sistema ti dà esattamente il tempo del frame. Il sample
`GameDev/TickGameLoop.bas` misura anche gli FPS: è il modo per verificare che il loop regga.

## 4. Sprite e collisioni

Uno sprite è un'immagine con posizione, disegnata sopra lo sfondo. Il ciclo tipico è: aggiorna la
posizione, disegna, verifica la collisione.

| Sample | Cosa mostra |
|---|---|
| `Sprites/SpritesAndPages.bas` | sprite e pagine (livelli) |
| `Sprites/SpriteCollision.bas` | collisioni |
| `Sprites/SpritedefDemo.bas` | definizione degli sprite |
| `GameDev/TilemapDemo.bas` | mappa a tile |

Le immagini (`player.png`, `explosion.png`) stanno accanto al sample: i percorsi sono relativi
alla cartella del programma, che è la directory corrente quando gira.

## 5. Suono

`SOUND`/`PLAY` per i toni, i sample `Audio/*.bas` per file audio: `AudioBasics.bas`,
`AudioPlay.bas`, `AudioFileLoading.bas`, `MusicPoliphonicDemo.bas` per la polifonia,
`AudioControls.bas` per i controlli di riproduzione. La guida è `SoundEnhanced`.

## 6. Giochi completi

Due giochi interi, leggibili come esempi: `GameDev/Tetris.bas` e `GameDev/PacMan.bas`. Il primo
mostra griglia, input, collisioni e punteggio; il secondo movimento su mappa, IA dei fantasmi e
stato di gioco. Sono la prova che il modello `TICK` + `SYNC` basta per un gioco vero.

Il joystick si legge con le funzioni del namespace `JOY` (`Joy-Guida-Utente`,
`GameDev/JoystickTest.bas`).

## 7. Perché non c'è un output catturato

Tutti questi programmi aprono una finestra e girano finché non li chiudi: sono interattivi
(`' TAG: interactive`), e nei runner vengono saltati. Un fotogramma catturato non proverebbe
nulla sul comportamento — meglio dichiararlo che simulare una verifica che non c'è stata.

## 8. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| schermo nero | nessun `SCREEN` o disegno prima dell'apertura | apri lo schermo, poi disegna |
| animazione a scatti | lavoro pesante fra due `SYNC` | sposta il calcolo fuori dal frame |
| CPU al 100% | ciclo senza `SYNC` | metti `SYNC` in fondo al `TICK` |
| lo sprite lascia una scia | non ridisegni lo sfondo | ridisegna il fondo a ogni frame |
| il programma non si chiude | `EXIT TICK` mancante | esci su condizione |

## 9. Dove andare adesso

- **IV.19** — finestre ed eventi.
- **IV.20** — i controlli, se non ti serve disegnare a mano.
- **V.27** — la matematica che serve al movimento.














<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Graphics-Guide

*Fonte: `Graphics-Guide`*


## 1. Inizializzazione

### `SCREEN n% [, switch%] [, active% [, visible% [, repl%]]]`

IMPORT GRAPHICS
SCREEN 12          ' 640×480, 16 colori
SCREEN 12, 0, 0, 0 ' modo 12, switch 0, active 0, visible 0
' Qualificata:
GRAPHICS.SCREEN 12


### `SCREENSET [visible%] [, active%]` e `SYNC`

SCREENSET 1, 0  ' disegna su 1, mostra 0
LINE (0,0)-(100,100), 4
SCREENSET 0    ' mostra pagina 1
SYNC


### `SCREENS() → LONG` / `SCREENINFO(n%)`

PRINT SCREENS() ' modi disponibili


## 2. Primitive di disegno

### `PSET (x!, y!) [, col%]` / `PRESET (x!, y!) [, col%]`

PSET (100, 100), 4  ' rosso
PRESET (100, 100)   ' cancella (colore sfondo)


### `LINE [(x1!, y1!)]-(x2!, y2!) [, col%] [, B|BF] [, style%]`

LINE (0,0)-(100,100), 4              ' linea
LINE (10,10)-(50,50), 4, BF            ' rettangolo pieno
LINE -(50,50), 4, , &HFF00             ' stile tratteggiato


### `CIRCLE (x!, y!), r! [, col%] [, start! [, end! [, aspect!]]]`

CIRCLE (160, 100), 50, 4
CIRCLE (160, 100), 50, 4, 0, 3.14, 0.5 ' semi-cerchio schiacciato


### `PAINT (x!, y!) [, fill%] [, border%]`

CIRCLE (100,100), 30, 4
PAINT (100,100), 4, 4


### `DRAW cmd$`

DRAW "U10 D10 L10 R10"
DRAW "BM100,100;M+10,+10"


## 3. Testo e font

### `DRAW TEXT ...` / `DRAWTEXT`

DRAW TEXT "Ciao" AT (100, 100) COLOR 4


### `FONT file$ [, size%]`

FONT "arial.ttf", 16


### `PALETTE col%, r%, g%, b%`

PALETTE 4, 255, 0, 0 ' rosso


## 4. Immagini e texture
| Comando | Descrizione |
|---|---|
| `IMAGEGET (x1,y1)-(x2,y2), arr%()` | Copia area in array |
| `IMAGEPUT (x!, y!), arr%() [, mode%]` | Disegna array |
| `BLOAD file$, addr%` / `BSAVE file$, addr%, len%` | Carica/salva memoria video |
| `LOAD TEXTURE id%, file$` | Carica texture |
| `DRAW TEXTURE id%, x!, y! [, rot! [, sx! [, sy!]]]` | Disegna texture |
| `UNLOAD TEXTURE id%` | Scarica |

DIM buf% (100)
IMAGEGET (0,0)-(10,10), buf%()
IMAGEPUT (20,20), buf%()
LOAD TEXTURE 1, "sprite.png"
DRAW TEXTURE 1, 100, 100, 0, 1, 1


## 5. Viewport e finestre

### `VIEW [SCREEN] [(x1,y1)-(x2,y2)] [, col% [, border%]]`

VIEW SCREEN (0,0)-(319,199), 0, 4
VIEW ' reset


### `WINDOW [SCREEN] [(x1,y1)-(x2,y2)]`

WINDOW SCREEN (0,0)-(100,100)


### `WINDOW DEFINE name$ FROM x1!,y1! TO x2!,y2! [TITLE t$] [DOUBLE] [SHADOW]`

WINDOW DEFINE "W1" FROM 10,10 TO 100,100 TITLE "Finestra"


## 6. Palette e schermo
| Comando | Descrizione |
|---|---|
| `SAVE SCREEN name$` / `RESTORE SCREEN name$` | Salva/ripristina viewport (forma canonica a due parole; `SAVESCREEN` una parola NON esiste nel lexer) |
| `CSAVESCREEN` / `CRESTORESCREEN` | Sempre su console base |
| `VIEWPORT` / `WINDOW` | Già descritti |
| `CLS [col%]` | Pulisce schermo |

SAVE SCREEN "tmp"
CLS 0
RESTORE SCREEN "tmp"


## 7. Sprite e tilemap
| Comando | Descrizione |
|---|---|
| `SPRITE DEF id%, ...` | Definisce sprite |
| `SPRITE DRAW id%, x!, y!` | Disegna |
| `TILEMAP LOAD file$` / `DRAW` | Tilemap |

SPRITE DEF 1, 16, 16, data%()
SPRITE DRAW 1, 100, 100


## 8. Esempi completi

### Esempio 1 — Grafica base

IMPORT GRAPHICS
SCREEN 12
CLS 0
LINE (0,0)-(639,479), 1, BF
CIRCLE (320, 240), 100, 4
PAINT (320, 240), 4, 1
DRAW "BM320,240;M+20,+20"
SCREEN 0


### Esempio 2 — Double buffering

IMPORT GRAPHICS
SCREEN 12
SCREENSET 1, 0
FOR i% = 0 TO 100
  CLS 0
  LINE (i*6, 0)-(i*6+5, 100), i MOD 16, BF
  SCREENSET 0
  SYNC
  SLEEP 50
NEXT


### Esempio 3 — Testo e immagine

IMPORT GRAPHICS
SCREEN 12
FONT "arial.ttf", 20
DRAW TEXT "Ciao GMBasic" AT (100, 100) COLOR 4
DIM buf%(100)
IMAGEGET (0,0)-(10,10), buf%()
IMAGEPUT (50,50), buf%()


## 8bis. Collisioni

IMPORT GRAPHICS
' Collisioni in batch tra gruppi di sprite (AABB):
PRINT GRAPHICS.COLLIDEBATCH(spritesA, spritesB)


## 9. Backend e note

## Riferimenti

## 10. Comandi legacy senza namespace (grafica / console / UI)

### 10.1 Grafica — primitive
| Comando | Sintassi canonica | Headless |
|---|---|---|
| `PSET` | `PSET (x!, y!) [, col%]` | ✅ |
| `PRESET` | `PRESET (x!, y!) [, col%]` | ✅ |
| `LINE` | `LINE [(x1,y1)]-(x2,y2) [, col%] [, B\|BF]` | ✅ |
| `CIRCLE` | `CIRCLE (x!, y!), r! [, col%]` | ✅ |
| `PAINT` | `PAINT (x!, y!) [, fill%] [, border%]` | ✅ |
| `POLYGON` | `POLYGON n%, (x!,y!)… [, col%]` | ✅ |
| `POINT` | `POINT (x!, y!)` → colore | ✅ |
| `CLEAR` | `CLEAR [(x1,y1)-(x2,y2)] [, col%]` | ✅ |
| `DRAW` | `DRAW cmd$` (stringa di primitiva) | ✅ |
| `FONT` | `FONT file$ [, size%]` | ✅ |
| `SCREEN` | `SCREEN mode% [, …]` | ⚠️ display |
| `SCREENSET` | `SCREENSET pagina%, visibile%` (double buffer) | ⚠️ display |
| `PCOPY` | `PCOPY sorgente%, destinazione%` | ⚠️ display |
| `PALETTE` | `PALETTE col%, r%, g%, b%` | ⚠️ display |
| `VIEW` | `VIEW [SCREEN] [(x1,y1)-(x2,y2)] [, col% [, border%]]` | ⚠️ display |
| `WINDOW` | `WINDOW [SCREEN] [(x1,y1)-(x2,y2)]` | ⚠️ display |
| `IMAGEGET` | `IMAGEGET (x1,y1)-(x2,y2), var$` | ⚠️ display |
| `IMAGEPUT` | `IMAGEPUT (x!,y!), var$ [, mode%]` | ⚠️ display |
| `IMAGEPUTS` | `IMAGEPUTS (x!,y!), var$ [, scala!]` | ⚠️ display |
| `BSAVE` | `BSAVE file$, (x1,y1)-(x2,y2)` | ⚠️ display |
| `BLOAD` | `BLOAD file$ [, (x!,y!)]` | ⚠️ display |
| `CHART` | `CHART …` (grafico da serie) | ⚠️ display |

### 10.2 Grafica — sprite / audio
| Comando | Sintassi canonica | Headless |
|---|---|---|
| `SPRITEDEF` | `SPRITEDEF id%, …` | ⚠️ display |
| `SPRITE` | `SPRITE id% …` (LOAD/ON/OFF/MOVE/SETPOSITION/COLLIDE…) | ⚠️ display |
| `TILEMAP` | `TILEMAP …` | ⚠️ display |
| `SYNC` | `SYNC` (present frame) | ⚠️ display |
| `SOUND` | `SOUND …` (carica/suona) | ⚠️ audio |
| `PLAY` | `PLAY nota$ [, durata%]` | ⚠️ audio |

### 10.3 Console (carattere) e schermo
| Comando | Sintassi canonica | Headless |
|---|---|---|
| `CCLS` | `CCLS` (pulisci schermo carattere) | ✅ |
| `CLOCATE` | `CLOCATE riga%, colonna%` | ✅ |
| `CPRINT` | `CPRINT [lista]` | ✅ |
| `CPALETTE` | `CPALETTE col%, r%, g%, b%` | ✅ |
| `SAVE SCREEN` | `SAVE SCREEN var$` (alias `CSAVESCREEN`; **due parole**) | ✅ |
| `RESTORE SCREEN` | `RESTORE SCREEN var$` (alias `CRESTORESCREEN`; **due parole**) | ✅ |
| `CAPTION` | `CAPTION "testo$"` (titolo finestra) | ⚠️ display |
| `SCROLLMODE` | `SCROLLMODE mode%` | ✅ |
| `CURSOR` | `CURSOR ON\|OFF` | ⚠️ display |
| `INKEY` | `INKEY var$` (attesa tasto, non bloccante con timeout) | ⚠️ input |
| `WAITKEY` | `WAITKEY` (attesa bloccante di un tasto) | ⚠️ input |
| `SENDKEY` | `SENDKEY tasto$` (inietta un tasto) | ⚠️ input |
| `SET` | `SET BELL\|CONFIRM\|CURSOR\|CENTURY\|WRAP\|THEME\|DATEFORMAT …` | ✅ |

### 10.4 Widget TUI (interattivi) e dialoghi
| Comando | Sintassi canonica (arità reale) | Ritorno |
|---|---|---|
| `CHECKBOX` | `CHECKBOX riga%, col%, caption$ INTO var` | `1`/`0` (o `"1"`/`"0"` su var stringa) |
| `TOGGLE` | `TOGGLE riga%, col%, caption$ INTO var` | `1`/`0` |
| `LISTBOX` | `LISTBOX riga%, col%, larghezza%, altezza%, items$ INTO var` | etichetta selezionata |
| `COMBOBOX` | `COMBOBOX riga%, col%, larghezza%, items$ INTO var` | valore selezionato |
| `MENU` | `MENU riga%, col%, items$ INTO var` | etichetta selezionata |
| `RADIO` | `RADIO riga%, col%, items$ INTO var` | valore selezionato |
| `SLIDER` | `SLIDER riga%, col%, larghezza%, min%, max%, step% INTO var` | valore corrente (da `min%`) |
| `SPINNER` | `SPINNER riga%, col%, preset$ [INTO var]` | indice del preset (`0` iniziale) |
| `TAB` | `TAB riga%, col%, larghezza%, altezza%, titoli$ INTO var` | indice scheda (`0`) |
| `GRID` | `GRID riga%, col%, larghezza%, altezza%, headers$, data$ INTO var` | indice selezionato (`-1`) |
| `TREEVIEW` | `TREEVIEW riga%, col%, larghezza%, altezza%, nodi$ INTO var` | nodo selezionato |
| `TBROWSE` | `TBROWSE riga%, col%, larghezza%, altezza%, colonne, dati [, opts] INTO var` | indice riga selezionata |
| `STATUSBAR` | `STATUSBAR testo$` | — (nessun INTO) |
| `FILEDIALOG` | `FILEDIALOG maschera$, titolo$ INTO var$` | path scelto (`""` su annulla) |
| `PRINTERDIALOG` | `PRINTERDIALOG [titolo$,] stampante$ , copie , duplex` | stampante (string), copie (`1`), duplex (`0`) |
| `DATEPICKER` | `DATEPICKER riga%, col% INTO var$` | data formattata (stringa) |
| `COLORPICKER` | `COLORPICKER riga%, col%, colonne% INTO var` | indice colore (`-1`) |

IMPORT CONTROLS
DIM scelta AS STRING
COMBOBOX 1, 1, 20, "Rosso,Verde,Blu" INTO scelta
SLIDER 2, 1, 30, 0, 100, 5 INTO valore
PRINTERDIALOG INTO stampante$, copie, duplex


## Appendice — Parità garantita della superficie grafica (2026-10-21)

### SCREENINFO (funzione) — `SCREENINFO(modo)`

### COLLIDEBATCH (funzione) — `COLLIDEBATCH(x1#, y1#, w1#, h1#, x2#, y2#, w2#, h2#)`

### CLS — `CLS [colore]`

### LINE — `LINE (x1,y1)-(x2,y2)[, colore[, "B"|"BF"]]`

### PSET / PRESET — `PSET (x,y)[, colore]` / `PRESET (x,y)`

### VIEW / WINDOW — `VIEW (x1,y1)-(x2,y2)[, coloreBordo, coloreSfondo]` / `WINDOW [SCREEN] (x1,y1)-(x2,y2)`

### PALETTE — `PALETTE index, r, g, b` / `PALETTE USE nome` / `PALETTE DEFINE nome, ...` / `PALETTE REPLACE ...` / `PALETTE SAVE file` / `PALETTE LOAD file`

### SPRITE / SPRITEDEF — `SPRITE SET SCALE id, sx[, sy]`, `SPRITEDEF id, ...`

### TICK — `TICK fps` ... `END TICK`

### SAVE / RESTORE SCREEN — `SAVE SCREEN "nome"` / `RESTORE SCREEN "nome"`

### IMAGEGET / IMAGEPUT / IMAGEPUTS / BSAVE / BLOAD

### TAB in modalità grafica

### Mini-game di riferimento

### Note di disponibilità

### Graph-Guida-Utente

*Fonte: `Graph-Guida-Utente`*


## 0. Modello

## 1. Costruzione (comandi legacy)

FIXTURE$ = PROJECTFOLDER() + "/../Fixtures/graph-vault"
WORK$ = PROJECTFOLDER() + "/work"
IF IO.DirExists(WORK$) = 0 THEN MKDIR WORK$

BRAINS RDD MEMORY                     ' backend di storage: MEMORY | JSON | SQLITE
BRAINS BUILD FIXTURE$, WORK$          ' costruisce il grafo, nome brain = basename del vault
BRAINS UPDATE FIXTURE$, WORK$         ' alias di BUILD (ricostruisce)
BRAINS FIND PATH "index.md" TO "trading/momentum.md"   ' path più corto nel grafo attivo

| Comando | Note |
|---|---|
| `BRAINS RDD <MEMORY\|JSON\|SQLITE>` | seleziona il backend; `ISAM`/`DBF` → errore esplicito |
| `BRAINS BUILD <dir>[, <out>]` | costruisce il grafo arricchito e lo rende attivo |
| `BRAINS UPDATE <dir>[, <out>]` | alias di `BUILD` |
| `BRAINS FIND PATH <from> TO <to>` | path più corto (BFS) nel grafo attivo |

## 2. Query single-brain
| Funzione | Ritorno | Note |
|---|---|---|
| `BRAINS.Neighbors(brain, node)` | JSON | vicini diretti (out+in) |
| `BRAINS.Path(brain, from, to)` | JSON | cammino minimo (BFS) |
| `BRAINS.PageRank(brain [, topK])` | JSON | PageRank (power iteration) |
| `BRAINS.Connected(brain)` | JSON | componenti connesse |
| `BRAINS.StronglyConnected(brain)` | JSON | SCC (Tarjan) |
| `BRAINS.TopoSort(brain)` | JSON | ordinamento topologico (Kahn) |
| `BRAINS.Bipartite(brain)` | JSON | bipartiteness |
| `BRAINS.Betweenness(brain)` | JSON | centralità betweenness (Brandes) |
| `BRAINS.Clustering(brain, node)` | JSON | coefficiente di clustering locale |
| `BRAINS.Diameter(brain)` | JSON | diametro (Floyd-Warshall) |
| `BRAINS.Mst(brain)` | JSON | albero ricoprente minimo (Kruskal) |
| `BRAINS.Stats(brain)` | JSON | metriche base + PageRank top |
| `BRAINS.IsValid(brain)` | `1`/`0` | 1 se almeno un nodo (brain assente → 0) |
| `BRAINS.HasCycle(brain)` | `1`/`0` | 1 se contiene un ciclo (brain assente → 0) |
| `BRAINS.Eccentricity(brain, node)` | JSON | eccentricità del nodo |
| `BRAINS.Dfs(brain, root)` | JSON | visita in profondità |
| `BRAINS.Bfs(brain, root)` | JSON | visita in ampiezza |
| `BRAINS.Dijkstra(brain, from, to)` | JSON | cammino a costo minimo (archi pesati) |
| `BRAINS.Louvain(brain)` | JSON | community detection (Louvain) |
| `BRAINS.MaxFlow(brain, from, to)` | JSON | flusso massimo (Edmonds-Karp) |

## 2bis. Community detection e flusso massimo (dettaglio)

### `BRAINS.Louvain(brain)`

### `BRAINS.MaxFlow(brain, from, to)`

IMPORT GRAPH
' ... BRAINS BUILD <fixture>, <work> ...
PRINT BRAINS.Louvain("graph-vault")
PRINT BRAINS.MaxFlow("graph-vault", "index.md", "airline/load-factor.md")


## 3. Export estesi

PRINT BRAINS.Dot("graph-vault")                        ' Graphviz (dot)
PRINT BRAINS.Subgraph("graph-vault", "index.md", 1)    ' mermaid, distanza ≤ depth
PRINT BRAINS.ByTag("graph-vault", "strategy")          ' nodi con tag + edge
PRINT BRAINS.ByFolder("graph-vault", "strategies")     ' nodi della cartella + edge


## 4. Query multi-brain — `BRAINS.QueryMulti`

' brains espliciti: lista separata da virgole (CSV)
PRINT BRAINS.QueryMulti("momentum", "graph-vault,strategies")
PRINT BRAINS.QueryMulti("index", "graph-vault")
' senza brains: usa tutti i brain collegati (AI BRAIN USE)
PRINT BRAINS.QueryMulti("momentum")


## 5. Verifiche sui quattro backend

## 6. Riferimenti

## Vedi anche

### Joy-Guida-Utente

*Fonte: `Joy-Guida-Utente`*


## Funzioni

IMPORT JOY

PRINT JOY.PORTS()              ' numero di porte joystick disponibili
PRINT JOY.AVAILABLE(0)         ' -1/0: joystick alla porta 0 presente
PRINT JOY.NAME(0)              ' nome del joystick alla porta 0
PRINT JOY.MASK(0)              ' bitmask dei pulsanti premuti

| Funzione | Ritorno | Note |
|---|---|---|
| `JOY.PORTS()` | `LONG` | numero di porte joystick |
| `JOY.AVAILABLE(port)` | `BOOLEAN` | joystick presente alla porta |
| `JOY.NAME(port)` | `STRING` | nome del dispositivo |
| `JOY.MASK(port)` | `LONG` | bitmask dei pulsanti |

## Vedi anche

### SoundEnhanced

*Fonte: `SoundEnhanced`*


## Descrizione

## Sintassi

' Single voice (backward compat)
SOUND freq, dur [, vol [, wave [, att, dec, sus, rel [, pw]]]]

' Multi-voice
SOUND ON voice, freq, dur [, vol [, wave [, att, dec, sus, rel [, pw]]]]

' Filter
FILTER voice, type, cutoff, resonance


## Parametri SOUND
| Parametro | Default | Descrizione |
|-----------|---------|-------------|
| `voice` | 0 | Voice indirizzato (0-7) |
| `freq` | — | Frequenza in Hz |
| `dur` | — | Durata in secondi |
| `vol` | 100 | Volume 0-100 |
| `wave` | 0 | Forma d'onda (vedi sotto) |
| `att` | 10 | Attack: % durata al picco |
| `dec` | 0 | Decay: % durata al sustain |
| `sus` | 100 | Sustain: volume mantenuto % |
| `rel` | 20 | Release: % estinzione |
| `pw` | 25 | Pulse width % (1-99, 50=square) |

## Forme d'onda
| # | Nome | Caratteristica |
|---|------|---------------|
| 0 | Sine | Tono puro + seconda armonica |
| 1 | Square | Ricca armoniche dispari |
| 2 | Sawtooth | Tutte le armoniche, "raspante" |
| 3 | Triangle | Armoniche dispari attenuate |
| 4 | Noise | Casuale bianco (percussioni) |
| 5 | Pulse | Duty cycle variabile via `pw` |

## FILTER
| Parametro | Descrizione |
|-----------|-------------|
| `type` | 0=Off, 1=LowPass, 2=HighPass, 3=BandPass |
| `cutoff` | Frequenza di taglio in Hz (20-20000) |
| `resonance` | Risuonanza Q (0-100%) |

## Voice System

## Implementazione

## AST

## Sample

## Gap residui (vs Commodore V2)

<!-- END SURFACE -->

<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **CHART.CREATE(x1)** — `h = CHART.CREATE("Disegni", "tempo", "prezzo")` (shared/samples/graphics/ChartDrawDemo.bas:31)
- **GRAPHICS.C2PX(x1)** — `PRINT "C2PX(10)    = "; C2PX(10)` (shared/samples/graphics/CoordsAndTextGaps.bas:12)
- **GRAPHICS.C2PY(x1)** — `PRINT "C2PY(5)     = "; C2PY(5)` (shared/samples/graphics/CoordsAndTextGaps.bas:13)
- **GRAPHICS.CELL(x1, x2, x3)** — `PRINT "cell="; GRAPHICS.CELL(0, 0, 0)` (shared/minitests/graphics/graphics_all.bas:16)
- **GRAPHICS.CELLSTR(x1, x2)** — `PRINT "cellstr=["; GRAPHICS.CELLSTR(0, 0); "]"` (shared/minitests/graphics/graphics_all.bas:17)
- **GRAPHICS.COLLIDE(x1)** — `PRINT "collide="; GRAPHICS.COLLIDE(1, 2)` (shared/minitests/graphics/graphics_all.bas:19)
- **GRAPHICS.COLLIDEBATCH(x1, x2, x3, x4)** — `CB = COLLIDEBATCH(X, Y, 14, 14)` (shared/samples/graphics/GraphicsPlanesExplode.bas:73)
- **GRAPHICS.P2CX(x1)** — `PRINT "P2CX(160)   = "; P2CX(160)` (shared/samples/graphics/CoordsAndTextGaps.bas:14)
- **GRAPHICS.P2CY(x1)** — `PRINT "P2CY(80)    = "; P2CY(80)` (shared/samples/graphics/CoordsAndTextGaps.bas:15)
- **GRAPHICS.POINT(x1, x2)** — `PRINT "point="; GRAPHICS.POINT(1, 2)` (shared/minitests/graphics/graphics_all.bas:15)
- **GRAPHICS.RGB(x1, x2, x3)** — `CLS RGB(0, 0, 255)` (shared/samples/Clipper/WindowPixelColorTest.bas:12)
- **GRAPHICS.SCREEN()** — `PRINT "screen="; GRAPHICS.SCREEN()` (shared/minitests/graphics/graphics_all.bas:18)
- **GRAPHICS.SCREENINFO()** — `70 PRINT "ModeID    : "; SCREENINFO().ModeID` (shared/samples/graphics/GraphicsShowModes.bas:47)
- **GRAPHICS.SCREENS()** — `20 NUM_SCREENS = SCREENS()` (shared/samples/graphics/GraphicsShowModes.bas:43)
- **GRAPHICS.TEXTHEIGHT()** — `DIM th = TEXTHEIGHT()          ' altezza logica del font corrente` (shared/samples/windows/WindowDemo.bas:14)
- **GRAPHICS.TEXTWIDTH(x1)** — `LOCATE Y, (640 - TEXTWIDTH(Text$)) / 2` (shared/samples/graphics/GraphicsDrawTextures.bas:205)
<!-- END DOMAIN TABLE -->
