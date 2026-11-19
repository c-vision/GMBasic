# Specifica di comportamento del motore 3D — oracolo per MiniTest e golden image

> **Stato**: specifica di comportamento, **nessun codice**. È il documento che rende verificabile la parità con `Engine1999` e che dice, funzione per funzione, cosa deve crescere in `gmbsuite`.
> **Metodo**: il riferimento è stato studiato per **capire cosa fa** (documentazione e comportamento osservabile). Il codice si riscrive da zero: vedi `docs/Provenienza-3D.md` per cosa è lecito portare (semantica, non espressione) e per il registro delle fonti.
> **Uso**: ogni voce ha un **ID** (`W-…` per settori, `M-…` per mesh, `C-…` condivisi, `T-…` tempo, `F-…` formati). Nel piano e nei MiniTest si cita l'ID, non la frase.

---

## 0. Come si usa questa specifica

1. **Prima di scrivere una funzione** si cerca qui il suo ID: se manca, la funzione non ha oracolo e non si scrive.
2. **Ogni ID diventa almeno un controllo**: un MiniTest headless (valori stampabili con `' EXPECT:`) o una golden image, secondo §7.
3. **Le divergenze sono dichiarate, non silenziose**: dove la specifica dice *diverso dal riferimento*, il motivo è scritto accanto. Nessuna divergenza non documentata.
4. **I numeri sono oracoli**: i valori riportati (default, limiti, sentinelle) sono il contratto verificabile. Se si cambiano, si aggiorna la specifica e il test, insieme.

---

## 1. Convenzioni comuni (`C-…`)

| ID | Comportamento | Oracolo |
|---|---|---|
| `C-AXES` | Assi: `x` est, `y` nord, `z` su. **Angoli in gradi**: 0 = +x, 90 = +y (antiorario su una mappa con y in su); alla destra di chi guarda è orario. | `BEARING` e `TURN` restituiscono i valori attesi su 4 direzioni cardinali e 4 diagonali. |
| `C-UNITS` | Unità libere; **1 unità = 1 pixel di texture** sulle superfici testurizzate (pareti, box). | Una texture 64×64 su un muro lungo 64 copre esattamente la larghezza. |
| `C-PITCH` | Pitch in gradi, **−80 … +80**, positivo = guarda in su. | Oltre il limite viene bloccato, non avvolto. |
| `C-WIND` | Triangolo **antiorario visto dal fronte**: la normale `(b−a)×(c−a)` punta all'osservatore. Un oggetto a una faccia non disegna né collide dal retro. | Un triangolo noto dà normale con segno atteso; `two-sided` disegna entrambi i lati. |
| `C-INDEX0` | **Alpha 0 del texel = trasparente** (nel riferimento: indice 0 di palette) dove serve (thing, oggetti masked/translucent, cielo); su parete o pavimento **pieno** lascia il pixel trasparente, cioè mostra i layer sotto. | Scena con texture bucata sopra un layer colorato: il colore sotto resta visibile. |
| `C-NOPIC` | Numero di texture mai definito → si disegna **`palette_rgb(n)` del runtime** (non un errore, non il magenta): lo stesso colore che darebbe il 2D. | `TEXTURE` non chiamata: il pixel ha esattamente il colore di `palette_rgb(n)`. |
| `C-SENTINEL` | Sentinelle: `−1` = **entità assente** ovunque (`TON`, `SECTORAT`, `ZONE`, `USE`, `SEES`); "fuori dal mondo" per le quote `−1000000`; `SEES` **`1` = vede, `0` = non vede, `−1` = thing inesistente** (corretta rispetto al riferimento: `C-DIVERGE-SEES`, §12.3); `WALLS(s)` = il numero che prenderà la prossima parete. | Un controllo per sentinella, con il caso "fuori" e il caso "dentro". |
| `C-LAYER` | Il 3D disegna **in un layer** indicato dal target grafico, sostituendo i pixel della vista; l'HUD si disegna sopra con le primitive 2D. Lo scambio di buffer è del programma, non del renderer. | Scena resa su page A, HUD su B, swap: il fotogramma composto contiene entrambi. |
| `C-NOENV` | Nessun parametro passa da variabili d'ambiente: tutto da statement o dal registro opzioni. | Grep di `getenv` sul codice nuovo: zero. |
| `C-CAPTURE` | **Il fotogramma si cattura dal target 3D** (`gmb_3d_target_capture_argb` / `capture_png`), **mai** dalla finestra: `gmb_gfx_present_capture` richiede video reale e main thread e in headless restituisce `-1`. La cattura è una copia senza scalature: dimensioni diverse sono un errore esplicito. | Golden per RID con `gmb_3d_oracle --check` (0 differenze in software) e `--perturb 1` che **deve** dare `DIFF`. |
| `C-GPM` | **Ogni comando e funzione 3D gira solo dentro un task GPM**: fuori da un task (main thread, job T2, hook) il comando produce un **errore esplicito**, non un no-op; ciò che aspetta una risorsa (file, modelli, texture) **parca** il task e riprende; gli handler (`ON KEY`/`ON MOUSE`/`ON COLLISION`/…) sono dispatchati dal GPM e il disegno avviene nello stesso task che li ha drenati. Il ciclo di gioco è park-based (`gmb_yield_ticks`), mai `gmb_sleep_ms` né spin. Vedi nota tecnica `R1`, `R3`, `R10`, `R11`, §14. | Comando 3D chiamato da un hook/job: errore atteso; `LOAD` che non blocca il worker (contatore di park > 0); `RENDER` dentro un handler funziona e non cambia task; `grep gmb_sleep_ms` = 0. |

---

## 2. `WORLD` — settori e portali (modello `W3D`)

### 2.1 Modello dati

