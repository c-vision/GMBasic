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
| Comando | `SCENE TEXTURE celle` | texture a scacchi procedurale (1…64 celle) |
| Comando | `SCENE TEXTURE "file.png"` | texture da un PNG su disco (stesso verbo, argomento stringa) |
| Comando | `SCENE OBJECT r, g, b [, flags [, luce]]` | apre un oggetto: tinta, flag di materiale e luce base |
| Comando | `SCENE VERTEX x, y, z, u, v` | aggiunge un vertice all'oggetto corrente |
| Comando | `SCENE QUAD a, b, c, d` | due triangoli sui quattro vertici indicati |
| Comando | `SCENE BOX x0, y0, z0, x1, y1, z1` | parallelepipedo: 24 vertici, 12 triangoli, UV per faccia |
| Comando | `SCENE OBJPOS x, y, z, yaw` | posizione e rotazione dell'oggetto corrente |
| Comando | `SCENE CAMERA x, y, z, yaw, pitch` | posizione e orientamento dell'osservatore |
| Comando | `SCENE LIGHT dx, dy, dz, livello` | luce direzionale: illumina per normale di faccia |
| Comando | `SCENE LAMP n, x, y, z, raggio, quantità` | lampada puntiforme (point), attenuazione lineare nel raggio |
| Comando | `SCENE LAMP n, x, y, z, raggio, quantità, dx, dy, dz, angolo` | lampada a cono (spot), semiangolo in gradi |
| Comando | `SCENE FOG d` | buio lineare alla distanza `d` |
| Comando | `SCENE FAR d` | oltre `d` la geometria si salta |
| Comando | `SCENE SKY repeat, shift` | cielo per angolo, con la texture corrente |
| Comando | `SCENE COLORMAP` | luce quantizzata a 32 livelli |
| Comando | `SCENE THING n, x, y, z, w, h` | billboard con la texture corrente |
| Comando | `SCENE SMOOTH on\|off` | filtro della texture: bilineare o nearest |
| Comando | `SCENE ALPHA on\|off` | alpha-test: i texel trasparenti si scartano o si disegnano |
| Comando | `SCENE TWOSIDED on\|off` | facce visibili da entrambi i lati (niente back-face cull) |
| Comando | `SCENE RENDER` | rasterizza la scena nel target |
| Comando | `SCENE PRESENT` | copia il target 3D nella pagina 2D corrente |
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

### `SCENE TEXTURE celle` / `SCENE TEXTURE "file.png"`

Definisce la texture della scena. È **un solo verbo con due forme**, distinte dal **tipo**
dell'argomento: un numero costruisce la scacchiera procedurale, una stringa carica un'immagine.

**Forma numerica — `SCENE TEXTURE celle`**

Scacchiera in scala di grigi di `celle × celle` quadrati, `8` pixel per cella (`1…64` celle ⇒
8…512 pixel di lato). Utile per le prove: non serve nessun file.

**Forma stringa — `SCENE TEXTURE "file.png"`**

Carica un **PNG** dal percorso indicato (relativo alla cwd del processo) e lo usa come materiale
di **tutti** i draw successivi:

```basic
SCENE TEXTURE "foto.png"           ' letterale
p$ = "tiles/granito.png"
SCENE TEXTURE p$                   ' variabile stringa: stessa forma
```

Parametri e semantica:

* **Formato**: solo PNG (RGB, RGBA, palette e trasparenza). Il decode è `png_decode`, l'unico del
  runtime: niente formati diversi, niente improvvisazione per gamba.
* **Dimensioni**: da 1 a **4096** pixel per lato; oltre, il comando è rifiutato.
* **Pixel**: ogni texel è RGBA8; l'alpha **0 scarta il frammento** (alpha-test), quindi un PNG con
  trasparenza ritaglia la geometria (foglie, grate, decal).
* **Orientamento**: la riga 0 del PNG resta in **alto** (convenzione di `gm_tex`), la stessa che
  usano le facce di `BOX` con UV `0…1`.
