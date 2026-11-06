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
- **VI.38 §7bis** — le opzioni del registro della grafica (`GMB_GFX_BACKEND`, `GMB_GFX_NULL`,
  `GMB_GFX_SCALE`, `GMB_GFX_SHOT`, `GMB_UI_FONT_SIZE`) e le impostazioni che le sostituiscono;
  `AI LIST OPTIONS` stampa nome, descrizione e valore corrente di ognuna.















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

## Superfici offscreen: il comando `CANVAS`

Le primitive di questo capitolo disegnano su una **pagina dello schermo**. Per comporre
un'immagine senza finestra — un grafico da salvare, una texture da riusare, un report PNG —
il linguaggio offre il **canvas offscreen**, una superficie RGBA8 indipendente dallo schermo.
L'handle è un `LONG` (≥ 1); `0` indica lo schermo.

Il comando è organizzato come `CHART`: `CANVAS` seguito da un sottocomando (`NEW`, `USE`,
`END`, `CLEAR`, `SET`, `GET`, `BLIT`, `SAVE`, `LOAD`, `WIDTH`, `HEIGHT`, `FREE`). `NEW` e
`LOAD` producono l'handle con `INTO`; `GET`/`WIDTH`/`HEIGHT`/`SAVE` restituiscono il valore
con `INTO`. La forma `CANVAS x, y, w, h [NAME …]` resta invece il **controllo** a celle del
namespace `CONTROLS`: la disambiguazione guarda il token successivo, esattamente come per
`CHART`.

Dentro un canvas le coordinate sono pixel 1:1: `VIEW`, `WINDOW` e la scala del display non si
applicano, così il contenuto è deterministico e confrontabile fra backend. Con
`CANVAS USE c` le primitive legacy (`PSET`, `LINE`, `CIRCLE`, `PAINT`, `POLYGON`, `DRAW`,
`CLEAR`) scrivono sul canvas; in esecuzione headless il servizio grafico dell'host può
disabilitarle (è il caso del runner IL con `NullGraphicsService`). I sottocomandi nativi
`SET`/`GET`/`BLIT`/`SAVE`/`LOAD` non dipendono dal servizio e sono quindi la via consigliata
per generare immagini nei test e nei report.

```basic
DIM c AS LONG, col AS LONG, ok AS LONG
CANVAS NEW 320, 200 INTO c
CANVAS CLEAR c, 0
FOR x = 0 TO 319 : CANVAS SET c, x, 100, 1 : NEXT x
CANVAS SAVE c, "grafico.png" INTO ok
CANVAS FREE c
```

Esempio completo: `shared/samples/graphics/CanvasPlot.bas` (tutti i sottocomandi, curva
sinusoidale, PNG, rilettura e blit). MiniTest: `graphics/canvas_core.bas`.


















## 4. Il dominio grafico: primitive nel linguaggio, calcolo in FFI

Il linguaggio espone solo le **primitive del compilatore**: le istruzioni di disegno storiche
(`PSET`, `LINE`, `CIRCLE`, `PAINT`, `DRAW`, `SCREEN`) e il comando `CANVAS`. Tutto ciò che è
calcolo — conversione di colore, geometria, filtri, 3D, compressione delle texture — è
implementato nella libreria `gmbgraphics` e raggiungibile via **FFI** con cinque include.

| Superficie | Natura | Dove vive | Motivo |
|---|---|---|---|
| `SCREEN`, `PSET`, `LINE`, `CIRCLE`, `PAINT`, `DRAW` | comandi | runtime nativo | azioni sul framebuffer, non valori |
| `CANVAS` (12 sottocomandi) | comando | runtime nativo | gestione esplicita di buffer offscreen |
| `GFXCOLOR` (11) | funzioni | FFI `libgmbgraphics` | valori puri (conversioni, miscela) |
| `GFXSHAPE` (35) | funzioni | FFI `libgmbgraphics` | predicati e misure geometriche |
| `GFX2D` (44) | funzioni | FFI `libgmbgraphics` | elaborazione numerica di immagini |
| `GFX3D` (60) | funzioni | FFI `libgmbgraphics` | geometria, raggi, mesh, raster |
| `GFXCODEC` (4) | funzioni | FFI `libgmbgraphics` | compressione BC1/DXT1 |

### 4.1 Perché questa separazione