| ID | Comportamento |
|---|---|
| `W-MODEL` | Un settore ha **pavimento e soffitto** (quota), **due texture** (pavimento, soffitto), **livello di luce**, **tag** libero. È delimitato da **pareti** (segmenti 2D); una parete con un settore dietro è **portale**. I **thing** sono immagini piatte che stanno sul pavimento e guardano sempre l'osservatore. |
| `W-ORDER` | Le pareti si elencano **in senso antiorario** attorno al settore (visto dall'alto): così la texture scorre da sinistra a destra. I buchi (pilastri) si elencano **in orario**. Un portale è elencato **in entrambi i settori**, con gli stessi estremi e il proprio `next`. Nessun controllo automatico: un ordine sbagliato specchia l'immagine. |
| `W-LIMITS` | Settori `0…4095`, thing `0…1023`, texture `0…1023` fino a `1024×1024`. Ogni raggio attraversa **al massimo 64 portali**. Al massimo **4 pareti traslucide per colonna**. |
| `W-DEFAULT` | Settore nuovo: texture pavimento e soffitto = **1**, luce = **255**. `WALL` senza `next` = parete solida (`−1`). Vista predefinita `0,0,640,300` a **scala 2**; `FOG` = **1200** (0 = nessun oscuramento per distanza); `PITCH` −80…80. |
| `W-GHOST` | Un **settore fantasma** si vede solo attraverso i portali: non è mai il settore di una posizione (camera, thing, `SECTORAT`) e la collisione ignora le sue pareti. Serve per sfondi sovrapposti ad altre stanze. |

### 2.2 Disegno

| ID | Comportamento | Oracolo |
|---|---|---|
| `W-COLUMN` | **Un raggio 2D per colonna di schermo**, tracciato dentro i settori. Si trova la parete più vicina del settore corrente, si disegnano le righe di soffitto e pavimento fino a lì; parete solida → la colonna finisce; portale → si disegnano i gradini superiore/inferiore, si **restringe la finestra di righe ancora aperte** e il raggio prosegue nel settore dietro. Nessun BSP, nessun ordinamento delle pareti; forme concave e buchi funzionano. | 8 direzioni da un punto noto: immagine identica alla golden, pixel per pixel. |
| `W-DEPTH` | Il parametro del raggio è la **profondità lungo la direzione di vista**: niente correzione fish-eye. I piani sono mappati **per pixel** (prospettici), le pareti **per colonna**. | Muro a 45°: altezza costante lungo la colonna, nessun incurvamento. |
| `W-SHEAR` | Guardare su/giù muove l'orizzonte: è uno **shear** dell'immagine, non un tilt vero. | Pitch ±30°: l'orizzonte si sposta linearmente, le verticali restano verticali. |
| `W-DBUFFER` | Un **depth buffer float per pixel** permette di disegnare thing e pareti masked in qualunque ordine. | Un thing dietro una parete bassa non compare sopra il cielo (`W-THINGVIS`). |
| `W-THINGVIS` | **Un thing si disegna solo se il suo settore è stato raggiunto da almeno una colonna** (prima del 2026-11-08 si disegnava tutto con il solo depth test, e i mobili dietro un muretto apparivano sopra il cielo). | Stanza chiusa con thing dentro, vista da fuori: nessun thing visibile. |
| `W-ANCHOR` | Le texture di parete sono **ancorate alla quota del mondo** (bordo inferiore a multipli dell'altezza dell'immagine) e si ripetono; pavimenti e soffitti sono ancorati alle `x, y` del mondo. | Stessa texture su due pareti contigue: nessun salto verticale. |
| `W-ALIGN` | `WALLOFF s,i,xoff,yoff` sposta l'immagine della parete; `WALLFLAG … 1` (masked) **non si ripete in verticale**: poggia sul pavimento più alto e `yoff` la alza. | Offset noti: il texel in (0,0) della parete è quello atteso. |
| `W-SKY` | `SKY tex[,repeat[,shift]]`: il cielo è mappato **per angolo** attorno all'osservatore, `repeat` volte (0.1…64, default 1), `shift` righe su/giù. La parete con flag **8** mostra il cielo per tutta la sua altezza (orizzonte dipinto). | Rotazione completa: il cielo si avvolge esattamente `repeat` volte. |
| `W-LIGHT` | La luminosità è `luce del settore × distanza` via **32 livelli di luce in fixed point** (la rampa che `COLORMAP` definisce; niente tabella di indici, §11); prima della prima chiamata tutti i livelli sono identità → nessuna ombreggiatura. Lampade: fino a **8**, point, falloff **lineare** a zero su `radius`, solo schiariscono (**niente luci colorate**). | Con `COLORMAP` non chiamata il colore è quello della texture; dopo, il **livello** atteso per una data distanza (0…32). |
| `W-TRANS` | Portale con flag **16**: la picture `mid` è **miscelata metà e metà** con ciò che sta dietro; texel con alpha 0 trasparente; poggia sul pavimento più alto e non si ripete in verticale. Miscela = **media per canale** in fixed point (`(a+b)>>1`): niente tabella `256×256`, niente ricostruzione dopo `COLORMAP` (§11). Le pareti traslucide si disegnano **dopo i thing**. | Parete traslucida davanti a un thing: il thing si vede attraverso; il colore è la **media per canale** attesa. |
| `W-INDEX0-SOLID` | Su parete/floor **pieno** (non masked) l'indice 0 lascia il pixel trasparente → si vedono i layer sotto. | Scena sopra un layer colorato: buchi del colore del layer. |
| `W-SLOPE` | `SLOPE s, kind, gx, gy[, ox, oy]` inclina pavimento (0), soffitto (1) o entrambi (2): `gx, gy` = salita per unità in x e y (`|gx|,|gy| ≤ 100`) attorno a `(ox, oy)` (default: centro delle pareti), dove il settore mantiene le quote date. Seguono il piano: pareti, gradini dei portali, piani, thing (i piedi), i piedi dell'osservatore, la collisione (il gradino si misura dove il corpo attraversa la parete), colpi e vista. Il piano è risolto **per pixel** dal raggio. | Rampe al 5 %: quota attesa in 4 punti noti; il gradino limite resta 24. |

### 2.3 Movimento, collisione, raggi

| ID | Comportamento | Oracolo |
|---|---|---|
| `W-MOVE` | `MOVE forward,right` cammina **relativo alla vista**, con collisione: scivola lungo le pareti, sale gradini fino a **step** (24 di default), passa i portali in cui il corpo entra (apertura ≥ altezza del giocatore). Imposta `HIT` con bit **1** = una parete ha spinto, **2** = non si è mossi affatto, **4** = cambio di settore. | 10 casi headless: muro frontale, muro obliquo (scivolamento), gradino 24 sì / 40 no, portale basso no, portale alto sì, angolo, cambio settore. |
| `W-PLAYER` | Default: giocatore **56** alto, occhio a **46**, settore alto **128**, step **24**. L'occhio **insegue il pavimento** con una frazione per frame disegnato (40 %): **dipendente dal frame rate** → *diverso da noi* (vedi `T-FIXED`). | Due frame rate diversi devono dare la stessa posizione dell'occhio (nostro comportamento). |
| `W-EYE-CLAMP` | L'occhio è limitato a **2 unità sotto il soffitto**. | Soffitto basso: l'occhio non lo attraversa. |
| `W-CAMERA` | `CAMERA x,y,angle[,pitch[,lift]]` con errore "posizione fuori da ogni settore"; `lift` si somma all'altezza dell'occhio (salto, accovacciarsi). `SECTORZ(x,y,z)` risolve le stanze sovrapposte: settore col pavimento **più alto ≤ z** e soffitto sopra; altrimenti quello col pavimento più vicino. | Camera in punto fuori: errore; punto in due settori sovrapposti: si ottiene quello giusto a due quote diverse. |
| `W-SHOOT` | `SHOOT(range)` spara lungo la vista (anche su/giù col pitch): restituisce il **thing** più vicino colpito (entro **0.4 × larghezza** dal centro, fra i piedi e la cima), `−1` se nessuno. Pareti e porte chiuse fermano il colpo; i portali lo lasciano passare se l'apertura c'è **alla quota del colpo**. Poi `SHOTKIND` 0/1/2 (niente/parete/thing), `SHOTDIST`, `SHOTX`, `SHOTY`. | Bersaglio dietro un vetro smerigliato (flag 4): nessun colpo; dietro un portale aperto: colpo. |
| `W-USE` | `USE(reach)` restituisce il settore dietro il portale davanti entro la portata, `−1` se la parete più vicina è solida o non c'è nulla (è il test "apri la porta"; le porte portano un `TAG`). | Porta davanti vs parete piena davanti. |
| `W-SEES` | `SEES(n)` = **`1`** se il thing **ha** linea di vista verso l'occhio (da **70 %** della sua altezza all'occhio), **`0`** se non ce l'ha, **`−1`** se `n` non è un thing esistente (§12.3). Bloccano: porte chiuse, pareti solide, vetro smerigliato (flag 4). Il trace **si ferma anche sul pavimento e sul soffitto** del settore in cui si trova. | Thing al piano di sopra: non "visto" attraverso il soffitto. |
| `W-FLAGS` | Flag parete: **1** masked (la `mid` si disegna sopra l'apertura, indice 0 trasparente, non ripete in verticale, poggia sul pavimento più alto), **2** il movimento non passa (finestra, ringhiera), **4** non passano colpi e vista (vetro smerigliato), **8** la parete mostra il cielo, **16** traslucida. **I flag sono di un lato**: su una finestra vanno messi su entrambe le pareti. | Matrice 5 flag × 3 raggi (movimento, colpo, vista) = 15 casi. |

### 2.4 Tempo

| ID | Comportamento |
|---|---|
| `T-TICK` | Il mondo ha un orologio **che il programma avanza** (`TICK secondi`): pilota `SECMOVE` (unità/secondo), `ANIM` (texture `first…first+count−1` a `fps`), `SCROLL` (pavimento/soffitto/pareti, unità/secondo), `LIGHTFX` (1 flicker casuale fra `luce−amount` e `luce`, 2 pulse sinusoidale ±`amount`, 3 strobe a mezzo periodo; `rate` al secondo). `TIME` legge l'orologio; `LIGHTNOW` la luce dell'ultimo frame. |
| `T-SECMOVE` | `SECMOVE s, velocità, pavimento[, soffitto]` porta le quote a quei valori; `MOVING(s)` è `−1` mentre si muovono; `HEIGHT` ferma un mover. Porte/ascensori/trituratori: il programma avvia e interroga. |
| `T-FIXED` | **Diverso dal riferimento**: noi usiamo un **passo fisso** (default 1/60) con accumulatore e limite di catch-up, e l'interpolazione dell'occhio è funzione del tempo simulato, non del frame rate. Motivo: determinismo e golden image riproducibili. |
| `T-CLOCK` | Il tempo si legge in **tick interi** (oltre ai secondi), così animazioni e porte sono riproducibili: stesso seed + stessi input → stessa sequenza di frame. |

### 2.5 Invarianti di authoring (scoperte a proprie spese dal riferimento)

Queste non sono regole del renderer: sono **condizioni che l'autore del mondo deve rispettare**, e che noi controlliamo con un validatore (`W-*` → `tools/…/world_check`).

| ID | Invariante | Sintomo se violata |
|---|---|---|
| `W-INV-RAY` | **Ogni raggio deve finire su una parete**: nessun raggio deve uscire dal mondo. | Spazio vuoto/nero oltre il bordo. |
| `W-INV-HEIGHT` | Un settore che si guarda attraverso deve essere **almeno alto** quanto ciò che sta dietro; un portale **non è mai più alto del più basso dei due settori**. | Casa tagliata in altezza; cielo dove dovrebbe esserci il muro. |
| `W-INV-FLOOR` | Il soffitto di un settore deve stare **sopra** il pavimento. | Settore degenere, divisioni per zero. |
| `W-INV-HOLE` | Un buco (pilastro/apertura nel pavimento) **non deve toccare** le pareti del settore né un altro buco. | Fessure nere nel disegno. |
| `W-INV-CCW` | Ordine antiorario delle pareti, orario dei buchi, portale elencato da entrambi i lati con gli stessi estremi. | Immagine specchiata o portale che non si attraversa. |
| `W-INV-GHOST` | Un settore fantasma non deve essere il settore di una posizione. | Camera/thing "dentro" uno sfondo, collisioni assurde. |

---

## 3. `SCENE` — mesh (modello `M3D`)

### 3.1 Modello dati

| ID | Comportamento |
|---|---|
| `M-MODEL` | Il mondo è un insieme di **oggetti** (`0…16383`): mesh di triangoli con posizione, **yaw attorno a z**, livello di luce `0…255` e flag. La mesh è in coordinate dell'oggetto. `THING` (`0…1023`) sono billboard: picture `w×h` che guarda sempre l'osservatore, base a `z`. |
| `M-BUILD` | Costruzione: `VERTEX o,x,y,z[,u,v]` (vertici numerati da 0 per oggetto), `FACE o,a,b,c,tex`, `QUAD o,a,b,c,d,tex`, `BOX o,x1,y1,z1,x2,y2,z2,tex[,faces]`. `faces` è una **maschera**: 1 `+x`, 2 `−x`, 4 `+y`, 8 `−y`, 16 sopra, 32 sotto, default **63**. Senza `u,v` le coordinate texture seguono la posizione secondo la direzione della faccia. |
| `M-BOXANCHOR` | Le picture dei box sono **ancorate all'oggetto** (1 texel = 1 unità): due box adiacenti combaciano. |
| `M-FLAGS` | **1** masked, **2** translucido (miscela metà e metà, disegnato per ultimo, da lontano a vicino), **4** passabile (niente collisione, niente pavimento/soffitto), **8** see-through (passano colpi, `USE` e vista: vetro, ringhiera), **16** due lati. |
| `M-LIMITS` | Oggetti `0…16383`, thing `0…1023`, lampade **8**, clip near **2**, zone per gioco. |

### 3.2 Disegno

| ID | Comportamento | Oracolo |
|---|---|---|
| `M-CULL` | Ogni triangolo è scartato se: faccia posteriore (salvo `two-sided`), dietro l'osservatore, fuori schermo. Poi **clip sul piano near = 2** e rasterizzazione con **z-buffer** e **UV corretti in prospettiva**. | Triangolo a cavallo del near: il poligono clippato ha i vertici attesi. |
| `M-ORDER` | Ordine: **cielo** → oggetti **opachi e masked** → **thing da lontano a vicino** (con depth test) → **traslucidi da lontano a vicino**. | Sovrapposizione vetro/thing: il thing si vede attraverso; due vetri: ordine corretto. |
| `M-LIGHT` | Luce per pixel = **luce dell'oggetto** + lampade + fog, con la riga di color map scelta da `COLORMAP`. Lampada: `amount × (1 − d/radius)`; con direzione e semiangolo è un **cono**: `spot = (cos θ − cos semiangolo) / (1 − cos semiangolo)` limitato a 0…1 (pieno sull'asse, zero al bordo). Solo schiarisce, **nessuna luce colorata**. | Lampada sull'asse: valore atteso; a 76° fuori cono: buio; puntata altrove: buio. |
| `M-FOG` | `FOG d`: buio alla distanza `d`. `FAR d` salta ciò che è oltre. | Due oggetti a distanze note: righe di colormap attese. |
| `M-SKY` | Cielo = picture mappata **per angolo** attorno all'osservatore. | Rotazione di 360°: avvolgimento esatto. |
| `M-THINGS` | Thing disegnati **da lontano a vicino**, con depth test, base a `z`; contano per `SHOOT`/`SEES`. | Due thing sovrapposti: vince il vicino. |
| `M-PALALPHA` | In un oggetto **translucido**, un texel con **alpha < 255** (canale alpha della texture) si miscela con quell'alpha (0 = niente); alpha 255 = metà e metà. Masked e opachi **ignorano** l'alpha del texel. | Valori esatti: `75 = 50×3/4 + 150/4`, e alpha 0 = nessun disegno. |
| `M-NOPIC` | Numero di picture senza picture → `palette_rgb(numero)`: un vetro colorato non richiede picture. | Vetro con `palette_rgb` a cui è stato dato alpha 0: si vede attraverso, senza texture. |