* **Filtro**: bilineare con wrap ripetuto: `SCENE VERTEX … 0, 10` fa ripetere la texture 10 volte.
* **Moltiplicazione**: il colore finale è texture × tinta dell'oggetto × luce di faccia. La tinta
  quindi **colora** la texture (`OBJECT 255, 200, 120` = caldo), non la sostituisce né la cancella.
* **Atomico**: se il file manca o non è un PNG valido, **la texture precedente resta** e la scena
  continua a disegnare con quella. L'errore si legge da `gmb_3d_last_error()` (come `WORLD LOAD`).
* **Durata**: la texture sopravvive a `SCENE RESET` (che azzera geometria e camera, non il
  materiale); per tornare alla scacchiera basta un `SCENE TEXTURE <celle>`.

Senza nessuna `TEXTURE` gli oggetti si disegnano col solo colore pieno della tinta: non è un
errore. La forma stringa è disponibile su **tutte e quattro le gambe** (C99Native, IL, PCodeVM,
VM/C) con gli stessi byte: la texture vive nel runtime C, le gambe gestite chiamano la stessa
facciata `gmb_scene_texture_file`.

Il PNG può essere RGB, RGBA (con trasparenza, utile con `SCENE ALPHA`) o a palette, con i
filtri standard delle righe: si leggono anche i PNG scritti da altri programmi, non solo quelli
salvati da `SCENE SAVE`.

MiniTest: `shared/minitests/3d/scene_texture_file.bas` (round-trip RENDER → SAVE → TEXTURE, con
anche il caso del file inesistente). Sample: `Samples3D/TextureFile.bas`.

### `SCENE OBJECT r, g, b [, flags [, luce]]`

Apre un **nuovo oggetto** e lo rende quello corrente: i `VERTEX`, `QUAD` e `BOX` successivi
appartengono a lui. `r, g, b` sono `0…255` e valgono come **tinta** (moltiplicata per la texture e
per la luce della faccia). Un oggetto senza vertici non disegna nulla. Il numero massimo di
oggetti è 256, di vertici 32768 e di triangoli 16384: oltre, il comando è rifiutato e la scena
resta quella di prima.

Con **cinque** argomenti si impostano anche i **flag di materiale** e la **luce base**
dell'oggetto:

| argomento | significato |
|---|---|
| `flags` | maschera: **1** masked (alpha-test), **2** traslucido (blend, disegnato per ultimo), **16** due lati. Gli altri bit si ignorano |
| `luce` | luce base `0…255` (default `255`), a cui le **lampade si sommano** |

La forma a tre argomenti resta identica (base `255`), quindi i programmi P3 non cambiano byte.
Flag e luce valgono per **quell'oggetto**, non per la scena: è la differenza rispetto a
`SMOOTH`/`ALPHA`/`TWOSIDED`, che sono globali. `flags` con bit `1` forza il masked anche con
`SCENE ALPHA OFF`; il bit `2` rende l'oggetto traslucido e lo sposta in fondo all'ordine di
disegno.

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

### `SCENE LIGHT dx, dy, dz, livello`