Un comando descrive un'**azione** con effetti osservabili (disegnare, salvare, selezionare un
buffer); una funzione descrive un **valore** (un colore convertito, un'area, una matrice). Tenere
nel linguaggio solo le azioni evita di gonfiare il set di keyword e lascia la matematica in una
libreria C compilabile e testabile da sola. Il confine è verificato: le funzioni colore e
hit-test che in una prima iterazione erano builtin sono state rimosse dal runtime e vivono solo
in FFI, senza duplicazioni.

### 4.2 Modello dei dati

Le superfici FFI non usano struct opache: ricevono e restituiscono **array `DOUBLE`**.

- Immagini 2D: `w*h` valori row-major (`x + w*y`); volumi 3D: `w*h*d` (`x + w*(y + h*z)`).
- Poligoni: coppie `x0,y0,x1,y1,…`; il parametro `n` è il numero di **valori** (quindi `n/2`
  vertici).
- AABB 2D: `[minx,miny,maxx,maxy]`; AABB 3D: `[minx,miny,minz,maxx,maxy,maxz]`.
- Raggi 2D: `[ox,oy,dx,dy]`; raggi 3D: `[ox,oy,oz,dx,dy,dz]`.
- Matrici 4x4: 16 valori in ordine **column-major**; frustum: 6 piani `[a,b,c,d]` (24 valori).
- Mesh e raster: **handle** `LONG` su tabelle statiche della libreria, da liberare sempre.

### 4.3 Contratto di errore

Le funzioni con parametri di uscita ritornano `0` in caso di successo e `-1` se un argomento non
è valido; i predicati ritornano `1`/`0`; le funzioni che creano handle ritornano `0` in caso di
errore. Le scritture fuori dai limiti (pixel, voxel, indici) sono **no-op silenziosi** o
ritornano `-1`, mai corruzione di memoria. Le funzioni che allocano (mesh, raster, superfici)
richiedono la liberazione esplicita: un handle non liberato resta occupato nella tabella del
processo.

### 4.4 `CANVAS`: disegno offscreen

`CANVAS` gestisce buffer RGBA8 indipendenti dalla finestra: `NEW` crea, `USE`/`END` selezionano
il target delle primitive, `SET`/`GET` leggono e scrivono pixel, `CLEAR` riempie, `BLIT` copia,
`SAVE`/`LOAD` scambiano PNG, `WIDTH`/`HEIGHT` interrogano, `FREE` rilascia. L'handle `0`
identifica lo schermo. Con un canvas attivo le primitive (`PSET`, `LINE`, `CIRCLE`, testo)
disegnano lì e la scala di render Retina non viene applicata: le coordinate sono 1:1 con i
pixel del buffer. È la base per generare immagini in modo deterministico e headless.

### 4.5 Parità fra backend e limiti

Il motore di disegno e `CANVAS` sono **nativi**: le quattro gambe (C99 nativo, IL, P-Code,
VM/C) eseguono lo stesso codice e producono lo stesso output. Le superfici FFI sono comuni a
tutte le gambe. Due limiti pratici da conoscere:

- il formato FFI non ha maschere e il compilatore accetta fino a **64 argomenti** (la VM/C
  ha buffer dinamici); le funzioni grafiche raggruppano comunque i parametri in array
  (`RAYCASTDDAEX`, `DRAWTRIANGLE`) per compattezza della firma;
- i locals delle procedure sono **dinamici** (crescono on demand): centinaia di variabili in
  una sola `SUB` funzionano su tutti i backend. Resta comunque consigliato dividere i programmi
  lunghi in `SUB` per leggibilità, come fa `shared/samples/graphics/GraphicsTour.bas`
  (una `SUB` per dominio).

### 4.6 Esempi e verifiche

| Programma | Cosa mostra |
|---|---|
| `shared/samples/graphics/GraphicsTour.bas` | **tour completo**: CANVAS + tutte le 154 funzioni FFI, una sezione per dominio |
| `shared/samples/graphics/CanvasPlot.bas` | canvas, curva, PNG, rilettura, blit |
| `shared/samples/graphics/HeatmapLab.bas` | campo scalare, heatmap, helper immagine/bitmap |
| `shared/samples/graphics/ColorLab.bas` | conversioni colore, hit-test, canvas |
| `shared/samples/graphics/Gfx3dLab.bas` | camera, mesh, raster, raggi, DDA con limite di passi |
| `shared/samples/graphics/ShapeLab.bas` | geometria 2D completa e point cloud 3D |
| `shared/samples/graphics/Bc1Lab.bas` | BC1/DXT1 blocco e immagine |
| `shared/minitests/graphics/`, `shared/minitests/graphicsffi/` | MiniTest con oracolo `EXPECT`, uno per superficie |

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


## 4bis. Canvas offscreen (comando `CANVAS`)
| Comando | Sintassi | Note |
|---|---|---|
| `CANVAS NEW` | `CANVAS NEW w%, h% INTO c%` | crea il canvas (`c%` = 0 se non c'è spazio/memoria) |
| `CANVAS USE` | `CANVAS USE c%` | da qui `PSET`/`LINE`/`CIRCLE`/`PAINT`/`DRAW`… disegnano sul canvas |
| `CANVAS END` | `CANVAS END` | il target torna allo schermo |
| `CANVAS CLEAR` | `CANVAS CLEAR c% [, col%]` | riempie (default nero) |
| `CANVAS SET` | `CANVAS SET c%, x%, y%, col%` | scrive un pixel |
| `CANVAS GET` | `CANVAS GET c%, x%, y% INTO col%` | legge un pixel (colore impacchettato, riusabile) |
| `CANVAS BLIT` | `CANVAS BLIT c% [, dx%, dy%]` | copia `c%` sul target attivo (schermo o altro canvas) |
| `CANVAS SAVE` | `CANVAS SAVE c%, path$ [INTO ok%]` | salva in PNG (0 = ok, -1 = errore) |
| `CANVAS LOAD` | `CANVAS LOAD path$ INTO c%` | PNG → nuovo canvas (`-1`/0 se illeggibile) |
| `CANVAS WIDTH` | `CANVAS WIDTH c% INTO w%` | larghezza (`-1` se handle non valido) |
| `CANVAS HEIGHT` | `CANVAS HEIGHT c% INTO h%` | altezza |
| `CANVAS FREE` | `CANVAS FREE c%` | libera la superficie |

CANVAS GET c, 10, 10 INTO col
CANVAS SET c, 11, 10, col     ' copia un pixel


### Esempio: grafico → PNG → rilettura

IMPORT GRAPHICS

SUB Main()
  DIM c AS LONG, w AS LONG, h AS LONG, col AS LONG, ok AS LONG

  CANVAS NEW 320, 200 INTO c
  CANVAS WIDTH c INTO w
  CANVAS HEIGHT c INTO h
  CANVAS CLEAR c, 0

  ' asse orizzontale + curva (pixel espliciti: identico su tutti i backend)
  FOR x = 0 TO w - 1
    CANVAS SET c, x, h \ 2, 1
  NEXT x
  FOR x = 0 TO w - 1
    y = h \ 2 - INT(60 * SIN(6.283185307179586 * x / (w - 1)))


### Canvas e primitive di disegno

### Errori
| Situazione | Risultato |
|---|---|
| handle inesistente/liberato | `WIDTH`/`HEIGHT`/`GET`/`SET` → `-1` |
| coordinata fuori dal canvas | `GET`/`SET` → `-1` (nessun accesso fuori buffer) |
| `NEW` senza memoria o dimensioni non valide | handle `0` |
| `LOAD` di file assente/non PNG | handle `-1` (o `0` in `INTO`) |
| `SAVE` su path non scrivibile | `-1` |

### `CANVAS` è anche un controllo

## 4ter. Colore e geometria via FFI (`GFXCOLOR`, `GFXSHAPE`)

IMPORT GFXCOLOR
IMPORT GFXSHAPE


### `GFXCOLOR` — colore
| Funzione | Firma | Risultato |
|---|---|---|
| `RGB2HSV` | `(r, g, b, out())` | `out(0)`=H `[0,360)`, `out(1)`=S, `out(2)`=V |
| `HSV2RGB` | `(h, s, v, out())` | RGB `0..255` |
| `RGB2HSL` / `HSL2RGB` | `(r,g,b, out())` / `(h,s,l, out())` | H/S/L, poi RGB |
| `RGB2CMYK` | `(r, g, b, out())` | CMYK `[0,1]` |
| `CMYK2RGB` | `(c, m, y, k, out())` | RGB `0..255` |
| `MIX` | `(r1,g1,b1, r2,g2,b2, t, out())` | interpolazione lineare, `t` clampato |
| `LIGHTEN` / `DARKEN` | `(r, g, b, amt, out())` | luminosità HSL ±amt |
| `SATURATE` | `(r, g, b, amt, out())` | saturazione HSL +amt |
| `COLORMAP` | `(t, out())` | colormap jet-like `t`→RGB `0..255` |

### `GFXSHAPE` — geometria 2D e hit-test
| Funzione | Firma | Risultato |
|---|---|---|
| `POINTINPOLYGON` | `(x, y, pts(), n)` | `1` dentro (even-odd), `0` fuori |
| `POINTINCIRCLE` | `(x, y, cx, cy, r)` | `1`/`0` (bordo incluso) |
| `POINTINRECT` | `(x, y, x1, y1, x2, y2)` | `1`/`0` (vertici in qualunque ordine) |
| `POINTINTRIANGLE` | `(x, y, ax, ay, bx, by, cx, cy)` | `1`/`0` |
| `POLYGONAREA` | `(pts(), n)` | area (shoelace, ≥ 0) |
| `POLYGONCENTROID` | `(pts(), n, out())` | baricentro d'area in `out(0..1)` |
| `CLOSESTPOINTSEG` | `(px, py, ax, ay, bx, by, out())` | punto più vicino sul segmento |
| `DIST` | `(a(), b())` | distanza euclidea |
| `AABBFROMPOINTS2D` / `AABBMERGE2D` / `AABBCENTER2D` / `AABBEXTENTS2D` / `AABBVALID` | AABB 2D `[minx,miny,maxx,maxy]` | |
| `CLOSESTRAY` / `CLOSESTAABB` / `CLOSESTCIRCLE` / `CLOSESTAABBAABB` | punto più vicino | |
| `CIRCLECIRCLE` / `AABBAABB` / `AABBCIRCLE` | intersezioni 2D | |
| `RAY2CIRCLE` / `RAY2AABB` / `RAY2SEGMENT` | raggio 2D `[ox,oy,dx,dy]` | |
| `BARYCENTRIC` | `(a(), b(), c(), p(), out())` | pesi baricentrici |
| `POINTINORIENTEDRECT` / `POINTINQUAD` | rettangolo orientato / quadrilatero | |
| `POLYGONSIGNEDAREA` | area con segno (shoelace) | |
| `HEXAREA` / `HEXLONG` / `HEXSHORT` / `HEXINNER` | esagono regolare | |
| `CHAMFER` / `HAUSDORFF` | `(A(), n, B(), m)` | point cloud xyz (n,m = valori) |
| `AABB2MAKE` / `RAY2AT` | costruttori AABB 2D / punto sul raggio | |

DIM o(5) AS DOUBLE
ok = GFXCOLOR.MIX(0, 0, 255, 255, 255, 255, 0.5, o)   ' grigio 128,128,128
ok = GFXCOLOR.RGB2HSV(0, 128, 255, o)                 ' o(0)=209.88, o(1)=1, o(2)=1

DIM p(7)
p(0)=0 : p(1)=0 : p(2)=10 : p(3)=0
p(4)=10 : p(5)=10 : p(6)=0 : p(7)=10
PRINT GFXSHAPE.POINTINPOLYGON(5, 5, p(), 8)           ' 1
PRINT GFXSHAPE.POLYGONAREA(p(), 8)                    ' 100


## 4quater. Immagini float via FFI (`GFX2D`)
| Gruppo | Funzioni |
|---|---|
| filtri | `CONVOLVE`, `CONVOLVESEP`, `GAUSSIAN`, `SOBEL`, `MEDIAN3` |
| trasformazioni | `RESAMPLE`, `TRANSPOSE`, `ROTATE90`, `ROTATE`, `FLIPH`, `FLIPV` |
| punti | `THRESHOLD`, `NORMALIZE`, `HISTOGRAM`, `HISTEQUALIZE` |
| disegno in-place | `DRAWLINE`, `DRAWRECT`, `DRAWRECTF`, `DRAWCIRCLE` |
| avanzate | `WARP` (CHW + flow), `POISSON` (seamless clone) |
| volumi/superfici | `IMG3CONVOLVE` (volume `w*h*d`), `HEATMAP` (griglia → colori impacchettati) |
| statistiche | `IMGMIN`, `IMGMAX`, `IMGABSMAX`, `IMGSCALE` (in-place), `IMGSUB` (`a-b`) |
| helper puntuali | `IMGGET` (valore del pixel; fuori range → `0`), `IMGSET`, `IMGFILL`, `IMGCLONE` |
| volumi 3D | `IMG3GET`, `IMG3SET`, `IMG3FILL` (layout `x + w*(y + h*z)`) |
| bitmap RGB8 | `GRAYNAIVE`, `GRAYLUM`, `CONVOLVE3`, `BRIGHTNESS`, `TOIMAGE`, `BMPGET`, `BMPSET`, `BMPFILL` (pixel = colori `0xRRGGBB`) |

IMPORT GFX2D
DIM a(15) AS DOUBLE, o(15) AS DOUBLE
FOR i = 0 TO 15 : a(i) = i : NEXT i
ok = GFX2D.GAUSSIAN(a(), 4, 4, 0.8, o)          ' blur gaussiano
ok = GFX2D.TRANSPOSE(a(), 4, 4, o)
ok = GFX2D.THRESHOLD(a(), 4, 4, 7.5, 0, 1, o)
DIM d(15) AS DOUBLE
ok = GFX2D.DRAWLINE(d(), 4, 4, 0, 0, 3, 3, 2.5)  ' disegno su array
ok = GFX2D.IMGSET(d(), 4, 4, 1, 1, 9)            ' pixel singolo
PRINT GFX2D.IMGGET(d(), 4, 4, 1, 1)              ' 9
ok = GFX2D.IMGCLONE(d(), 4, 4, o)                ' copia indipendente


## 4quinquies. Geometria 3D via FFI (`GFX3D`)
| Gruppo | Funzioni |
|---|---|
| matrici/camera | `PERSPECTIVE`, `ORTHO`, `LOOKAT`, `CAMERAVIEW`, `CAMERAPROJECTION`, `CAMERAVIEWPROJECTION`, `DECOMPOSESRT` |
| AABB/frustum | `AABBFROMPOINTS`, `AABBMERGE`, `AABBCENTER`, `AABBEXTENTS`, `AABBTESTPOINT`, `AABBINTERSECT`, `FRUSTUMFROM`, `FRUSTUMTESTPOINT`, `FRUSTUMTESTAABB` |
| raggi | `RAYSPHERE`, `RAYPLANE`, `RAYTRIANGLE`, `RAYAABB`, `RAYBOX`, `RAYREFLECT`, `RAYREFRACT`, `SHADEPHONG` |
| intersezioni | `SPHERESPHERE`, `SPHEREBOX`, `SPHEREPLANE`, `SEGMENTPLANE`, `SEGMENTSPHERE`, `SEGMENTTRIANGLE`, `TRIANGLETRIANGLE`, `VOLUMERENDER` |
| mesh (handle) | `MESHSPHERE`, `MESHBOX`, `MESHPLANE`, `MESHCYLINDER`, `MESHNV`, `MESHNT`, `MESHPOS`, `MESHTRI`, `MESHFREE` |
| raster (handle) | `RASTERNEW`, `RASTERFREE`, `RASTERCLEAR`, `RASTERGET`, `RASTERTRI`, `RASTERCLIPNEAR` |
| superfici/griglie | `MESHSURFACE` (campo di altezza → handle), `RAYCASTDDA` (griglia, max 256 passi), `RAYCASTDDAEX` (limite di passi configurabile) |
| altro | `RAYCYLINDER`, `RAYCONE`, `CYLINDERNORMAL`, `CONENORMAL`, `AABBTRANSFORM`, `AABBMAKE`, `RAYAT`, `CAMERAORBIT`, `CAMERATRANSLATE`, `CAMERANDCRAY` |

IMPORT GFX3D
DIM m(15) AS DOUBLE
ok = GFX3D.PERSPECTIVE(1.5707963, 1.5, 0.1, 100, m)

DIM ray(5) AS DOUBLE, sph(3) AS DOUBLE, t(1) AS DOUBLE
ray(0)=0 : ray(1)=0 : ray(2)=5 : ray(3)=0 : ray(4)=0 : ray(5)=-1
sph(0)=0 : sph(1)=0 : sph(2)=0 : sph(3)=1
PRINT GFX3D.RAYSPHERE(ray(), sph(), t)      ' 1, t=4

h = GFX3D.MESHBOX(1, 1, 1)                  ' handle; vedi MESHNV/MESHNT/MESHPOS/MESHTRI
ok = GFX3D.MESHFREE(h)

r = GFX3D.RASTERNEW(64, 64)                 ' z-buffer RGB float
ok = GFX3D.RASTERCLEAR(r, 1.0)
ok = GFX3D.RASTERTRI(r, p(), c())           ' p() e c() = 9 valori (3 vertici RGB)


## 4sexies. Codec texture via FFI (`GFXCODEC`)
| Funzione | Firma | Note |
|---|---|---|
| `BC1ENCODEBLOCK` | `(rgb(), out())` | 48 → 8 |
| `BC1DECODEBLOCK` | `(in(), out())` | 8 → 48 |
| `BC1ENCODE` | `(rgb(), w, h, out())` | `w,h` multipli di 4; `out()` = `(w/4)*(h/4)*8` |
| `BC1DECODE` | `(in(), w, h, out())` | `out()` = `w*h*3` |

IMPORT GFXCODEC
DIM rgb(47) AS DOUBLE, enc(7) AS DOUBLE, dec(47) AS DOUBLE
FOR i = 0 TO 47 : rgb(i) = 10 + (i MOD 16) * 12 : NEXT i
ok = GFXCODEC.BC1ENCODEBLOCK(rgb(), enc())
ok = GFXCODEC.BC1DECODEBLOCK(enc(), dec())


## 4septies. Riferimento completo delle funzioni FFI

#### `GFXCOLOR` — 11 funzioni
| Funzione | Firma | Parametri e ritorno |
|---|---|---|
| `RGB2HSV` | `(r AS DOUBLE, g AS DOUBLE, b AS DOUBLE, out() AS DOUBLE) AS LONG` | RGB (0..255) -> HSV: out(0)=h [0,360), out(1)=s, out(2)=v [0,1]. |
| `HSV2RGB` | `(h AS DOUBLE, s AS DOUBLE, v AS DOUBLE, out() AS DOUBLE) AS LONG` | HSV -> RGB (0..255) in out(). |
| `RGB2HSL` | `(r AS DOUBLE, g AS DOUBLE, b AS DOUBLE, out() AS DOUBLE) AS LONG` | RGB -> HSL: out(0)=h, out(1)=s, out(2)=l. |
| `HSL2RGB` | `(h AS DOUBLE, s AS DOUBLE, l AS DOUBLE, out() AS DOUBLE) AS LONG` | HSL -> RGB (0..255) in out(). |
| `RGB2CMYK` | `(r AS DOUBLE, g AS DOUBLE, b AS DOUBLE, out() AS DOUBLE) AS LONG` | RGB -> CMYK: out(0..3)=c,m,y,k in [0,1]. |
| `CMYK2RGB` | `(c AS DOUBLE, m AS DOUBLE, y AS DOUBLE, k AS DOUBLE, out() AS DOUBLE) AS LONG` | CMYK (0..1) -> RGB (0..255) in out(). |
| `MIX` | `(r1 AS DOUBLE, g1 AS DOUBLE, b1 AS DOUBLE, r2 AS DOUBLE, g2 AS DOUBLE, b2 AS DOUBLE, t AS DOUBLE, out() AS DOUBLE) AS LONG` | Interpolazione lineare tra due RGB: t clampato [0,1]; out() = 3 componenti 0..255. |
| `LIGHTEN` | `(r AS DOUBLE, g AS DOUBLE, b AS DOUBLE, amt AS DOUBLE, out() AS DOUBLE) AS LONG` | Luminosità HSL +amt (0..1); out() = RGB 0..255. |
| `DARKEN` | `(r AS DOUBLE, g AS DOUBLE, b AS DOUBLE, amt AS DOUBLE, out() AS DOUBLE) AS LONG` | Luminosità HSL -amt; out() = RGB 0..255. |
| `SATURATE` | `(r AS DOUBLE, g AS DOUBLE, b AS DOUBLE, amt AS DOUBLE, out() AS DOUBLE) AS LONG` | Saturazione HSL +amt; out() = RGB 0..255. |
| `COLORMAP` | `(t AS DOUBLE, out() AS DOUBLE) AS LONG` | Colormap jet-like: t in [0,1] -> out() = RGB 0..255 (blu -> ciano -> verde -> giallo -> rosso). |

#### `GFXSHAPE` — 35 funzioni
| Funzione | Firma | Parametri e ritorno |
|---|---|---|
| `POINTINPOLYGON` | `(x AS DOUBLE, y AS DOUBLE, pts() AS DOUBLE, n AS LONG) AS LONG` | Punto in un poligono (even-odd, come il ray-casting classico). |
| `POINTINCIRCLE` | `(x AS DOUBLE, y AS DOUBLE, cx AS DOUBLE, cy AS DOUBLE, r AS DOUBLE) AS LONG` | Punto in un cerchio (bordo incluso). |
| `POINTINRECT` | `(x AS DOUBLE, y AS DOUBLE, x1 AS DOUBLE, y1 AS DOUBLE, x2 AS DOUBLE, y2 AS DOUBLE) AS LONG` | Punto in un rettangolo (vertici in qualunque ordine). |
| `POINTINTRIANGLE` | `(x AS DOUBLE, y AS DOUBLE, ax AS DOUBLE, ay AS DOUBLE, bx AS DOUBLE, by AS DOUBLE, cx AS DOUBLE, cy AS DOUBLE) AS LONG` | Punto in un triangolo (baricentrico). |
| `POLYGONAREA` | `(pts() AS DOUBLE, n AS LONG) AS DOUBLE` | Area del poligono (shoelace, sempre >= 0). |
| `POLYGONCENTROID` | `(pts() AS DOUBLE, n AS LONG, out() AS DOUBLE) AS LONG` | Baricentro dell'area: out(0)=x, out(1)=y. |
| `CLOSESTPOINTSEG` | `(px AS DOUBLE, py AS DOUBLE, ax AS DOUBLE, ay AS DOUBLE, bx AS DOUBLE, by AS DOUBLE, out() AS DOUBLE) AS LONG` | Punto più vicino sul segmento (a,b): out(0)=x, out(1)=y. |
| `AABBFROMPOINTS2D` | `(pts() AS DOUBLE, n AS LONG, out() AS DOUBLE) AS LONG` | AABB = [minx,miny,maxx,maxy]; n = numero di VALORI del poligono/point-cloud. |
| `AABBMERGE2D` | `(a() AS DOUBLE, b() AS DOUBLE, out() AS DOUBLE) AS LONG` | Unione di due AABB 2D: out() = [minx,miny,maxx,maxy]. |
| `AABBCENTER2D` | `(a() AS DOUBLE, out() AS DOUBLE) AS LONG` | Centro dell'AABB 2D: out(0)=x, out(1)=y. |
| `AABBEXTENTS2D` | `(a() AS DOUBLE, out() AS DOUBLE) AS LONG` | Estensioni (max-min) dell'AABB 2D: out(0)=dx, out(1)=dy. |
| `AABBVALID` | `(a() AS DOUBLE) AS LONG` | 1 se l'AABB è ben formato (min <= max), 0 altrimenti. |
| `DIST` | `(a() AS DOUBLE, b() AS DOUBLE) AS DOUBLE` | ── vicinanza / distanze ────────────────────────────────────────────────── |
| `CLOSESTRAY` | `(ray() AS DOUBLE, p() AS DOUBLE, out() AS DOUBLE) AS LONG` | Punto più vicino sul raggio 2D [ox,oy,dx,dy] / sull'AABB / su un cerchio / fra due AABB. |
| `CLOSESTAABB` | `(box() AS DOUBLE, p() AS DOUBLE, out() AS DOUBLE) AS LONG` | Punto dell'AABB più vicino a p(): out(0)=x, out(1)=y. |
| `CLOSESTCIRCLE` | `(cx AS DOUBLE, cy AS DOUBLE, r AS DOUBLE, p() AS DOUBLE, out() AS DOUBLE) AS LONG` | Punto del cerchio (cx,cy,r) più vicino a p(): out(0)=x, out(1)=y. |
| `CLOSESTAABBAABB` | `(a() AS DOUBLE, b() AS DOUBLE, out() AS DOUBLE) AS LONG` | Punto di a() più vicino a b(): out(0)=x, out(1)=y. |
| `CIRCLECIRCLE` | `(c1() AS DOUBLE, r1 AS DOUBLE, c2() AS DOUBLE, r2 AS DOUBLE) AS LONG` | ── intersezioni 2D ─────────────────────────────────────────────────────── |
| `AABBAABB` | `(a() AS DOUBLE, b() AS DOUBLE) AS LONG` | 1 se i due AABB 2D si sovrappongono. |
| `AABBCIRCLE` | `(a() AS DOUBLE, cx AS DOUBLE, cy AS DOUBLE, r AS DOUBLE) AS LONG` | 1 se l'AABB 2D interseca il cerchio (cx,cy,r). |
| `RAY2CIRCLE` | `(ray() AS DOUBLE, cx AS DOUBLE, cy AS DOUBLE, r AS DOUBLE, t() AS DOUBLE) AS LONG` | Raggio 2D vs cerchio/AABB/segmento: t() riceve il parametro (o tmin/tmax per AABB). |
| `RAY2AABB` | `(ray() AS DOUBLE, box() AS DOUBLE, t() AS DOUBLE) AS LONG` | Raggio 2D vs AABB: t() = [tmin,tmax]; 1 se colpito. |
| `RAY2SEGMENT` | `(ray() AS DOUBLE, a() AS DOUBLE, b() AS DOUBLE, t() AS DOUBLE) AS LONG` | Raggio 2D vs segmento a–b: t(0)=parametro; 1 se colpito. |
| `BARYCENTRIC` | `(a() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, p() AS DOUBLE, out() AS DOUBLE) AS LONG` | Baricentro: out(0..2)=wa,wb,wc (somma 1); ritorno 0 su triangolo degenere. |
| `POINTINORIENTEDRECT` | `(center() AS DOUBLE, u() AS DOUBLE, v() AS DOUBLE, half_u AS DOUBLE, half_v AS DOUBLE, p() AS DOUBLE) AS LONG` | Rettangolo orientato (centro, assi u/v ortonormali, semi-lati) e quadrilatero (4 vertici). |
| `POINTINQUAD` | `(a() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, d() AS DOUBLE, p() AS DOUBLE) AS LONG` | 1 se p() è dentro il quadrilatero a,b,c,d (vertici in ordine). |
| `POLYGONSIGNEDAREA` | `(pts() AS DOUBLE, n AS LONG) AS DOUBLE` | Area con segno (shoelace): il segno indica il verso dei vertici. |
| `HEXAREA` | `(side AS DOUBLE) AS DOUBLE` | ── esagono regolare ────────────────────────────────────────────────────── |
| `HEXLONG` | `(side AS DOUBLE) AS DOUBLE` | Diagonale lunga di un esagono regolare di lato side. |
| `HEXSHORT` | `(side AS DOUBLE) AS DOUBLE` | Diagonale corta di un esagono regolare di lato side. |
| `HEXINNER` | `(side AS DOUBLE) AS DOUBLE` | Apotema (raggio inscritto) di un esagono regolare di lato side. |
| `CHAMFER` | `(A() AS DOUBLE, n AS LONG, B() AS DOUBLE, m AS LONG) AS DOUBLE` | A/B = array piatti x,y,z,…; n = numero di VALORI (multipli di 3). |
| `HAUSDORFF` | `(A() AS DOUBLE, n AS LONG, B() AS DOUBLE, m AS LONG) AS DOUBLE` | Distanza di Hausdorff fra due point cloud 3D (max delle distanze minime). |
| `AABB2MAKE` | `(mn() AS DOUBLE, mx() AS DOUBLE, out() AS DOUBLE) AS LONG` | Costruttori 2D (AABB da min/max, punto al parametro t sul raggio). |
| `RAY2AT` | `(ray() AS DOUBLE, t AS DOUBLE, out() AS DOUBLE) AS LONG` | Punto al parametro t lungo il raggio 2D: out(0)=x, out(1)=y. |

#### `GFX2D` — 44 funzioni
| Funzione | Firma | Parametri e ritorno |
|---|---|---|
| `CONVOLVE` | `(in() AS DOUBLE, w AS LONG, h AS LONG, k() AS DOUBLE, kx AS LONG, ky AS LONG, out() AS DOUBLE) AS LONG` | Convoluzione 2D con kernel kx*ky (ancorata al centro), bordo invariato. |
| `CONVOLVESEP` | `(in() AS DOUBLE, w AS LONG, h AS LONG, kx() AS DOUBLE, nx AS LONG, ky() AS DOUBLE, ny AS LONG, out() AS DOUBLE) AS LONG` | Convoluzione separabile: righe con kx(nx), colonne con ky(ny). |
| `GAUSSIAN` | `(in() AS DOUBLE, w AS LONG, h AS LONG, sigma AS DOUBLE, out() AS DOUBLE) AS LONG` | Blur gaussiano (kernel costruito da sigma). |
| `SOBEL` | `(in() AS DOUBLE, w AS LONG, h AS LONG, gx() AS DOUBLE, gy() AS DOUBLE) AS LONG` | Gradiente di Sobel: gx() e gy() in uscita (w*h ciascuno). |
| `MEDIAN3` | `(in() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG` | Mediana 3x3. |
| `RESAMPLE` | `(in() AS DOUBLE, w AS LONG, h AS LONG, nw AS LONG, nh AS LONG, out() AS DOUBLE) AS LONG` | Ricampionamento bilineare a nw x nh (out() = nw*nh). |
| `TRANSPOSE` | `(in() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG` | Trasposta (out() stessa dimensione, w*h valori; nuove dim = h x w). |
| `ROTATE90` | `(in() AS DOUBLE, w AS LONG, h AS LONG, k AS LONG, out() AS DOUBLE) AS LONG` | Rotazione di k*90° antiorario (k mod 4); stessi w*h valori, dim scambiate se k dispari. |
| `ROTATE` | `(in() AS DOUBLE, w AS LONG, h AS LONG, radians AS DOUBLE, bg AS DOUBLE, out() AS DOUBLE) AS LONG` | Rotazione arbitraria attorno al centro (bilineare, bg fuori immagine). |
| `FLIPH` | `(in() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG` | Flip orizzontale / verticale. |
| `FLIPV` | `(in() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG` | Flip verticale: out() riceve l'immagine rovesciata (stesse dimensioni). |
| `THRESHOLD` | `(in() AS DOUBLE, w AS LONG, h AS LONG, t AS DOUBLE, lo AS DOUBLE, hi AS DOUBLE, out() AS DOUBLE) AS LONG` | Soglia: sotto t -> lo, sopra -> hi. |
| `NORMALIZE` | `(in() AS DOUBLE, w AS LONG, h AS LONG, lo AS DOUBLE, hi AS DOUBLE, out() AS DOUBLE) AS LONG` | Normalizza min..max in lo..hi. |
| `HISTOGRAM` | `(in() AS DOUBLE, w AS LONG, h AS LONG, nbins AS LONG, lo AS DOUBLE, hi AS DOUBLE, counts() AS DOUBLE) AS LONG` | Istogramma a nbins classi in [lo,hi]: counts() ha nbins elementi. |
| `HISTEQUALIZE` | `(in() AS DOUBLE, w AS LONG, h AS LONG, nbins AS LONG, out() AS DOUBLE) AS LONG` | Equalizzazione dell'istogramma. |
| `DRAWLINE` | `(d() AS DOUBLE, w AS LONG, h AS LONG, x0 AS LONG, y0 AS LONG, x1 AS LONG, y1 AS LONG, v AS DOUBLE) AS LONG` | --- disegno (in-place sull'array) --- |
| `DRAWRECTF` | `(d() AS DOUBLE, w AS LONG, h AS LONG, x0 AS LONG, y0 AS LONG, x1 AS LONG, y1 AS LONG, v AS DOUBLE) AS LONG` | Rettangolo pieno in-place su un'immagine float. |
| `DRAWRECT` | `(d() AS DOUBLE, w AS LONG, h AS LONG, x0 AS LONG, y0 AS LONG, x1 AS LONG, y1 AS LONG, v AS DOUBLE) AS LONG` | Rettangolo (solo contorno) in-place su un'immagine float. |
| `DRAWCIRCLE` | `(d() AS DOUBLE, w AS LONG, h AS LONG, cx AS LONG, cy AS LONG, r AS LONG, v AS DOUBLE, fill AS LONG) AS LONG` | Cerchio in-place: fill=1 pieno, fill=0 contorno. |
| `WARP` | `(in() AS DOUBLE, c AS LONG, h AS LONG, w AS LONG, fx() AS DOUBLE, fy() AS DOUBLE, out() AS DOUBLE) AS LONG` | Warping all'indietro: in() = c*h*w (CHW), fx()/fy() = h*w offset, out() = c*h*w. |
| `POISSON` | `(target() AS DOUBLE, source() AS DOUBLE, mask() AS DOUBLE, nx AS LONG, ny AS LONG, out() AS DOUBLE, maxit AS LONG) AS LONG` | Poisson/seamless clone: mask() = double 0/1 (nx*ny), out() = nx*ny. |
| `IMG3CONVOLVE` | `(in() AS DOUBLE, w AS LONG, h AS LONG, d AS LONG, k() AS DOUBLE, n AS LONG, out() AS DOUBLE) AS LONG` | Convoluzione separabile su volume w*h*d (stesso kernel 1D sui tre assi). |
| `HEATMAP` | `(z() AS DOUBLE, nx AS LONG, ny AS LONG, zmin AS DOUBLE, zmax AS DOUBLE, out() AS DOUBLE) AS LONG` | Heatmap da griglia z(nx*ny): out() = nx*ny colori impacchettati (0x40000000/RGB), pronti per CANVAS SET. |
| `IMGMIN` | `(in() AS DOUBLE, w AS LONG, h AS LONG) AS DOUBLE` | ── statistiche e combinazioni (immagini float) ──────────────────────────── |
| `IMGMAX` | `(in() AS DOUBLE, w AS LONG, h AS LONG) AS DOUBLE` | Valore massimo dell'immagine. |
| `IMGABSMAX` | `(in() AS DOUBLE, w AS LONG, h AS LONG) AS DOUBLE` | Massimo valore assoluto dell'immagine. |
| `IMGSCALE` | `(d() AS DOUBLE, w AS LONG, h AS LONG, s AS DOUBLE) AS LONG` | Scala in-place: out = d * s. |
| `IMGSUB` | `(a() AS DOUBLE, b() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG` | Differenza: out = a - b. |
| `IMGGET` | `(in() AS DOUBLE, w AS LONG, h AS LONG, x AS LONG, y AS LONG) AS DOUBLE` | Valore del pixel (x,y); coordinate fuori range -> 0. |
| `IMGSET` | `(d() AS DOUBLE, w AS LONG, h AS LONG, x AS LONG, y AS LONG, v AS DOUBLE) AS LONG` | Scrittura di un pixel e riempimento dell'intera immagine (in-place). |
| `IMGFILL` | `(d() AS DOUBLE, w AS LONG, h AS LONG, v AS DOUBLE) AS LONG` | Riempie tutta l'immagine con v (in-place). |
| `IMGCLONE` | `(in() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG` | Copia indipendente: out() = clone di in() (w*h). |
| `IMG3GET` | `(in() AS DOUBLE, w AS LONG, h AS LONG, d AS LONG, x AS LONG, y AS LONG, z AS LONG) AS DOUBLE` | Volume 3D w*h*d (layout x + w*(y + h*z)): pixel e riempimento. |
| `IMG3SET` | `(v() AS DOUBLE, w AS LONG, h AS LONG, d AS LONG, x AS LONG, y AS LONG, z AS LONG, val AS DOUBLE) AS LONG` | Scrive il voxel (x,y,z) del volume w*h*d. |
| `IMG3FILL` | `(v() AS DOUBLE, w AS LONG, h AS LONG, d AS LONG, val AS DOUBLE) AS LONG` | Riempie il volume w*h*d con val. |
| `DRAWTRIANGLE` | `(d() AS DOUBLE, w AS LONG, h AS LONG, a() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, v AS DOUBLE, fill AS LONG) AS LONG` | Triangolo (pieno o contorno): vertici in array [x,y] (a(),b(),c()), in-place. |
| `GRAYNAIVE` | `(px() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG` | Grayscale naive (media) e a luminanza; out() = w*h colori impacchettati. |
| `GRAYLUM` | `(px() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG` | Grayscale a luminanza (0.299r+0.587g+0.114b) -> colori packed 0xRRGGBB. |
| `CONVOLVE3` | `(px() AS DOUBLE, w AS LONG, h AS LONG, k() AS DOUBLE, divider AS DOUBLE, out() AS DOUBLE) AS LONG` | Convoluzione 3x3 per canale (kernel 9 valori, divider); bordo invariato. |
| `BRIGHTNESS` | `(px() AS DOUBLE, w AS LONG, h AS LONG, brightness AS DOUBLE, contrast AS DOUBLE, out() AS DOUBLE) AS LONG` | Luminosità/contrasto: out = clamp((in-128)*contrast + 128 + brightness). |
| `TOIMAGE` | `(px() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG` | Luminanza RGB8 -> immagine float out() (w*h). |
| `BMPGET` | `(px() AS DOUBLE, w AS LONG, h AS LONG, x AS LONG, y AS LONG) AS DOUBLE` | Pixel RGB8 (packed 0xRRGGBB): lettura (fuori range -> 0), scrittura, riempimento. |
| `BMPSET` | `(px() AS DOUBLE, w AS LONG, h AS LONG, x AS LONG, y AS LONG, color AS DOUBLE) AS LONG` | Scrive il pixel RGB8 (colore packed 0xRRGGBB); fuori range = -1. |
| `BMPFILL` | `(px() AS DOUBLE, w AS LONG, h AS LONG, color AS DOUBLE) AS LONG` | Riempie la bitmap con il colore packed 0xRRGGBB. |

#### `GFX3D` — 60 funzioni
| Funzione | Firma | Parametri e ritorno |
|---|---|---|
| `PERSPECTIVE` | `(fovy AS DOUBLE, aspect AS DOUBLE, znear AS DOUBLE, zfar AS DOUBLE, m() AS DOUBLE) AS LONG` | Proiezione prospettica: m() riceve 16 valori. |
| `ORTHO` | `(left AS DOUBLE, right AS DOUBLE, bottom AS DOUBLE, top AS DOUBLE, znear AS DOUBLE, zfar AS DOUBLE, m() AS DOUBLE) AS LONG` | Proiezione ortografica. |
| `LOOKAT` | `(eye() AS DOUBLE, center() AS DOUBLE, up() AS DOUBLE, m() AS DOUBLE) AS LONG` | Matrice di vista look-at. |
| `AABBFROMPOINTS` | `(pts() AS DOUBLE, n AS LONG, out() AS DOUBLE) AS LONG` | AABB dai punti (array piatto x,y,z,…; n = numero di VALORI, multiplo di 3). |
| `AABBMERGE` | `(a() AS DOUBLE, b() AS DOUBLE, out() AS DOUBLE) AS LONG` | Unione di due AABB. |
| `AABBCENTER` | `(a() AS DOUBLE, out() AS DOUBLE) AS LONG` | Centro / estensioni di un AABB. |
| `AABBEXTENTS` | `(a() AS DOUBLE, out() AS DOUBLE) AS LONG` | Estensioni (max-min) dell'AABB 3D: out() = [dx,dy,dz]. |
| `AABBTESTPOINT` | `(a() AS DOUBLE, p() AS DOUBLE) AS LONG` | Test punto in AABB / intersezione AABB. |
| `AABBINTERSECT` | `(a() AS DOUBLE, b() AS DOUBLE) AS LONG` | 1 se i due AABB 3D si sovrappongono. |
| `FRUSTUMFROM` | `(m() AS DOUBLE, planes() AS DOUBLE) AS LONG` | Frustum da matrice (6 piani = 24 valori) e test punto/AABB. |
| `FRUSTUMTESTPOINT` | `(planes() AS DOUBLE, p() AS DOUBLE) AS LONG` | 1 se il punto è dentro il frustum (piani con normale verso l'interno). |
| `FRUSTUMTESTAABB` | `(planes() AS DOUBLE, box() AS DOUBLE) AS LONG` | 1 se l'AABB è almeno parzialmente dentro il frustum. |
| `DECOMPOSESRT` | `(m() AS DOUBLE, scale() AS DOUBLE, rot() AS DOUBLE, pos() AS DOUBLE) AS LONG` | Decomposizione SRT: m = T*R*S -> scale(), rot() (quaternione 4), pos(). |
| `CAMERAVIEW` | `(pos() AS DOUBLE, target() AS DOUBLE, up() AS DOUBLE, m() AS DOUBLE) AS LONG` | Camera: vista / proiezione / view-projection. |
| `CAMERAPROJECTION` | `(fov AS DOUBLE, aspect AS DOUBLE, znear AS DOUBLE, zfar AS DOUBLE, m() AS DOUBLE) AS LONG` | Matrice di proiezione prospettica della camera (16 valori). |
| `CAMERAVIEWPROJECTION` | `(pos() AS DOUBLE, target() AS DOUBLE, up() AS DOUBLE, fov AS DOUBLE, aspect AS DOUBLE, znear AS DOUBLE, zfar AS DOUBLE, m() AS DOUBLE) AS LONG` | Matrice vista x proiezione (16 valori). |
| `RAYSPHERE` | `(ray() AS DOUBLE, sphere() AS DOUBLE, t() AS DOUBLE) AS LONG` | Sfera: sphere() = [cx,cy,cz,r]; t(0) = parametro del colpo più vicino. |
| `RAYPLANE` | `(ray() AS DOUBLE, plane() AS DOUBLE, t() AS DOUBLE) AS LONG` | Piano: plane() = [px,py,pz,nx,ny,nz]. |
| `RAYTRIANGLE` | `(ray() AS DOUBLE, tri() AS DOUBLE, tuv() AS DOUBLE) AS LONG` | Triangolo (Möller–Trumbore): tri() = 9 valori, tuv() = [t,u,v] baricentriche. |
| `RAYAABB` | `(ray() AS DOUBLE, box() AS DOUBLE, t() AS DOUBLE) AS LONG` | AABB: t() = [tmin,tmax]; RAYBOX = t della prima faccia. |
| `RAYBOX` | `(ray() AS DOUBLE, box() AS DOUBLE, t() AS DOUBLE) AS LONG` | t della prima faccia dell'AABB colpita dal raggio. |
| `RAYREFLECT` | `(v() AS DOUBLE, n() AS DOUBLE, out() AS DOUBLE) AS LONG` | Riflessione e rifrazione (eta = n1/n2; 0 = riflessione totale interna). |
| `RAYREFRACT` | `(v() AS DOUBLE, n() AS DOUBLE, eta AS DOUBLE, out() AS DOUBLE) AS LONG` | Rifrazione (legge di Snell, eta=n1/n2): out()=direzione; 0 = riflessione totale. |
| `SHADEPHONG` | `(p() AS DOUBLE, n() AS DOUBLE, view() AS DOUBLE, light() AS DOUBLE, params() AS DOUBLE, out() AS DOUBLE) AS LONG` | Phong: params() = [lr,lg,lb, or,og,ob, ka,kd,ks,shininess]; out() = RGB. |
| `SPHERESPHERE` | `(c1() AS DOUBLE, r1 AS DOUBLE, c2() AS DOUBLE, r2 AS DOUBLE) AS LONG` | Intersezioni sfera/segmento/triangolo. |
| `SPHEREBOX` | `(c() AS DOUBLE, r AS DOUBLE, box() AS DOUBLE) AS LONG` | 1 se la sfera interseca l'AABB. |
| `SPHEREPLANE` | `(c() AS DOUBLE, r AS DOUBLE, plane() AS DOUBLE, dist() AS DOUBLE) AS LONG` | 1 se la sfera interseca il piano; dist(0)=distanza con segno centro-piano. |
| `SEGMENTPLANE` | `(a() AS DOUBLE, b() AS DOUBLE, plane() AS DOUBLE, t() AS DOUBLE) AS LONG` | 1 se il segmento interseca il piano; t(0) in [0,1]. |
| `SEGMENTSPHERE` | `(a() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, r AS DOUBLE, t() AS DOUBLE) AS LONG` | 1 se il segmento interseca la sfera; t(0) in [0,1]. |
| `SEGMENTTRIANGLE` | `(a() AS DOUBLE, b() AS DOUBLE, tri() AS DOUBLE, t() AS DOUBLE) AS LONG` | 1 se il segmento interseca il triangolo; t(0) in [0,1]. |
| `TRIANGLETRIANGLE` | `(t1() AS DOUBLE, t2() AS DOUBLE) AS LONG` | 1 se i due triangoli (9 valori ciascuno) si intersecano. |
| `VOLUMERENDER` | `(n AS LONG, sigma() AS DOUBLE, rgb() AS DOUBLE, dt() AS DOUBLE, out() AS DOUBLE, acc() AS DOUBLE) AS LONG` | Volume rendering: n campioni, sigma(n), rgb(n*3), dt(n); out() = RGB, acc() = opacità. |
| `MESHSPHERE` | `(slices AS LONG, stacks AS LONG, radius AS DOUBLE) AS LONG` | ── mesh (handle) ───────────────────────────────────────────────────────── |
| `MESHBOX` | `(sx AS DOUBLE, sy AS DOUBLE, sz AS DOUBLE) AS LONG` | Mesh box (handle), dimensioni sx,sy,sz. |
| `MESHPLANE` | `(nx AS LONG, ny AS LONG, sx AS DOUBLE, sy AS DOUBLE) AS LONG` | Mesh piano nx x ny (handle), estensione sx,sy. |
| `MESHCYLINDER` | `(slices AS LONG, radius AS DOUBLE, height AS DOUBLE) AS LONG` | Mesh cilindro (handle): slices, radius, height. |
| `MESHNV` | `(h AS LONG) AS LONG` | Vertici/triangoli: MESHPOS riempie nv*3 valori, MESHTRI nt*3 indici. |
| `MESHNT` | `(h AS LONG) AS LONG` | Numero di triangoli della mesh. |
| `MESHPOS` | `(h AS LONG, out() AS DOUBLE) AS LONG` | Vertici della mesh: out() = nv*3 valori x,y,z. |
| `MESHTRI` | `(h AS LONG, out() AS DOUBLE) AS LONG` | Indici dei triangoli: out() = nt*3 valori. |
| `MESHFREE` | `(h AS LONG) AS LONG` | Libera la mesh: l'handle non è più valido. |
| `RASTERNEW` | `(w AS LONG, h AS LONG) AS LONG` | ── raster software (handle) ────────────────────────────────────────────── |
| `RASTERFREE` | `(h AS LONG) AS LONG` | Libera il raster: l'handle non è più valido. |
| `RASTERCLEAR` | `(h AS LONG, depth AS DOUBLE) AS LONG` | Azzera colore e z-buffer al valore depth. |
| `RASTERGET` | `(h AS LONG, x AS LONG, y AS LONG, out() AS DOUBLE) AS LONG` | Colore RGB del pixel (x,y): out() = 3 valori. |
| `RASTERTRI` | `(h AS LONG, p() AS DOUBLE, c() AS DOUBLE) AS LONG` | Rasterizza un triangolo con colore per vertice (p() e c() = 9 valori). |
| `RASTERCLIPNEAR` | `(tri() AS DOUBLE, znear AS DOUBLE, out() AS DOUBLE) AS LONG` | Clippa il triangolo contro z >= znear: ritorna il numero di vertici (out() fino a 12 valori). |
| `MESHSURFACE` | `(z() AS DOUBLE, nx AS LONG, ny AS LONG, sx AS DOUBLE, sy AS DOUBLE, zscale AS DOUBLE) AS LONG` | Mesh da campo di altezza z(nx*ny) su [0,sx]x[0,sy], altezza z*zscale (handle). |
| `RAYCASTDDA` | `(grid() AS DOUBLE, rows AS LONG, cols AS LONG, px AS DOUBLE, py AS DOUBLE, dx AS DOUBLE, dy AS DOUBLE, hit() AS DOUBLE) AS LONG` | DDA su griglia rows*cols (grid() piatto, !=0 solido). hit() = [hit,map_x,map_y,side,perp_dist,wall_x]. |
| `RAYCASTDDAEX` | `(grid() AS DOUBLE, rows AS LONG, cols AS LONG, ray() AS DOUBLE, hit() AS DOUBLE) AS LONG` | (maxsteps <= 0 -> 256). Limite FFI di 8 slot: gli argomenti del raggio stanno in un array. |
| `RAYCYLINDER` | `(ray() AS DOUBLE, base() AS DOUBLE, axis() AS DOUBLE, radius AS DOUBLE, height AS DOUBLE, t() AS DOUBLE) AS LONG` | ── cilindro/cono e trasformazione AABB ─────────────────────────────────── |
| `RAYCONE` | `(ray() AS DOUBLE, apex() AS DOUBLE, axis() AS DOUBLE, radius AS DOUBLE, height AS DOUBLE, t() AS DOUBLE) AS LONG` | Raggio vs cono (apex, axis, radius, height): t(0). |
| `CYLINDERNORMAL` | `(base() AS DOUBLE, axis() AS DOUBLE, p() AS DOUBLE, out() AS DOUBLE) AS LONG` | Normale del cilindro nel punto p(): out() = 3 valori. |
| `CONENORMAL` | `(apex() AS DOUBLE, axis() AS DOUBLE, k AS DOUBLE, p() AS DOUBLE, out() AS DOUBLE) AS LONG` | Normale del cono (k = raggio/altezza) nel punto p(): out() = 3 valori. |
| `AABBTRANSFORM` | `(a() AS DOUBLE, m() AS DOUBLE, out() AS DOUBLE) AS LONG` | AABB trasformato dalla matrice 4x4: out() = 6 valori. |
| `AABBMAKE` | `(mn() AS DOUBLE, mx() AS DOUBLE, out() AS DOUBLE) AS LONG` | ── costruttori e camera ────────────────────────────────────────────────── |
| `RAYAT` | `(ray() AS DOUBLE, t AS DOUBLE, out() AS DOUBLE) AS LONG` | Punto al parametro t lungo il raggio. |
| `CAMERAORBIT` | `(eye() AS DOUBLE, target() AS DOUBLE, yaw AS DOUBLE, pitch AS DOUBLE, radius AS DOUBLE, out() AS DOUBLE) AS LONG` | Orbita/traslazione della camera: out() = nuova posizione. |
| `CAMERATRANSLATE` | `(eye() AS DOUBLE, target() AS DOUBLE, delta() AS DOUBLE, out() AS DOUBLE) AS LONG` | Trasla la camera di delta(): out() = nuova posizione. |
| `CAMERANDCRAY` | `(pos() AS DOUBLE, target() AS DOUBLE, up() AS DOUBLE, fov AS DOUBLE, aspect AS DOUBLE, ndcx AS DOUBLE, ndcy AS DOUBLE, out() AS DOUBLE) AS LONG` | Direzione del raggio dal NDC ([-1,1]^2) nel mondo. |

#### `GFXCODEC` — 4 funzioni
| Funzione | Firma | Parametri e ritorno |
|---|---|---|
| `BC1ENCODEBLOCK` | `(rgb() AS DOUBLE, out() AS DOUBLE) AS LONG` | Comprimi un blocco 4x4: rgb() 48 valori -> out() 8 byte. |
| `BC1DECODEBLOCK` | `(in() AS DOUBLE, out() AS DOUBLE) AS LONG` | Decomprimi un blocco: in() 8 byte -> out() 48 valori RGB. |
| `BC1ENCODE` | `(rgb() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG` | Immagine intera (w,h multipli di 4): out() = (w/4)*(h/4)*8 byte; ritorno = byte scritti o -1. |
| `BC1DECODE` | `(in() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG` | Decodifica: in() = blocchi compressi, out() = w*h*3 valori RGB. |

### Tour completo (riferimento eseguibile)

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
| `CANVAS` | `CANVAS NEW/USE/END/CLEAR/SET/GET/BLIT/SAVE/LOAD/WIDTH/HEIGHT/FREE` | ✅ (nativo) |
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
| `BRAINS.Stats(brain)` | JSON | metriche base + PageRank top |
| `BRAINS.IsValid(brain)` | `1`/`0` | 1 se almeno un nodo (brain assente → 0) |

> **Boundary (2026-11-06).** `BRAINS` non contiene più algoritmi di teoria dei grafi: espone
> solo l'accesso AI/dati al cervello. Gli algoritmi (PageRank, connected/SCC, topo-sort,
> bipartiteness, betweenness, clustering, diametro, eccentricità, MST, DFS/BFS, Dijkstra,
> Louvain, max-flow, sottografo, has-cycle) stanno **una sola volta** nel pacchetto FFI
> `GRAPH`/`FLOW` e lavorano su matrici esplicite — vedi §2 "graph.inc" più sotto e
> `shared/samples/graph/GraphFFILab.bas`. Mappa `BRAINS.X → GRAPH.Y` nel commento di testa di
> `GMBasicSharp/common/StdLib/StdLib.Brains.cs`.

## 2bis. Community detection e flusso massimo (dettaglio)

Gli algoritmi sono nel pacchetto FFI (una sola implementazione, in C):

IMPORT GRAPH
IMPORT FLOW
' adj()/w() = matrice di adiacenza o dei pesi, n = numero di nodi (row-major)
PRINT GRAPH.LOUVAIN(w, n, comm, 1.0)                   ' community detection
PRINT FLOW.MAXFLOW(cap, n, s, t)                      ' flusso massimo s -> t


## 3. Export estesi

PRINT BRAINS.Dot("graph-vault")                        ' Graphviz (dot)
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

### Graph-FFI-Guida-Utente

*Fonte: `Graph-FFI-Guida-Utente`*


IMPORT GRAPH


## 1. Convenzioni

## 2. `graph.inc` — 33 funzioni
| Funzione | Firma | Parametri |
|---|---|---|
| `DEGREE` | `FUNCTION DEGREE(adj() AS LONG, n AS LONG, v AS LONG)` AS LONG | Grado (out+in, archi non orientati) del nodo v; -1 se v non esiste. |
| `HASCYCLE` | `FUNCTION HASCYCLE(adj() AS LONG, n AS LONG)` AS LONG | 1 se il grafo ORIENTATO ha un ciclo (Kahn: nodi processati < n). |
| `ISBIPARTITE` | `FUNCTION ISBIPARTITE(adj() AS LONG, n AS LONG)` AS LONG | 1 se il grafo (non orientato) è bipartito. |
| `BFS` | `FUNCTION BFS(adj() AS LONG, n AS LONG, src AS LONG, order() AS LONG)` AS LONG | BFS da src: riempie order() e ritorna il numero di nodi visitati (-1 errore). |
| `DFS` | `FUNCTION DFS(adj() AS LONG, n AS LONG, src AS LONG, order() AS LONG)` AS LONG | BFS da src: riempie order() e ritorna il numero di nodi visitati (-1 errore). |
| `TOPOSORT` | `FUNCTION TOPOSORT(adj() AS LONG, n AS LONG, order() AS LONG)` AS LONG | Ordinamento topologico di un DAG: 1 ok (order() = nodi), 0 se c'è un ciclo. |
| `COMPONENTS` | `FUNCTION COMPONENTS(adj() AS LONG, n AS LONG, comp() AS LONG)` AS LONG | Componenti (debolmente) connesse: comp(i) = id componente; ritorna #componenti. |
| `SCC` | `FUNCTION SCC(adj() AS LONG, n AS LONG, comp() AS LONG)` AS LONG | Componenti fortemente connesse (Tarjan): comp(i) = id; ritorna #componenti. |
| `DIJKSTRA` | `FUNCTION DIJKSTRA(w() AS DOUBLE, n AS LONG, src AS LONG, dist() AS DOUBLE, prev() AS LONG)` AS LONG | Dijkstra (pesi non negativi): dist() e prev() (-1 = nessun padre). 1 ok. |
| `BELLMANFORD` | `FUNCTION BELLMANFORD(w() AS DOUBLE, n AS LONG, src AS LONG, dist() AS DOUBLE, prev() AS LONG)` AS LONG | Bellman-Ford (pesi negativi ammessi). 1 ok, GOR_ERR_NOCONV (4) se ciclo negativo. |
| `FLOYDWARSHALL` | `FUNCTION FLOYDWARSHALL(w() AS DOUBLE, n AS LONG, dist() AS DOUBLE)` AS LONG | Floyd-Warshall: dist() è n*n. 1 ok. |
| `PAGERANK` | `FUNCTION PAGERANK(w() AS DOUBLE, n AS LONG, rank() AS DOUBLE, damping AS DOUBLE, iters AS LONG)` AS LONG | PageRank (damping tipico 0.85, iters 50..100). rank() = n. |
| `BETWEENNESS` | `FUNCTION BETWEENNESS(w() AS DOUBLE, n AS LONG, bc() AS DOUBLE)` AS DOUBLE | Betweenness (Brandes, archi non pesati): bc() = n; ritorna il valore massimo. |
| `CLOSENESS` | `FUNCTION CLOSENESS(adj() AS LONG, n AS LONG, cc() AS DOUBLE)` AS LONG | Closeness centrality (BFS non pesato): cc() = n. |
| `LOUVAIN` | `FUNCTION LOUVAIN(w() AS DOUBLE, n AS LONG, comm() AS LONG, resolution AS DOUBLE)` AS LONG | Louvain (un livello): comm() = n; ritorna #community. |
| `COLORING` | `FUNCTION COLORING(adj() AS LONG, n AS LONG, color() AS LONG)` AS LONG | Colorazione DSATUR: color() = 0..k-1; ritorna k. |
| `MAXCLIQUE` | `FUNCTION MAXCLIQUE(adj() AS LONG, n AS LONG, clique() AS LONG)` AS LONG | Clique massima (Bron-Kerbosch): clique() = n; ritorna la dimensione. |
| `TRANSITIVECLOSURE` | `FUNCTION TRANSITIVECLOSURE(adj() AS LONG, n AS LONG, out() AS LONG)` AS LONG | Chiusura transitiva (Warshall): out() = n*n, 0/1. |
| `DIAMETER` | `FUNCTION DIAMETER(w() AS DOUBLE, n AS LONG)` AS DOUBLE | Diametro pesato (Floyd-Warshall). 0 se n<2 o nessuna coppia raggiungibile. |
| `ECCENTRICITY` | `FUNCTION ECCENTRICITY(w() AS DOUBLE, n AS LONG, v AS LONG)` AS DOUBLE | Eccentricità del nodo v; -1 se v fuori range. |
| `CLUSTERING` | `FUNCTION CLUSTERING(adj() AS LONG, n AS LONG, v AS LONG)` AS DOUBLE | Coefficiente di clustering locale di v (archi non orientati). |
| `SUBGRAPH` | `FUNCTION SUBGRAPH(w() AS DOUBLE, n AS LONG, nodes() AS LONG, k AS LONG, out() AS DOUBLE)` AS LONG | Sottografo indotto dai nodi nodes(0..k-1): out() = k*k; 0 = GOR_OK. |
| `MSTKRUSKAL` | `FUNCTION MSTKRUSKAL(w() AS DOUBLE, n AS LONG, eu() AS LONG, ev() AS LONG, ew() AS DOUBLE)` AS LONG | Kruskal: riempie eu()/ev()/ew() (capacità n-1) e ritorna il numero di archi. |
| `BRIDGES` | `FUNCTION BRIDGES(adj() AS LONG, n AS LONG, is_bridge() AS LONG)` AS LONG | Ponti (grafo non orientato): is_bridge() = n*n (0/1); ritorna #ponti. |
| `ARTICULATION` | `FUNCTION ARTICULATION(adj() AS LONG, n AS LONG, art() AS LONG)` AS LONG | Punti di articolazione: art() = n (0/1); ritorna il numero. |
| `ASTAR` | `FUNCTION ASTAR(w() AS DOUBLE, n AS LONG, h() AS DOUBLE, s AS LONG, t AS LONG, path() AS LONG, info() AS LONG)` AS DOUBLE | Ritorna il costo del cammino o -1 se irraggiungibile. |
| `STOERWAGNER` | `FUNCTION STOERWAGNER(w() AS DOUBLE, n AS LONG)` AS DOUBLE | Global min-cut (Stoer-Wagner, matrice SIMMETRICA). |
| `KSHORTEST` | `FUNCTION KSHORTEST(w() AS DOUBLE, n AS LONG, s AS LONG, t AS LONG, K AS LONG, paths() AS LONG, plens() AS LONG)` AS LONG | K cammini minimi semplici (Yen): paths() = K*n, plens() = K; ritorna K'. |
| `ISOMORPHIC` | `FUNCTION ISOMORPHIC(adj1() AS LONG, n1 AS LONG, adj2() AS LONG, n2 AS LONG, mapping() AS LONG)` AS LONG | Isomorfismo (VF2): mapping() = n1; 1 se isomorfi. |
| `DRNL` | `FUNCTION DRNL(adj() AS LONG, n AS LONG, u AS LONG, v AS LONG, label() AS LONG)` AS LONG | DRNL (SEAL) per il link (u,v): label() = n; ritorna 1 se calcolato. |
| `WL` | `FUNCTION WL(adj() AS LONG, n AS LONG, colors() AS LONG)` AS LONG | Weisfeiler-Lehman (1-WL): colors() = n; ritorna #classi. |
| `MINPATHCOVER` | `FUNCTION MINPATHCOVER(adj() AS LONG, n AS LONG)` AS LONG | Min path cover di un DAG. |
| `BFSOPT` | `FUNCTION BFSOPT(adj() AS LONG, n AS LONG, src AS LONG, thr AS DOUBLE, dist() AS DOUBLE)` AS LONG | BFS direction-optimizing (Beamer): dist() = n (-1 non raggiunto); ritorna #visitati. |

## 3. `flow.inc` — 10 funzioni (stesso namespace, dylib `gmbflow`)
| Funzione | Firma | Parametri |
|---|---|---|
| `MAXFLOW` | `FUNCTION MAXFLOW(cap() AS DOUBLE, n AS LONG, s AS LONG, t AS LONG)` AS DOUBLE | Max-flow (Edmonds-Karp): capacità cap(i*n+j); ritorna il valore (-1 errore). |
| `MINCUT` | `FUNCTION MINCUT(cap() AS DOUBLE, n AS LONG, s AS LONG, t AS LONG, side() AS LONG)` AS DOUBLE | Min-cut: side() = 0 lato sorgente, 1 lato pozzo; ritorna il valore del taglio. |
| `DINIC` | `FUNCTION DINIC(cap() AS DOUBLE, n AS LONG, s AS LONG, t AS LONG)` AS DOUBLE | Dinic (max-flow veloce): ritorna il valore. |
| `MINCOSTFLOW` | `FUNCTION MINCOSTFLOW(cap() AS DOUBLE, cost() AS DOUBLE, n AS LONG, s AS LONG, t AS LONG, flowValue() AS DOUBLE)` AS DOUBLE | Costo minimo per il max-flow; flowValue(0) riceve il valore del flusso. |
| `BIPARTITEMATCH` | `FUNCTION BIPARTITEMATCH(adj() AS LONG, nL AS LONG, nR AS LONG, matchL() AS LONG, matchR() AS LONG)` AS LONG | Matching bipartito massimo (Kuhn): adj() = nL*nR; ritorna la cardinalità. |
| `GENERALMATCH` | `FUNCTION GENERALMATCH(adj() AS LONG, n AS LONG, match() AS LONG)` AS LONG | Matching massimo su grafo generale (blossom): match() = n; ritorna la cardinalità. |
| `WEIGHTEDMATCH` | `FUNCTION WEIGHTEDMATCH(w() AS DOUBLE, n AS LONG, match() AS LONG)` AS DOUBLE | Matching pesato (euristica greedy + local search): ritorna il peso totale. |
| `CIRCULATION` | `FUNCTION CIRCULATION(lower() AS DOUBLE, upper() AS DOUBLE, n AS LONG, b() AS DOUBLE, flow() AS DOUBLE)` AS LONG | Circolazione ammissibile con lower/upper e domanda netta b(): 1 se ammissibile. |
| `GOMORYHU` | `FUNCTION GOMORYHU(w() AS DOUBLE, n AS LONG, parent() AS LONG, weight() AS DOUBLE)` AS LONG | Gomory-Hu tree: parent() = n, weight() = n; ritorna n-1. |
| `MULTICOMMODITY` | `FUNCTION MULTICOMMODITY(cap() AS DOUBLE, n AS LONG, ncomm AS LONG, commodities() AS DOUBLE, flow() AS DOUBLE)` AS DOUBLE | Multi-commodity flow (greedy): commodities() = ncomm*3 (src, dst, demand); flow() = n*n. |

## 4. Esempio minimo

IMPORT GRAPH

INF = GRAPH.ABSENT()
DIM w(8) AS DOUBLE          ' 3 nodi: 0-1 (1), 1-2 (2), 0-2 assente
w(0)=0 : w(1)=1 : w(2)=INF
w(3)=1 : w(4)=0 : w(5)=2
w(6)=INF : w(7)=2 : w(8)=0

DIM dist(2) AS DOUBLE
DIM prev(2) AS LONG
PRINT GRAPH.DIJKSTRA(w, 3, 0, dist, prev)   ' 1 (successo)
PRINT dist(2)                                ' 3  (0 -> 1 -> 2)


## 5. Protocollo §4.1 — esito
| Superficie | MiniTest | Funzioni | IL | PCode | VM/C | C99Native |
|---|---|---|---|---|---|---|
| `graph.inc` | `graphffi/graph_core.bas`, `graph_weighted.bas`, `graph_advanced.bas` | 33 | ✓ | ✓ | ✓ | ✓ |
| `flow.inc` | `flowffi/flow_core.bas` | 10 | ✓ | ✓ | ✓ | ✓ |

## 6. Riferimenti

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