### 3.3 Collisione, raggi, zone

| ID | Comportamento |
|---|---|
| `M-PLAYER` | Capsula verticale: default **raggio 14, altezza 56, occhio 46, step 24**. `CAMERA x,y,z,yaw[,pitch]` posiziona i **piedi**. |
| `M-COLLIDE` | Stessa semantica di `W-MOVE`: scivola, sale gradini ≤ step, bloccato da soffitto basso; `HIT` uguale. |
| `M-RAYS` | `SHOOT(range)` contro thing e oggetti (`SHOTKIND` 0/1 oggetto/2 thing, `SHOTOBJ`, `SHOTDIST`, `SHOTX/Y/Z`); `USE(reach)` l'oggetto davanti **qualunque siano i flag**; `SEES(n)` linea di vista del thing verso l'occhio. Passano solo i flag 8 (non 4) per vista/colpi. |
| `M-FLOOR` | `FLOORAT(x,y,z)` = **superficie più alta a o sotto z**; `CEILAT` l'analogo sopra. È ciò che permette di stare su un mobile senza una fisica generale. |
| `M-ZONE` | `ZONE id,x1,y1,x2,y2,z1,z2[,tag]`: `ZONE(x,y,z)` = id della **prima** zona che contiene il punto (`z` entro `z1−2 … z2`), `−1` se nessuna; `ZONETAG` il tag. **L'ordine di creazione conta**: le zone dei piani superiori e dei mobili calpestabili vanno create **prima** di quelle sotto. Il renderer non usa le zone. |
| `M-PASSABLE` | Flag 4: nessuna collisione e nessun pavimento/soffitto (auto, oggetti decorativi). Sul pavimento ci si passa attraverso **visivamente**, ma `FLOORAT` continua a vederlo. |