Accende una **luce direzionale** (un sole, una lampada all'infinito) e da quel momento ogni
faccia è illuminata dal proprio orientamento: il motore calcola la **normale del triangolo** sulle
posizioni MONDO (dopo posizione e rotazione dell'oggetto) e la confronta con la direzione della
luce.

| parametro | significato |
|---|---|
| `dx, dy, dz` | direzione **verso la luce** (il vettore `L` di Lambert). Viene normalizzata dal runtime; `0, 1, 0` = luce dall'alto, `0, -1, 0` = dal basso, `1, 0, 0` = da destra. Una direzione nulla è un errore esplicito. |
| `livello` | luce `0…255` sulle facce **rivolte** alla sorgente (clampato). |

Come si calcola: `cos = max(0, normale · direzione)`, il coseno è quantizzato una volta in un
intero `0…255`, e la luce della faccia è una miscela **intera** fra un ambiente fisso di `40` e
`livello`: `luce = 40 + (livello - 40) * cos / 255`. Niente virgola nell'ultimo passo, quindi le
quattro gambe danno **gli stessi byte** (la sede del calcolo è una sola, il runtime C).

Effetti sul resto della scena:

* la luce della faccia si moltiplica con la **tinta** dell'oggetto e con la **texture**, come prima:
  ombreggiare non cambia i colori, li scurisce;
* con `LIGHT` attiva la luce calcolata **sostituisce** la tabella fissa delle sei facce del cubo
  (`BOX`), che era una stilizzazione: i cubi diventano coerenti con l'orientamento reale;
* `LIGHT` è **stato del materiale**, come `TEXTURE`: sopravvive a `SCENE RESET`. Un secondo
  `SCENE LIGHT` sostituisce direzione e livello;
* senza `LIGHT` la scena si disegna esattamente come prima: la tabella per faccia del cubo resta,
  e gli hash dei frame non cambiano (è quello che blinda il MiniTest `scene_mesh`).

```basic
SCENE LIGHT 0.4, 0.9, 0.2, 235     ' sole alto a destra
SCENE RENDER
```

MiniTest: `shared/minitests/3d/scene_light.bas` (piano in su illuminato, piano laterale
illuminato e in ombra: `16777215`, `16777215`, `2631720`). Sample: `Samples3D/LightFaces.bas`
(il sole gira attorno a torri e pilastri).

### `SCENE LAMP n, x, y, z, raggio, quantità`

Accende una **lampada puntiforme**. `n` è lo slot `0…7` (otto lampade per scena, come
`M-LIMITS`): riaccendere lo stesso slot la sostituisce, senza dover spegnere le altre.

| parametro | significato |
|---|---|
| `n` | slot della lampada, `0…7` |
| `x, y, z` | posizione nel mondo (`y` è l'altezza) |
| `raggio` | distanza entro cui illumina; `0` la spegne |
| `quantità` | luce aggiunta sul punto più vicino, `0…255` (clampata); `0` la spegne |

L'attenuazione è **lineare**: a distanza `d < raggio` la lampada aggiunge
`quantità × (1 − d/raggio)`; oltre il raggio non arriva nulla. La luce si somma alla luce base
dell'oggetto (quella di `OBJECT … , luce`) e si ferma a `255`: non si sovraespone oltre il
bianco. Il calcolo è **per pixel**, nel punto del mondo interpolato, quindi una faccia grande si
schiarisce solo dove la lampada è vicina.

```basic
SCENE OBJECT 255, 255, 255, 0, 40     ' base bassa: la lampada si vede
SCENE LAMP 0, 0, 1.5, 0, 2.5, 255     ' torcia accesa al centro
SCENE RENDER
```

MiniTest: `shared/minitests/3d/scene_lamp_point_spot.bas` (`13948116` al centro, `2631720`
all'angolo, fuori dal raggio).

### `SCENE LAMP n, x, y, z, raggio, quantità, dx, dy, dz, angolo`

La **stessa** lampada con un **cono**: dieci argomenti invece di sei, e il verbo è lo stesso — la
forma la decide il **numero di argomenti**, non un verbo diverso.

| parametro | significato |
|---|---|
| `dx, dy, dz` | direzione **verso ciò che si illumina** (normalizzata dal runtime); una direzione nulla è un errore esplicito |
| `angolo` | **semiangolo** del cono in gradi, `0 < angolo < 90`: pieno sull'asse, zero al bordo |

Dentro il cono vale `spot = (cos θ − cos angolo) / (1 − cos angolo)`, limitato a `0…1`, e si
moltiplica per l'attenuazione del raggio; fuori dal cono la lampada non aggiunge nulla. Un
`angolo` fuori da `(0, 90)` è un errore esplicito: non esiste un cono degenere silenzioso.

```basic
SCENE LAMP 0, 0, 2, -1, 6, 255, 0, 0, 1, 25   ' riflettore dal davanti, cono stretto
```

MiniTest: `shared/minitests/3d/scene_lamp_point_spot.bas` — con lo stesso raggio la puntiforme
illuminerebbe anche l'angolo; lo spot lo lascia a `2631720` (solo ambiente).

### `SCENE FOG d` · `SCENE FAR d`

Due limiti di distanza, entrambi **lineari** e indipendenti:

| comando | effetto | `0` |
|---|---|---|
| `SCENE FOG d` | la luce si moltiplica per `1 − distanza/d`: alla distanza `d` il pixel è nero | spegne il fog |
| `SCENE FAR d` | i triangoli la cui distanza media supera `d` sono **saltati** (contano in `CULLED()`) | spegne il limite |

`d` negativa è un errore esplicito. Il fog agisce sulla **luce** (un oggetto in ombra resta nero),
non è una nebbia colorata: il colore del fondo non viene miscelato.

MiniTest: `shared/minitests/3d/scene_fog.bas` (`8421504` = `0x808080` a metà distanza, `0` oltre)
e `shared/minitests/3d/scene_far.bas` (geometria oltre il limite: fondo `988969` e `DRAWN() = 0`,
poi di nuovo `16711680` alzando `FAR`).

### `SCENE SKY repeat, shift`

Riempie i pixel **non coperti** con la texture corrente mappata **per angolo** attorno
all'osservatore: il cielo non scrive la profondità e non vince mai un depth test, quindi la
geometria resta davanti.

| parametro | significato |
|---|---|
| `repeat` | quante volte la picture copre i 360°; `<= 0` vale `1` (un giro) |
| `shift` | sfasamento in **giri** (`0.5` = mezzo giro) |

**Entrambi obbligatori.** Il runtime **copia** la texture al momento della chiamata: un
`SCENE TEXTURE` successivo non cambia il cielo già impostato. Senza texture il comando è un
errore esplicito — un cielo nero silenzioso sembrerebbe un difetto del renderer. `RESET` spegne
il cielo.

```basic
SCENE TEXTURE "sky.png"
SCENE SKY 6, 0.25      ' sei giri e un quarto di sfasamento
```

MiniTest: `shared/minitests/3d/scene_sky.bas` (picture 8×8 a bordo verde e centro rosso: `65280`
al centro, `16711680` spostandosi, e il centro che diventa rosso con `shift 0.5`).

### `SCENE COLORMAP`

Accende la **quantizzazione a 32 livelli** della luce (`C-SAME-COLORMAP`): dopo lampade e fog,
`q = (luce × 31 + 127) / 255` e il livello diventa `q × 255 / 31`. Il default è **spento**, così
le scene P3 che non lo chiamano danno gli stessi byte di sempre; `RESET` lo spegne.

```basic
SCENE LIGHT 0, 1, 0, 100
SCENE COLORMAP        ' 100 → 98: la rampa a 32 livelli
```

MiniTest: `shared/minitests/3d/scene_colormap.bas` (`6579300` senza, `6447714` = `0x626262` con).

### `SCENE THING n, x, y, z, w, h`

Un **billboard**: un rettangolo texturizzato con la texture corrente, sempre rivolto
all'osservatore, con la **base** in `(x, y, z)` e `y` = altezza (la convenzione della superficie
`SCENE`). `w` e `h` sono larghezza e altezza in unità del mondo e devono essere `> 0`; `n` è lo
slot `0…255`. Il disegno è `masked` (alpha `0` = trasparente) e **due lati**, con depth test, e
usa la stessa pipeline delle mesh: niente seconda via di rasterizzazione. La luce del thing è
piena (`255`), salvo la facciata C `gmb_scene_thing_light`, che il BASIC non espone ancora.

I thing si disegnano **dopo** gli opachi e **prima** dei traslucidi, da lontano a vicino. Un thing
senza texture viene saltato, e una scena di **soli thing** si disegna lo stesso.

```basic
SCENE TEXTURE "tree.png"
SCENE THING 0, 0, 0, 3, 1, 2      ' un albero 1×2 in piedi a (0,0,3)
```

MiniTest: `shared/minitests/3d/scene_thing.bas` (`16711680` sul corpo, `8943360` sul bordo
bilineare, `988969` sotto la base).

### Ordine di disegno

`SCENE RENDER` segue un ordine fisso (`M-ORDER`): **cielo** → oggetti **opachi e masked** →
**thing** da lontano a vicino → **traslucidi** da lontano a vicino. I traslucidi si miscelano con
il fondo: un texel con **alpha 255** vale **metà e metà** (`50/50`), uno con **alpha < 255** vale
quella frazione (`M-PALALPHA`); masked e opachi ignorano l'alpha del texel e scrivono il pixel.
Il blend usa la distanza del pixel, quindi due vetri sovrapposti si compongono nell'ordine
corretto.

La scena può essere fatta di **solo cielo** o di **soli thing**: `RENDER` non pretende una mesh.

### `SCENE SMOOTH on|off` · `SCENE ALPHA on|off` · `SCENE TWOSIDED on|off`

Tre **flag di materiale** che il kernel aveva già (sono i campi `bilinear`, `masked` e
`two_sided` di `gmb_3d_material_t`) e che il BASIC non raggiungeva. Sono interruttori: `on`
oppure `off` (si accetta anche `1`/`0`), e valgono per **tutta la scena**, perché il materiale
in `SCENE` è uno solo.

| flag | `on` | `off` | default |
|---|---|---|---|
| `SMOOTH` | filtro **bilineare**: texture morbide, sfumate | **nearest**: texel netti, "pixel art" | `on` |
| `ALPHA` | **alpha-test**: i texel con alpha `0` sono scartati e si vede il fondo | i texel trasparenti si disegnano col loro RGB (i buchi diventano quadrati pieni) | `on` |
| `TWOSIDED` | le facce si vedono da **entrambi** i lati | **back-face cull**: le facce girate di schiena finiscono in `SCENE.CULLED()` | `off` |

I default sono quelli storici, quindi una scena che non chiama questi verbi dà **esattamente**
gli stessi byte di prima (i MiniTest `scene_mesh` e `scene_texture_file` restano verdi). I flag
sono stato del materiale e **sopravvivono a `SCENE RESET`**, come `TEXTURE` e `LIGHT`:
reimpostali esplicitamente se vuoi cambiarli a metà scena.

```basic
SCENE TEXTURE "FoliageAlpha.png"    ' PNG RGBA: i texel trasparenti sono buchi
SCENE ALPHA ON                       ' i buchi si scartano
SCENE TWOSIDED ON                    ' lastre visibili da dietro
SCENE RENDER
```

`ALPHA` ha senso solo con una texture che porti trasparenza: la scacchiera procedurale di
`SCENE TEXTURE n` è sempre opaca. MiniTest: `shared/minitests/3d/scene_material_flags.bas`
(alpha-test, cull e hash bilineare/nearest). Sample: `Samples3D/MaterialFlags.bas` (fogliame
con buchi attorno a un anello di lastre, tre fasi con i flag scambiati).

### `SCENE RENDER`

Rasterizza tutti gli oggetti nel target: clip contro i sei piani del frustum, divisione
prospettica, back-face, z-buffer, UV prospettici, campionamento bilineare con alpha-test. I
contatori (`DRAWN`, `CULLED`, `PIXELS`) sono **azzerati e ricalcolati** a ogni chiamata, e
`HASH()` diventa lo stesso numero per due frame identici — è la prova che il frame non porta
stato da un render all'altro.

`RENDER` non disegna nulla e restituisce un errore se manca il target (`VIEW` mai chiamato) o se
la scena non ha oggetti/triangoli. **Non** è un errore chiamarlo più volte: è il ciclo normale.

### `SCENE PRESENT`

È il **ponte** fra il 3D e il 2D: copia il **target 3D** (quello di `SCENE VIEW`/`SCENE RENDER`)
nella **pagina 2D corrente** — la stessa pagina in cui scrivono `PSET`, `LINE`, `PRINT` — e non
tocca né la geometria né il frame 3D, che restano quelli dell'ultimo `RENDER`.

```basic
SCENE VIEW 320, 180, 55
SCENE RENDER
SCENE PRESENT              ' il 3D diventa lo sfondo della pagina
LINE (1, 1)-(318, 1), 3    ' HUD 2D disegnato SOPRA
```

#### Ordine: `RENDER` → `PRESENT` → 2D sopra

`PRESENT` non rasterizza: copia. L'ordine quindi conta, e nell'ordine sbagliato il risultato è
vuoto senza nessun errore:

1. `SCENE RENDER` scrive nel **target** del 3D;
2. `SCENE PRESENT` copia il target nella pagina 2D (dall'angolo in alto a sinistra);
3. i comandi 2D successivi disegnano **sopra** il 3D, sulla stessa pagina.

Un `PRESENT` prima del primo `RENDER` è rifiutato: senza target non c'è niente da copiare. Un
`RENDER` dopo `PRESENT` **non** aggiorna la pagina: la copia è un fotogramma, non un
collegamento, quindi il ciclo corretto è `RENDER`, `PRESENT`, poi il 2D. Chiamare `PRESENT` due
volte di fila dà lo stesso risultato della prima (il target non è cambiato).

#### Copia 1:1, nessuna scalatura

La copia è **pixel per pixel** con lo stesso formato (il target e le pagine contengono lo stesso
valore `0x00RRGGBB`), **dall'angolo in alto a sinistra** e **ritagliata** alle due dimensioni più
piccole: se il target è `8×8` in una pagina `16×16`, riempie solo l'angolo in alto a sinistra e
il resto della pagina resta quello che c'era prima. Non c'è uno "stiramento" implicito: per
occupare tutta la pagina il target deve avere le **stesse dimensioni** (`SCENE VIEW` con la
larghezza e l'altezza della pagina, come fa `Samples3D/PresentLoop.bas` con `320×180`).

`PRESENT` **non** azzera la pagina: sovrascrive la zona che il target copre, lascia il resto
intatto. È per questo che la pagina va preparata prima (sfondo 2D, `CANVAS CLEAR` o `CLS`).

#### Canvas o `SCREEN`: cambia solo la destinazione

La destinazione è sempre `active_page()`, la pagina 2D attiva, e si sceglie con i comandi 2D già
esistenti — `PRESENT` non ha varianti per l'uno o per l'altro:

| destinazione | come si attiva | che cosa succede |
|---|---|---|
| **canvas offscreen** | `CANVAS NEW w, h INTO c` + `CANVAS USE c` | caso headless: la pagina non è visibile, ma è leggibile con `CANVAS GET`, salvabile in PNG con `CANVAS SAVE` e confrontabile con `SCENE.HASH()` |
| **pagina dello schermo** | `SCREEN 12` (o `SCREEN 12, w, h, 32`) | la pagina è quella visibile: il **pump SDL** la mostra nella finestra al tick successivo |

Se nessuna pagina 2D è attiva, il comando è rifiutato con l'errore
`SCENE PRESENT: nessuna pagina 2D attiva (SCREEN o CANVAS USE prima)`: un canvas creato e mai
selezionato con `CANVAS USE` non conta.

#### Regola GPM

`PRESENT` scrive **memoria** (target → pagina) e va eseguito **dentro un task GPM**, che è l'unico
esecutore di codice BASIC (§0ter): è il task che esegue il ciclo di gioco a copiare, mai il
thread di un evento o di rete. Fuori da un task, con lo scheduler GPM attivo, il comando è
rifiutato in modo esplicito (`E_NO_TASK`, "fuori dal task GPM") invece di scrivere da un thread
che non possiede la pagina; nei tool e negli oracoli, dove lo scheduler non è attivo, la stessa
chiamata funziona in modo sincrono.

`PRESENT` **non tocca SDL**: Cocoa/SDL si chiamano solo dal main thread, il BASIC gira sul worker.
Con `SCREEN` la catena è: il worker copia nel buffer della pagina, il pump sul main thread la
posta a video al tick successivo. È anche il motivo per cui non c'è un "present" di SDL dentro
questo comando: lo fa già il pump della finestra.

MiniTest: `shared/minitests/3d/scene_present.bas` — un quad rosso in un target `8×8`, una pagina
`16×16` con sfondo palette 1: dentro il target si legge `1090453504` (`0xFF0000`), fuori resta
`1073741994` (`0x400000AA`), e una seconda `PRESENT` lascia il valore identico. Sample:
`Samples3D/PresentLoop.bas` — 24 fotogrammi `320×180` su canvas, con `RENDER` → `PRESENT` → HUD
2D sopra e `CANVAS SAVE` per ogni frame; stampa `frame= 24  hash= 464449865  corner= 1074730793  centro= 1086957446  bordo= 1074730793` (identico sulle quattro gambe).

Disponibile su **tutte e quattro le gambe** (C99Native, IL, PCodeVM, VM/C) con gli stessi byte:
la copia vive una volta sola nel modulo grafico che possiede le pagine (`gmb_gfx_present_argb`),
e il 3D fornisce solo il puntatore e le dimensioni del frame.

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
| un oggetto resta nero | luce base `0` e nessuna lampada, oppure `FOG` più corto della distanza della scena |
| il cielo non compare | `SCENE SKY` senza `TEXTURE`, oppure `RESET` dopo `SKY` |
| `THING` invisibile | texture mai impostata, `w`/`h` non positivi, o slot `> 255` |
| una lampada non si vede | slot `> 7`, `raggio 0`, `quantità 0`, oppure oggetto già a luce `255` |
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

La superficie `SCENE` copre oggi **22 verbi e 7 letture** (P3 + P4). Mancano, e arriveranno con le
fasi successive del piano:

* `CLEAR`, `TURN`, `PLAYER`, `MOVE`, `LIFT`, `ZONE`, `WORLD`, `SOFTWARE`/`HARDWARE`;
* le forme a **id oggetto esplicito** (`VERTEX o, …`, `OBJECT o, flags, light`, `BOX o, …`), la
  faccia singola (`FACE`) e le texture con handle numerico (`TEXTURE n, a$`); oggi l'oggetto
  corrente lo apre `OBJECT` e la texture è una sola per scena (PNG o scacchiera);
* i comandi per-thing (`THING POS/TEX/OFF/LIGHT/TRANS/MOVE`): il BASIC ha solo
  `THING n, x, y, z, w, h`, con luce piena e la texture corrente;
* le letture della camera e degli oggetti (`SCENE.X/Y/Z/EYE/YAW/PITCH/HIT/OBJ*`), perché camera e
  oggetti oggi sono solo ingressi;
* il present **a video** e il tetto dei frame: `SCENE PRESENT` porta il target nella pagina 2D
  (canvas o `SCREEN`), e la finestra la mostra il pump SDL al tick successivo; il controllo del
  frame rate resta fuori dal dominio.

MiniTest che bloccano i numeri: `shared/minitests/3d/scene_mesh.bas`,
`scene_texture_file.bas`, `scene_light.bas`, `scene_material_flags.bas`, `scene_present.bas` per
la P3, e `scene_lamp_point_spot.bas`, `scene_fog.bas`, `scene_far.bas`, `scene_sky.bas`,
`scene_colormap.bas`, `scene_thing.bas` per la P4 — tutti con gli stessi valori su C99Native, IL,
PCode e VM/C. Sample: `Samples3D/MeshBasics.bas`, `TextureFile.bas`, `LightFaces.bas`,
`MaterialFlags.bas`, `PresentLoop.bas`.
