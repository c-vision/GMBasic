# SCENE — Guida utente

Il dominio `SCENE` espone al linguaggio BASIC il **renderer mesh** del motore 3D: si dichiara una
scena con oggetti e vertici, si imposta la camera e si rasterizza in un target ARGB con
depth buffer, taglio del frustum, back-face e UV prospettici. È pensato per il ciclo di un gioco
(`SCENE RENDER` una volta per frame), non per disegnare un'immagine statica una volta sola.

Disponibile su **tutte e quattro le gambe** (C99Native, IL, PCodeVM, VM/C), senza differenze di
parametri né di risultati: lo stato della scena vive **solo** nel runtime C e le gambe gestite
chiamano la stessa facciata.

---

## 1. Sintassi

Il dominio segue la regola del linguaggio: **il comando scrive, la funzione legge** (§1).

| Forma | Sintassi | Che cosa fa |
|---|---|---|
| Comando | `SCENE RESET` | azzera geometria, oggetti e camera |
| Comando | `SCENE VIEW w, h, fov` | crea il target `w×h` e imposta il campo visivo **verticale** |
| Comando | `SCENE TEXTURE celle` | definisce la texture a scacchi (1…64 celle) |
| Comando | `SCENE OBJECT r, g, b` | apre un oggetto e ne fissa la tinta |
| Comando | `SCENE VERTEX x, y, z, u, v` | aggiunge un vertice all'oggetto corrente |
| Comando | `SCENE QUAD a, b, c, d` | due triangoli sui quattro vertici indicati |
| Comando | `SCENE BOX x0, y0, z0, x1, y1, z1` | parallelepipedo: 24 vertici, 12 triangoli, UV per faccia |
| Comando | `SCENE OBJPOS x, y, z, yaw` | posizione e rotazione dell'oggetto corrente |
| Comando | `SCENE CAMERA x, y, z, yaw, pitch` | posizione e orientamento dell'osservatore |
| Comando | `SCENE RENDER` | rasterizza la scena nel target |
| Comando | `SCENE SAVE path$` | salva il frame corrente in PNG |
| Funzione | `SCENE.VERTS()` | vertici dichiarati |
| Funzione | `SCENE.TRIS()` | triangoli dichiarati |
| Funzione | `SCENE.DRAWN()` | triangoli effettivamente rasterizzati nell'ultimo `RENDER` |
| Funzione | `SCENE.CULLED()` | triangoli scartati (fuori frustum o back-face) |
| Funzione | `SCENE.PIXELS()` | pixel passati dal depth test nell'ultimo `RENDER` |
| Funzione | `SCENE.HASH()` | FNV-1a del frame (stesso frame ⇒ stesso numero) |
| Funzione | `SCENE.PIXEL(x, y)` | colore `0x00RRGGBB` del target, `−1` fuori dai limiti |

`IMPORT SCENE` è **obbligatoria**: il dominio non è implicito (come `WORLD` e `CLOCK`). Senza
l'import il programma non compila, sia per il comando sia per le letture qualificate:

```
namespace non importato: SCENE (aggiungi IMPORT SCENE).
```

---

## 2. Comandi

### `SCENE RESET`

Azzera vertici, triangoli, oggetti e statistiche, riporta la camera a `(0, 1.5, 6)` con
`yaw = pitch = 0` e fov `60`. **Non** libera il target: dopo un `RESET` si può ridichiarare la
scena e chiamare `RENDER` senza un nuovo `VIEW`.

### `SCENE VIEW w, h, fov`

Alloca (o rialloca) il target `w×h` e lo riempie col fondo della scena (`0x0F1729`). `fov` è il
campo visivo **verticale** in gradi: valori validi `1 < fov < 179`, altrimenti il comando è
rifiutato e **il target precedente resta quello di prima**. Il rapporto d'aspetto è `w / h`.

### `SCENE TEXTURE celle`

Definisce la texture della scena: una scacchiera in scala di grigi di `celle × celle` quadrati,
`8` pixel per cella (`1…64` celle ⇒ 8…512 pixel di lato). Il colore finale di un texel è la
texture moltiplicata per la tinta dell'oggetto, quindi la stessa texture serve oggetti diversi
(`OBJECT 220, 90, 60` = rame, `OBJECT 90, 150, 220` = azzurro). Senza `TEXTURE` gli oggetti si
disegnano col solo colore pieno della tinta: non è un errore.

### `SCENE OBJECT r, g, b`

Apre un **nuovo oggetto** e lo rende quello corrente: i `VERTEX`, `QUAD` e `BOX` successivi
appartengono a lui. `r, g, b` sono `0…255` e valgono come **tinta** (moltiplicata per la texture e
per la luce della faccia). Un oggetto senza vertici non disegna nulla. Il numero massimo di
oggetti è 256, di vertici 32768 e di triangoli 16384: oltre, il comando è rifiutato e la scena
resta quella di prima.