---

## 4. Comportamenti condivisi e divergenze dichiarate

| ID | Tema | Riferimento | Noi |
|---|---|---|---|
| `C-DIVERGE-TIME` | Tempo | `TICK` a secondi, occhio che insegue il pavimento al 40 % **per frame disegnato** | **Passo fisso**, interpolazione sull'occhio legata al tempo simulato (`T-FIXED`) |
| `C-DIVERGE-JSON` | Formato mondo | due dialetti: `world-model` (settori) e `e99-mesh-world` (mesh) | **un solo schema** `gmb-3d` v1: un documento per file, `kind: "sectors" \| "mesh"` **obbligatorio**, estensione convenzionale `.gmbscene` (`F-SCHEMA`, deciso in **§12.1**) |
| `C-DIVERGE-SEES` | Sentinella di `SEES` | `−1` = **vede** (invertita rispetto a tutto il resto) | `1` = vede, `0` = non vede, `−1` = **thing inesistente**: `−1` significa "assente" in tutta la superficie (specifica §12.3; la conversione degli script presi dal riferimento va in guida, P9) |
| `C-DIVERGE-HW` | Hardware | GL 2 caricato a runtime, con parità misurata (media < 1,2/255) | stesso approccio, **rinviato** e solo con parità misurata (fase `PH`) |
| `C-SAME-COLORMAP` | Luce | **32 livelli** di luce in fixed point; traslucenza = media per canale | **stessa meccanica, non identico** (target ARGB, §11): il *banding* a 32 livelli resta, le tabelle `256×256` no |
| `C-SAME-CONSOLE` | Test headless | l'host console esegue il mondo **ma non disegna** | identico: `RENDER` in **null mode** per i MiniTest (`T-NULL`) |
| `T-NULL` | Modalità nulla | `RENDER` ritorna subito, lo stato avanza lo stesso | identico, ed è la base dell'oracolo headless |

---

## 5. Cosa **non** portiamo dal riferimento (e perché)

1. **Aritmetica intera/fixed** del raster: il riferimento è integer-heavy per velocità su CPU del 1999. Noi usiamo `gm_real` (double) con oracoli espliciti; dove serve velocità si misura (§0septies), non si cambia tipo in silenzio.
2. **Tabelle precalcolate e costanti di stile** (rampe di palette, tabelle di traslucenza pronte): le nostre si **costruiscono a runtime dalla palette**, come già fanno `gm_gfx_colormap` e la tabella lazy.
3. **Struttura interna dei moduli** (nomi di struct, ordine dei campi, split dei file): la nostra architettura è quella di §3 del piano, non la loro.
4. **Il loro JSON**: prendiamo i *concetti* (environment/start/zones/things/objects), non il testo dello schema.
5. **Difetti noti**: occhio dipendente dal frame rate, shear oltre ~30°, niente buchi fra piani sovrapposti, 4 traslucidi per colonna. Sono **limiti dichiarati**, non requisiti: alcuni li superiamo (buchi, tilt), altri li teniamo come limite esplicito perché costano più di quanto valgono.

---

## 6. Oracoli: come si verifica ogni gruppo

| Gruppo | Verifica | File previsti |
|---|---|---|
| `C-AXES`, `C-PITCH`, `C-WIND`, `C-SENTINEL` | MiniTest headless, valori stampati con `' EXPECT:` | `shared/minitests/3d/scene_math.bas` |
| `C-GPM` | Selfcheck C (hook/job fuori dal task, park del `LOAD`) + MiniTest `RENDER` dentro un handler | `gpm_compat.bas`, `tools/c/gmb_3d_gpm_selfcheck.c` |
| `W-MODEL`, `W-DEFAULTS`, `W-LIMITS` | MiniTest: query su settore appena creato (valori di default), errori sui limiti | `world_model.bas` |
| `W-COLUMN`, `W-DEPTH`, `W-SHEAR`, `W-ANCHOR`, `W-ALIGN` | **Golden image**, 4 direzioni + pitch ±30°, confronto `IMGDIFF` | `world_golden1..8` |
| `W-DBUFFER`, `W-THINGVIS`, `W-TRANS` | Golden + MiniTest su `VISITED`/`DRAWN` | `world_depth.bas`, golden |
| `W-MOVE`, `W-SHOOT`, `W-USE`, `W-SEES`, `W-FLAGS` | MiniTest headless: 10–20 casi con `HIT`, `SHOTKIND`, `SEES` | `world_move.bas`, `world_rays.bas` |
| `W-INV-*` | 6 mappe rotte apposta: **il `LOAD` fallisce** e `ON ERROR` controlla l'elemento nominato (`F-ERRORS`); mappe buone: anche il tool `tools/sh/check_3d_world.sh` in CI (§12.2) | `world_invariants.bas` + fixture JSON |
| `M-CULL`, `M-ORDER`, `M-LIGHT`, `M-FOG`, `M-SKY`, `M-THINGS`, `M-PALALPHA` | Golden image (10 scene) + MiniTest numerici su colore atteso | `scene_golden1..10` |
| `M-BUILD`, `M-FLAGS`, `M-ZONE`, `M-FLOOR` | MiniTest: conteggi, maschera `faces`, zona "prima creata vince", `FLOORAT` su piani sovrapposti | `scene_model.bas` |
| `T-*` | MiniTest di replay: stesso input + stesso seed → stessa sequenza di posizioni e di frame | `time_replay.bas` |
| `F-*` | Round-trip `LOAD → SAVE → LOAD` identico + rifiuto di versioni future | `format_roundtrip.bas` |
| Parità software/hardware | 10 scene, media ≤ 2/255, pixel diversi ≤ 2 %, **golden per RID** | `tools/sh/check_3d_renderers.sh` |

**Regola delle golden**: una golden vale **per RID** (arm64/x64, `cc`/`zcc` possono divergere nei float). Se una scena non è riproducibile fuori dal RID che l'ha generata, si dichiara e si tiene solo come riferimento visivo — non come gate.

---

## 7. Formati (`F-…`)

| ID | Comportamento |
|---|---|
| `F-SCHEMA` | Un solo schema: `{ "format": "gmb-3d", "version": 1, "kind": "sectors"|"mesh", "environment": {...}, "start": {...}, "textures": [...], "objects"|"sectors": [...] }`, **un documento per file**, estensione convenzionale `.gmbscene` (il loader non la impone). `kind` **obbligatorio** e unico: `WORLD` rifiuta `kind:"mesh"` e `SCENE` rifiuta `kind:"sectors"`; un documento non può contenere entrambi i payload (v2). Versionamento esplicito e distinto dalla versione del build. Motivi del formato unico: §12.1. |
| `F-ERRORS` | Gli errori **nominano l'elemento** (`objects[3].boxes[1]: servono 7 numeri`). Un mondo che non si carica dice *dove*. |
| `F-MIGRATE` | `version` minore → migrazione dichiarata (tabella di conversioni); `version` maggiore → **rifiuto esplicito**, non interpretazione best-effort. |
| `F-ROUNDTRIP` | `LOAD → SAVE → LOAD` produce lo stesso documento canonico (chiavi ordinate, numeri normalizzati) — è il test che tiene onesto il formato. |
| `F-TEXTURES` | Le texture sono riferite per **nome/indice** con dimensioni dichiarate; il formato non incorpora immagini di terzi (niente asset GPL: solo asset nostri o generati). |

---

## 8. Cosa deve crescere in `gmbsuite` (contratto per dominio)

> Solo nomi e semantica: nessuna implementazione. Colonne: dominio → funzioni nuove (una per riga) → oracolo.

### 8.1 `gmbgfx-3d` (già esteso in P0 con `gm_mesh_attrs` e `gm_tex`)