### `SCENE VERTEX x, y, z, u, v`

Aggiunge un vertice **all'oggetto corrente** con la sua coordinata di texture `(u, v)` in unità
della texture (`u, v` in `0…1` coprono l'intera immagine). L'ordine dei vertici è quello del
poligono: `QUAD`/`BOX` li interpretano in quell'ordine.

**Winding**: i vertici si dichiarano in senso **antiorario visto da fuori**. Una faccia scritta al
contrario è una back-face e finisce in `SCENE.CULLED()`, non sullo schermo: se un oggetto
"sparisce", il primo sospetto è l'ordine dei vertici, non la camera.

### `SCENE QUAD a, b, c, d`

Due triangoli (`a-b-c` e `a-c-d`) sui vertici dell'oggetto corrente, indicati per **posizione**
(0 = primo vertice dell'oggetto, 1 = secondo, …). Utile per piani e pannelli.

### `SCENE BOX x0, y0, z0, x1, y1, z1`

Parallelepipedo allineato agli assi, in coordinate dell'oggetto: 24 vertici e 12 triangoli, con
UV `0…1` su ogni faccia. Le sei facce hanno **luci diverse** (`220, 160, 255, 110, 190, 140` per
`+X, −X, +Y, −Y, +Z, −Z`): è così che si legge il volume senza un modello di luce completo.

### `SCENE OBJPOS x, y, z, yaw`

Posizione dell'oggetto corrente nel mondo e rotazione attorno all'asse verticale, in **gradi**
(`0` = nessuna rotazione). Il motore costruisce la matrice: il BASIC non maneggia matrici.

### `SCENE CAMERA x, y, z, yaw, pitch`

`yaw` in **gradi**: `0` guarda verso `+z`, `90` verso `+x`. `pitch` in **gradi**, negativo =
guarda in basso; il valore deve stare in `−89 … +89` (oltre, il comando è rifiutato e la camera
resta quella di prima).

### `SCENE RENDER`

Rasterizza tutti gli oggetti nel target: clip contro i sei piani del frustum, divisione
prospettica, back-face, z-buffer, UV prospettici, campionamento bilineare con alpha-test. I
contatori (`DRAWN`, `CULLED`, `PIXELS`) sono **azzerati e ricalcolati** a ogni chiamata, e
`HASH()` diventa lo stesso numero per due frame identici — è la prova che il frame non porta
stato da un render all'altro.

`RENDER` non disegna nulla e restituisce un errore se manca il target (`VIEW` mai chiamato) o se
la scena non ha oggetti/triangoli. **Non** è un errore chiamarlo più volte: è il ciclo normale.

### `SCENE SAVE path$`

Scrive il frame corrente in PNG (stessa risoluzione del target, nessuna scala). È l'unico comando
del dominio che tocca il filesystem: serve ai test e alle catture, non al ciclo di gioco.

---

## 3. Letture

### `SCENE.VERTS()` / `SCENE.TRIS()`

Geometria **dichiarata** (quella che hai scritto), non quella disegnata. Servono a verificare che
la scena sia stata costruita come credi: 1 quad = 4 vertici e 2 triangoli, 1 box = 24 e 12.

### `SCENE.DRAWN()` / `SCENE.CULLED()`

Triangoli passati al rasterizzatore e triangoli scartati nell'**ultimo** `RENDER`. Se
`DRAWN() = TRIS()` il culling non sta lavorando (di solito: tutto frontale, o scena troppo
piccola); se `DRAWN() = 0` la camera non vede nulla (posizione, `yaw`, oppure `pitch` fuori
limite). Il loro rapporto è la diagnosi più rapida di una scena "vuota".

### `SCENE.PIXELS()`

Pixel passati dal depth test, cioè quelli che hanno davvero scritto nel frame. È il numero che
dice se la scena **occupa** lo schermo o se è un puntino lontano.

### `SCENE.HASH()`

FNV-1a a 32 bit del frame: due `RENDER` identici danno lo stesso valore, un cambiamento di
geometria, camera o luce lo cambia. È il modo per confrontare due frame senza guardarli (ed è
quello che usa il MiniTest). Dopo `SCENE RESET` e prima di un `RENDER` vale `0`.

### `SCENE.PIXEL(x, y)`

Colore del target in `0x00RRGGBB` (come le pagine 2D): `r = INT(c / 65536) MOD 256`, eccetera.
Fuori dai limiti (`x < 0`, `y < 0`, `x ≥ w`, `y ≥ h`) restituisce `−1`: la sentinella è un
numero, non un colore, quindi non si confonde con il nero. Prima del primo `RENDER` restituisce
`−1` perché il target non esiste ancora.

---

## 4. Errori e diagnosi

I comandi **non hanno valore di ritorno** leggibile (§1): un comando rifiutato non ferma il
programma e non produce un numero da controllare. Gli errori si vedono **dallo stato**:

| Sintomo | Causa probabile |
|---|---|
| `DRAWN() = 0` e `TRIS() > 0` | camera fuori scena, `yaw` sbagliato, `pitch` oltre `±89`, oppure `VIEW` mai chiamato |
| `DRAWN() = TRIS()` | nessuna faccia scartata: vertici in ordine **orario** (fronti tutte verso la camera) o scena dentro il frustum per intero |
| un oggetto non compare | `OBJPOS` chiamato **prima** di `OBJECT` (agisce sull'oggetto precedente), oppure oggetto aperto e mai riempito |
| il frame è tutto del colore di fondo | `RENDER` prima di dichiarare la geometria, oppure `RESET` dopo averla dichiarata |
| `VERTS()`/`TRIS()` più bassi del previsto | il comando è stato rifiutato per limite (`256` oggetti, `32768` vertici, `16384` triangoli) |
| `PIXEL()` sempre `−1` | `VIEW` mai chiamato, o coordinate fuori dal target |
| `SCENE SAVE` non scrive | percorso non scrivibile dalla cartella di esecuzione del programma |

`IMPORT SCENE` dimenticata: il programma non compila (`namespace non importato: SCENE`). È un
errore di compilazione, quindi si vede subito e su tutte le gambe.

---

## 5. Programma completo

```basic
IMPORT SCENE

SUB Main()
  ' Target e inquadratura: 320x150, campo visivo verticale di 60 gradi.
  SCENE RESET
  SCENE VIEW 320, 150, 60
  SCENE TEXTURE 8

  ' Piano d'appoggio 40x40, antiorario visto da sopra (+y).
  SCENE OBJECT 90, 150, 220
  SCENE VERTEX -20, 0, -20, 0, 0
  SCENE VERTEX -20, 0,  20, 0, 8
  SCENE VERTEX  20, 0,  20, 8, 8
  SCENE VERTEX  20, 0, -20, 8, 0
  SCENE QUAD 0, 1, 2, 3

  ' Un cubo 2x2x2 appoggiato sul piano.
  SCENE OBJECT 220, 90, 60
  SCENE BOX -1, 0, -1, 1, 2, 1
  SCENE OBJPOS 0, 0, 0, 0

  ' Camera in alto a destra, che guarda verso l'origine.
  SCENE CAMERA 14, 9, 18, 220, -12
  SCENE RENDER

  PRINT "vertici:  "; SCENE.VERTS()
  PRINT "triangoli:"; SCENE.TRIS()
  PRINT "disegnati:"; SCENE.DRAWN()
  PRINT "scartati: "; SCENE.CULLED()
  PRINT "pixel:    "; SCENE.PIXELS()
  PRINT "hash:     "; SCENE.HASH()

  IF SCENE.DRAWN() <= 0 THEN
    PRINT "la camera non vede la scena: controlla CAMERA (yaw/pitch)"
  END IF
END SUB
```

Output misurato su questa macchina (identico sulle quattro gambe, a meno dello spazio finale che
il `PRINT` aggiunge e dell'hash, che è a 32 bit ma stabile per lo stesso frame):

```
vertici:   28
triangoli: 14
disegnati: 11
scartati:  6
pixel:     20767
hash:      2805902046
```

`DRAWN()` + `CULLED()` = 17 > `TRIS()` = 14: i due contatori **non** sono una partizione della
geometria. Un triangolo tagliato dal frustum viene scomposto e contato in `CULLED` (il pezzo
dietro la camera o oltre il bordo) **e** in `DRAWN` (il pezzo che resta); `TRIS()` resta la
geometria dichiarata. Per questo i contatori si leggono insieme, mai come somma.

---

## 6. Che cosa non c'è ancora

Questa è la superficie di **P3**, il primo rasterizzatore mesh. Mancano, e arriveranno con le
fasi successive del piano:

* i comandi che presuppongono uno stato che il kernel non ha: `CLEAR`, `FOG`, `FAR`, `SKY`,
  `COLORMAP`, `THING`, `TURN`, `PLAYER`, `MOVE`, `LIFT`, `LAMP`, `ZONE`, `WORLD`,
  `SOFTWARE`/`HARDWARE`;
* le letture della camera (`SCENE.X/Y/Z/EYE/YAW/PITCH/HIT/…`), perché la camera oggi è solo un
  ingresso;
* le forme con **id oggetto esplicito** (`VERTEX o, …`, `OBJECT o, flags, light`) e le texture da
  file (`TEXTURE n, a$`): oggi l'oggetto corrente lo apre `OBJECT` e la texture è procedurale;
* il present a video e il tetto dei frame: `RENDER` scrive nel target, la cattura è `SAVE`.

Il sample di riferimento è `Samples3D/MeshBasics.bas`; il MiniTest che blinda i numeri è
`shared/minitests/3d/scene_mesh.bas`.