| Dominio / file | Funzioni da aggiungere | Oracolo |
|---|---|---|
| `gm_gfx_raster` (estensione) | target **ARGB `0x00RRGGBB`** + depth; blending traslucido con alpha della texture; mip-lite; blit/clip per layer | `M-PALALPHA`, `W-TRANS`, `C-INDEX0` |
| `gm_gfx_sector` (nuovo) | settore/parete/portale: creazione, validazione, risoluzione punto→settore (con quota), portali per raggio, quote con pendenza | `W-MODEL`, `W-INV-*`, `W-SECTORZ` |
| `gm_gfx_light` (nuovo) | livelli di luce in fixed point (rampa `COLORMAP`), lampada point (falloff lineare), lampada cono, fog lineare | `W-LIGHT`, `M-LIGHT`, `M-FOG` |
| `gm_gfx_scene` (nuovo) | ordinamento (opachi/things/traslucidi), culling per oggetto/frustum, zone | `M-ORDER`, `M-CULL`, `M-ZONE` |
| `gm_gfx_anim` (nuovo) | keyframe TRS, skinning (oltre il riferimento), interpolazione a tempo simulato | `T-FIXED` |

### 8.2 Monorepo — runtime (`native/gmb_runtime_c`)

| File | Contenuto | Oracolo |
|---|---|---|
| `gmb_3d_world.c` | settori, pareti, flag, thing, query `WORLD.*` | `W-*` |
| `gmb_3d_frame.c` | vedere/clip, target (pagina o canvas), null mode, present; **esiste dal 2026-11-19 (P1)**: `gmb_3d_target_*`, cattura headless, guardie di render, `alloc_count` — nessun header SDL, nessuna dipendenza `gm_*` | `C-CAPTURE`, `C-LAYER`, `T-NULL`, `R3`, `R5` |
| `gmb_3d_scene.c` | oggetti, thing, lampade, zone, query `SCENE.*` | `M-*` |
| `gmb_3d_move.c` | corpo, movimento con scivolamento/gradini/portali, `HIT` | `W-MOVE`, `M-COLLIDE` |
| `gmb_3d_ray.c` | `SHOOT`/`USE`/`SEES` sui due modelli, con flag 2/4/8 | `W-SHOOT`, `M-RAYS` |
| `gmb_3d_time.c` | passo fisso, `TICK`, mover, animazioni, scroll, lightfx | `T-*` |
| `gmb_3d_io.c` | schema `gmb-3d`, errori per elemento, migrazione, round-trip; **usabile anche fuori dal GPM** (park se `gmb_cur()!=NULL`, sincrono altrimenti), così il tool `world_check.c` e l'oracolo P1 non avviano lo scheduler (`R11`) | `F-*`, `C-GPM` |
| `gmb_3d_check.c` | validatore `W-INV-*` con messaggi che nominano l'elemento, chiamato **dal `gmb_3d_io` durante il load** (violazioni fatali) e dal tool `tools/c/world_check.c`; **nessun comando BASIC** (§12.2) | `W-INV-*` |

**Regola di confine**: la matematica e le primitive restano in `gm_gfx_*` (suite, testabile da sola); lo **stato** e la semantica dei comandi stanno nel runtime, perché le due istanze della dylib non condividono memoria (vedi piano §3.1.3 e spike P0). Nessuna funzione nuova nasce in `gmb_runtime_c` se è pura matematica, e nessuna logica di gioco nasce nella suite.

---

## 9. Rinunce dichiarate (non "dimenticanze")

1. **Nessun** room-over-room con buco visibile: le stanze sovrapposte si uniscono con scale/rampe (come il riferimento); i buchi fra piani restano un'estensione possibile, non un requisito P0–P5.
2. **Nessun** tilt vero prima di P5: shear fino a ~30°, come il riferimento, poi valutazione.
3. **Nessuna** luce colorata nel path a palette: le lampade schiariscono; il colore arriva solo nel path hardware (`PH`), dichiarato.
4. **Nessun** multi-angolo per gli sprite: i thing guardano sempre l'osservatore.
5. **Nessun** asset di terzi: texture nostre o generate; il formato non incorpora immagini GPL.

---

## 10. Domande aperte

> Le quattro questioni di formato/semantica sono chiuse il **2026-11-19**: sotto la risposta in una riga, i motivi in **§12**. Le domande di thread/present sono nella [nota tecnica](Nota-Tecnica-Thread-Present-3D.md) §13, anch'esse chiuse.

1. **Palette o ARGB** — **CHIUSA: ARGB**, target `0x00RRGGBB`, luce a 32 livelli in fixed point, alpha dalle texture. Motivo, conseguenze sul ciclo caldo e cosa si perde: **§11**.
2. **Un solo schema JSON o due file** — **CHIUSA: un solo schema `gmb-3d`, un documento per file, `kind` obbligatorio** (`.gmbscene`). Motivi, costo e cosa la ribalterebbe: **§12.1**.
3. **`WORLD CHECK`: comando o tool** — **CHIUSA: né un comando né un tool come unica via**; le invarianti sono **fatali nel load** e il tool C `tools/c/world_check.c` serve alla CI e alle mappe costruite a statement. Motivi in **§12.2**.
4. **`SEES` con sentinella invertita** — **CHIUSA: si corregge** (`1` vede, `0` non vede, `−1` thing inesistente), divergenza dichiarata (`C-DIVERGE-SEES`). Motivi in **§12.3**.

---

## 11. Decisione: target ARGB (2026-11-19)

> Gli ID di §4 che parlano di "indice di palette" (`C-INDEX0`, `M-PALALPHA`, `C-NOPIC`) descrivono il **riferimento**: accanto è indicato l'equivalente ARGB, che è il contratto nostro.

### 11.1 Cosa serve davvero (verificato nel codice, non dedotto)

- **Le pagine del 2D non sono indicizzate.** `resolve_color()` (`gmb_graphics.c`) risolve l'indice in `0xRRGGBB` **al momento della scrittura**: la pagina contiene word RGB, `gmb_gfx_visible_pixels()` le restituisce così come sono e il present le carica come `ARGB8888`. `PALETTE i,r,g,b` cambia solo i disegni successivi: **non ricolora** i pixel già scritti.
- **La palette nel runtime è un modo di nominare i colori**, non un modello di memoria: `gmb_gfx_point()` fa il percorso inverso (cerca l'indice il cui RGB coincide, altrimenti restituisce l'RGB), e il flag `0x40000000` serve solo a distinguere un argomento "indice" da un argomento "RGB" nelle API.
- **`palette_rgb(idx)` è pubblico** (`gmb_graphics_internal.h`): un modulo 3D può chiedere il colore di un indice senza toccare le pagine.
- **Non esiste alcuna macchina di fade di palette** da preservare: un target indicizzato non renderebbe possibile nulla che oggi manchi, e aggiungerebbe un secondo modello di colore da tenere coerente col primo.

### 11.2 Cosa serve davvero agli effetti a rampa

| Effetto del riferimento | Cosa produce il "look" | In ARGB |
|---|---|---|
| color map a 32 righe | **quantizzazione della luce su 32 gradini** — il banding *è* il look | livello 0…32 in fixed point: **stesso banding** |
| traslucenza `256×256` | media di due colori **snappata alla voce di palette più vicina** | media per canale: differenza solo per colori assenti in palette |
| alpha di palette | trasparenza per texel | canale alpha della texture: uguale, per texel invece che per indice |
| indice 0 trasparente | maschera | alpha 0 |
| "colore pieno dell'indice n" | colore di palette per picture mancante | `palette_rgb(n)`: **identico**, è la stessa palette del 2D |

Il look a rampa **non richiede indici: richiede quantizzazione**. Le tabelle sono un dettaglio dell'hardware dell'epoca, non la sorgente dell'effetto.

### 11.3 Decisione

Target **ARGB**: word `0x00RRGGBB` (alpha 0, **come le pagine**: il present non è in blend mode), depth nel target (float o 16 bit, si misura in P3), alpha solo nelle texture. **Nessun percorso indicizzato**, nemmeno come opzione: sarebbe un secondo modello di colore da mantenere e da rendere coerente con il primo.

### 11.4 Conseguenze sul ciclo caldo

1. Ordine per pixel: test depth → campiona texel (RGBA8) → `alpha == 0` ⇒ salta → moltiplica l'RGB per il livello di luce → scrivi la word. **Niente lookup di palette, niente tabelle da costruire al primo uso.**
2. Aritmetica **intera fixed point** (`(c * L) >> 5`): goldens **byte-identiche** fra `cc` e `zcc`, cosa che con LUT in float non sarebbe garantita.
3. **Ponte col 2D**: il frame si copia nella pagina mascherando `& 0xFFFFFF`; present, `capture`, `SAVE SCREEN` e `POINT` funzionano senza conversioni né seconde palette.
4. Memoria: 4 B/px di colore contro 1 B/px dell'indicizzato. A 640×300 sono 768 KB contro 192 KB, con depth 768 KB: irrilevante; a 1280×720 restano 3,7 MB.
5. Traslucenza: `(a + b) >> 1` per il 50/50; interpolazione con l'alpha del texel per `M-PALALPHA`.

### 11.5 Cosa si perde (dichiarato)

- Il **banding esatto** della rampa 15×16 del riferimento: resta il banding a 32 livelli, non gli stessi scalini.
- La **media snappata** alla palette: colori che nel riferimento collassavano sulla stessa voce ora restano distinti (più fedele, meno "retrò"). Se servisse il look retrò, un'opzione `posterize` che riconduce i colori alla palette richiede una ricerca del più vicino, che oggi non esiste: **fuori da P3**.
- Le tabelle precalcolate non esistono: niente costo di costruzione (~50 ms nel riferimento) al primo frame, ma 3 moltiplicazioni per pixel.

`C-SAME-COLORMAP` passa da "identico" a "stessa meccanica, quantizzazione dichiarata": è l'unica riga della specifica che cambia significato, ed è il motivo per cui la decisione andava presa **prima** del ciclo caldo.

### 11.6 Costo del ciclo caldo: le due opzioni a confronto

Per pixel coperto, opaco, con depth test (target 640×300 = 192.000 px):

| Voce | Indicizzato (1 B) | ARGB (4 B) |
|---|---|---|
| lettura texel | 1 B (indice) | 4 B (RGBA8) |
| lavoro per pixel | 2 lookup (`colormap[livello][texel]`, LUT da 256 B in L1) | 3 moltiplicazioni + 3 shift (fixed point) |
| scrittura colore | 1 B (o 4 B se si scrive già RGB risolto) | 4 B |
| traffico colore per pixel (R+W) | 2 B | 8 B |
| tabelle | colormap 32×256 = 8 KB + traslucenza 256×256 = 64 KB, ricostruite dopo `COLORMAP` | nessuna |
| texture | serve un formato indicizzato: conversione RGB8/BC1 → indice al caricamento, **secondo percorso** in `gm_tex` | RGBA8, quello che esiste già |
| composizione col 2D | conversione indice→RGB per pixel, ogni frame | `memcpy`, o nessuna copia se si disegna nella pagina |
| alpha per texel | tabella di palette separata (256 valori) | canale alpha, 8 bit |
| blending traslucido | tabella 256×256 (o ricostruzione) | `(a+b)>>1` |

**Il vantaggio dell'indicizzato è solo nel fill**: ~2 B contro ~8 B di traffico colore per pixel e due lookup contro sei operazioni aritmetiche. **Stima** (non misura, §0septies) su 640×300: 0,2–0,5 ms per frame a favore dell'indicizzato, in parte riassorbiti dalla conversione per la composizione. Contro: un secondo modello di colore, un secondo formato di texture, due tabelle da costruire e mantenere, 64 KB di traslucenza contro un'addizione.

Esiste anche la variante "ibrida" (texture indicizzate + LUT di luce, ma **scrittura immediata in ARGB** nella pagina): elimina la conversione a fine frame, e resta comunque con LUT, palette, secondo formato di texture e nessun alpha per texel. Anche la sua versione migliore non guadagna abbastanza da pagare il secondo modello.

**Cosa ribalterebbe la decisione**: una misura in P3 in cui il fill superi ~25% del budget di frame (16,7 ms) **e** la conversione risulti più economica del previsto. Fino a quella misura non si paga complessità per un guadagno non misurato.

---

## 12. Decisioni chiuse il 2026-11-19 (quelle che P2 eredita)

> Formato: **scelta**, **motivo**, **cosa costa**, **cosa la ribalterebbe**. Le decisioni thread/present stanno nella loro sede ([nota tecnica](Nota-Tecnica-Thread-Present-3D.md) §13) perché lì vivono gli invarianti; qui stanno formato, validatore e sentinelle. Tutte e sei sono richiamate nel piano (§3.1, §6 P2/P3, §12).

### 12.1 Un solo schema JSON, `kind` obbligatorio (`C-DIVERGE-JSON`, `F-SCHEMA`)

**Scelta.** Un solo documento `gmb-3d` v1 per file: `environment`, `start`, `textures` comuni, e **uno** dei due payload (`sectors` oppure `objects`) scelto da `kind`. `kind` è obbligatorio e unico; `WORLD` rifiuta `kind:"mesh"`, `SCENE` rifiuta `kind:"sectors"`. Estensione convenzionale `.gmbscene`, non imposta dal loader.

**Motivo.**

1. Un loader, un reporter di errori, una migrazione, un round-trip: i quattro oracoli `F-*` restano **uno per tipo** invece di due copie che divergono alla prima modifica.
2. La parte comune è la maggioranza del documento (`environment`, `start`, `textures`, e in `SCENE` perfino i `things` con un campo in più): due dialetti la duplicherebbero per un discriminante di una parola.
3. Il riferimento ha due dialetti perché aveva **due programmi separati**; noi abbiamo un `gmb_3d_io.c` e due namespace che condividono camera, luci, texture, fog e present (piano §3.3).
4. Il rifiuto incrociato (`WORLD LOAD` su un documento mesh) trasforma l'errore più comune — caricare il file sbagliato — in un messaggio immediato invece che in una scena vuota.

**Cosa costa.** `kind` mancante è un errore, non un default: non esiste un "documento vecchio" da indovinare. I `THING` hanno arità diversa nei due `kind` (`x,y` nei settori; `x,y,z` nelle mesh): è dichiarato nella tabella delle chiavi per `kind`, e non è ambiguo perché un documento non può contenere entrambi.

**Cosa la ribalterebbe.** Un mondo che deve contenere **settori e mesh insieme** (una stanza a settori con un'auto a mesh). Oggi non serve: due documenti, due load. Se servirà, è un `kind:"mixed"` in **v2**, non un secondo schema in v1.

### 12.2 `WORLD CHECK` non è un comando: la validazione sta nel load (`W-INV-*`)

**Scelta.** Tre punti, un uso ciascuno:

1. `gmb_3d_check` gira **dentro il caricamento** (`WORLD`/`SCENE` da JSON): ogni `W-INV-*` violata è **fatale**, il load fallisce con l'elemento nominato (`F-ERRORS`), catturabile con `ON ERROR`.
2. **Nessun comando BASIC**: `WORLD CHECK` non esiste nella superficie canonica.
3. Il controllo in blocco (CI, cartelle di mappe) è il **tool** `tools/c/world_check.c` + `tools/sh/check_3d_world.sh`: linka staticamente `gmb_3d_io.c`/`gmb_3d_check.c` (stesso schema dello spike P0, `tools/c/gmb_3d_link_spike.c`), stampa una riga per violazione, exit code `0` = nessuna violazione, `1` = violazioni, `2` = uso/IO.

**Motivo.**

1. Un comando costerebbe ~8 artefatti (TokenKind, lexer, parser, emitter, StdLib C#, `gmb_audit`, guida, MiniTest — piano §3.4) per un'operazione che serve **prima** di giocare, non durante. La superficie canonica è la risorsa scarsa (piano §12, riga 6).
2. La validazione **deve** comunque stare nel load: `W-INV-FLOOR` su un settore degenere significa divisione per zero durante il disegno, e un mondo che non si carica non deve poter entrare. Averla *anche* come comando sarebbe un secondo punto d'ingresso alla stessa funzione, con due comportamenti da tenere allineati.
3. Le invarianti sono **tutte fatali**: nessuna è un avviso, perché ognuna produce un disegno rotto o una divisione per zero, e "un mondo che si carica ma si vede male" non è mai preferibile a un errore che dice dove. Quindi non c'è un canale di warning da inventare (`WORLD.WARN$` non esiste, e non serve).
4. Le mappe costruite a mano con `SECTOR`/`WALL` non passano dal load: chi le vuole validare fa `WORLD SAVE` e lancia il tool. Un giro su disco in più, solo in sviluppo — e il `SAVE` serve comunque a `F-ROUNDTRIP`.

**Cosa costa.** Un binario in più in `tools_verify.sh`; la mappa scritta a statement non è validata a runtime; chi vuole diagnosticare *senza* far fallire il load deve usare il tool.

**Cosa la ribalterebbe.** Un flusso di authoring che costruisca mondi a statement e li testi in CI senza mai serializzarli. In quel caso l'ingresso dal vivo sarebbe un **hook di debug del tool** (una libreria chiamata dal test C), non un comando BASIC.

### 12.3 `SEES` torna un booleano con sentinella di assenza (`W-SEES`, `C-SENTINEL`)

**Scelta.** `SEES(n)`: `1` = il thing ha linea di vista, `0` = non ce l'ha, `−1` = `n` non è un thing esistente. Divergenza dichiarata (`C-DIVERGE-SEES`) e documentata in guida con la riga di migrazione per gli script presi dal riferimento (P9).

**Motivo.**

1. `−1` = "vede" è l'unico punto della superficie dove un valore negativo significa "sì": `IF SEES(3) THEN` funziona per caso, `IF SEES(3) = 1 THEN` no. È la trappola n.1 per chi scrive un gioco e l'abbiamo già annotata come tale nella specifica.
2. Tutte le altre funzioni usano `−1` = **entità assente** (`TON`, `SECTORAT`, `ZONE`, `USE`): tenerla invertita metterebbe due convenzioni opposte nella stessa tabella, e la tabella è l'oracolo.
3. `−1` = assente ha un uso reale che l'inversione toglie: distinguere "non lo vedo perché c'è un muro" da "quel thing non esiste" è la differenza fra un bug del gioco e un bug della mappa.

**Cosa costa.** I casi presi dai numeri del riferimento vanno riscritti con la sentinella corretta (nessuna golden dipende da `SEES`: le golden sono nostre), e la guida deve dire una volta, chiaramente, che il riferimento faceva il contrario.

**Cosa la ribalterebbe.** Niente: è una scelta di superficie, non di prestazioni. Chi volesse il valore del riferimento *per compatibilità* lo otterrebbe da una routine in `shared/libs/e99compat.inc` (P7), non dal motore.
