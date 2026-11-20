# Provenienza e licenza del motore 3D

> **Scopo**: rendere verificabile la regola «il codice si riscrive senza copiarlo». Ogni file del motore 3D deve poter rispondere a due domande: *da dove viene questo comportamento?* e *questo testo è nostro?*
> **Regola in una riga**: si porta la **semantica** (cosa fa e con quali numeri), mai l'**espressione** (codice, tabelle, struttura, commenti, asset).

---

## 1. Perché questo documento esiste

Il riferimento principale (`ekkehard-morgenstern/Engine1999`) è **GPL-3.0** con un'eccezione che copre solo i programmi scritti nel *suo* linguaggio. GMBasic ha licenza diversa: mescolare codice del riferimento renderebbe il motore un'opera derivata e contaminerebbe la distribuzione.

C'è anche un fatto scomodo da mettere per iscritto: **la stessa persona ha letto i sorgenti del riferimento e poi scritto la specifica**. La "clean room" in senso stretto (due squadre separate da un muro) qui non c'è. Quindi:

- non si dichiara "clean room";
- si dichiara **reimplementazione da specifica di comportamento**, con questo registro come prova del metodo;
- ciò che è verificabile non è l'intenzione, è il **risultato**: nel nostro codice non deve esistere espressione del riferimento.

---

## 2. Cosa è lecito portare

| Lecito | Esempi | Perché |
|---|---|---|
| **Semantica osservabile** | "un portale disegna i gradini superiore/inferiore e restringe la finestra di righe" | È il funzionamento, non la scrittura. |
| **Fatti funzionali numerici** | step 24, occhio 46, `PITCH` −80…80, 64 portali per raggio, near 2, 8 lampade | Sono costanti di compatibilità, non espressione creativa; servono alla parità. |
| **Regole di gioco** | collisione con scivolamento, `SHOOT` che si ferma sul vetro smerigliato, `SEES` al 70 % dell'altezza | Descrivono un comportamento, non un'implementazione. |
| **Nomi di capacità** | "c'è un comando che sposta il pavimento a una quota a velocità data" | Il *nome* del nostro comando lo scegliamo noi. |
| **Invarianti di authoring** | "ogni raggio deve finire su una parete", "un portale non è più alto del più basso dei due settori" | Sono scoperte di comportamento, utili e non proprietarie. |

## 3. Cosa non è lecito portare

| Vietato | Perché |
|---|---|
| Righe di codice, anche riscritte "cambiando i nomi" | È l'espressione, non l'idea (e la riscrittura pedissequa è derivazione). |
| Tabelle precalcolate, array di costanti, rampe di palette già pronte | Sono dati espressivi del riferimento; le nostre si costruiscono a runtime. |
| Layout di struct, ordine dei campi, split dei moduli, nomi interni | È architettura del riferimento: la nostra è quella del piano §3. |
| Commenti, nomi di funzioni interne, messaggi d'errore testuali | Testo originale. |
| Schema JSON, mappe di esempio, librerie di texture/sprite | Opere del riferimento (e le texture sarebbero comunque asset di terzi da licenziare). |
| Struttura dei file di livello/generatori | È il loro modello dati, non un requisito nostro. |

**Zona grigia da trattare con cautela**: i *nomi delle API*. La specifica usa `W3D`/`M3D` solo per **riferirsi** al modello studiato; la superficie BASIC di GMBasic è `WORLD`/`SCENE`, con verbi scelti da noi. Nessun file `.inc` deve contenere un nome del riferimento.

---

## 4. Fonti consultate (registro)

| # | Fonte | Tipo | Cosa ne è stato preso |
|---|---|---|---|
| 1 | `Engine1999/docs/3d-subsystem.md` | documentazione | modello settori/portali, default e limiti, flag, semantica di movimento/raggi/vista, tempo, lampade, traslucenza, invarianti di authoring, elenco dei "non fatti" |
| 2 | `Engine1999/docs/3d-mesh-subsystem.md` | documentazione | modello oggetti/thing/zone, maschera `faces`, ordine di disegno, clip near 2, formula del cono, alpha di palette, schema del documento mondo, parità hardware misurata, difetti noti |
| 3 | `Engine1999/docs/graphics-subsystem.md`, `docs/world-model.md` | documentazione | composizione a layer, modello di mondo per settori, formato dei documenti |
| 4 | sorgenti del riferimento (`gfxops.h`, `instset.h`, `w3d.c`, `m3d.c`, `m3dsoft.c`, `m3dgl.c`) — **letti in una fase precedente** | codice | **solo comportamento osservato**; nessuna riga, tabella o struttura riportata nel nostro repo |
| 5 | Doom wiki (map format, linedef/sidedef/sector, texture alignment, light diminishing, sky) · `fabiensanglard.net` (Doom/Build renderer internals) · Lode Vandevenne (raycasting, floor casting, sprites) | articoli | vocabolario tecnico e casi limite (settori non chiusi, self-referencing, seam di texture, thin walls, cicli di portali) |
| 6 | Khronos (glTF 2.0, OpenGL/Vulkan clipping e interpolation, depth test, sampler) | specifiche | clip near/far, UV prospettici (`1/w`), ordine opachi/masked/traslucidi, mip, convenzioni UV/winding/assi |
| 7 | Microsoft DX9 (attenuation, spotlight, fog formulas) · Gaffer on Games (fixed timestep, lockstep) · Godot/SDL docs (input map, audio 3D, `SDL_RenderPresent` main-thread) | articoli/documentazione | lampade point/spot, fog lineare/exp, passo fisso e replay, mappa input, audio posizionale, thread di present |
| 8 | `gmbsuite` / `shared/libs/graphics` (codice **nostro**) | codice nostro | riuso diretto: math, camera, ray, mesh, raster, colormap |

Nessuna fonte è stata copiata in `docs/` o nel codice; le righe 1–8 sono citazioni di studio.

---

## 5. Procedura per chi implementa (checklist di revisione)

Prima di ogni commit nell'area 3D:

1. **Origine**: la funzione che sto scrivendo ha un ID in `docs/3D-Specifica-Comportamenti.md`? Se no → manca la specifica, non il codice.
2. **Espressione**: il codice contiene nomi, tabelle o strutture riconducibili al riferimento? Se sì → riscrivere secondo la nostra architettura, non "rinominare".
3. **Numeri**: ogni costante nuova è nella specifica con il suo oracolo? Altrimenti è un numero inventato.
4. **Asset**: le immagini usate sono nostre o generate? (Nessun file del riferimento, nessuna texture di terzi.)
5. **Testo**: commenti e messaggi d'errore sono scritti da noi, e gli errori **nominano l'elemento** (`objects[3].boxes[1]`), non citano messaggi altrui.
6. **Dichiarazione**: se il comportamento diverge dal riferimento, la divergenza è in §4 della specifica *prima* del codice.

---

## 6. Rischi residui e mitigazioni

| Rischio | Stato | Mitigazione |
|---|---|---|
| Riscrittura troppo aderente alla struttura del riferimento | **aperto** | l'architettura nostra è data dal piano §3 (suite/runtime divisi, stato nel runtime); nessuna funzione nostra ha la firma o il nome di una loro |
| Costanti "prese in blocco" senza capirle | aperto | ogni costante ha un oracolo nella specifica; se non si sa perché vale, non entra |
| Asset di terzi finiti nel repo per comodità | aperto | `F-TEXTURES`: solo asset nostri/generati; nessuna immagine del riferimento |
| Perdita del registro nel tempo | aperto | questo file si aggiorna nello stesso commit che aggiunge una fonte nuova |
| Revisione indipendente assente | **non risolto** | se serve una garanzia più forte, la revisione va fatta da chi **non** ha letto il riferimento, partendo dalla sola specifica |

---

## 7. Decisione registrata

- **2026-11-19** — Il motore 3D di GMBasic è una **reimplementazione da specifica di comportamento**, non un port. Si studiano il riferimento e altri progetti per capire *cosa deve succedere*; il codice, le tabelle, gli schemi e gli asset sono nostri. La specifica (`docs/3D-Specifica-Comportamenti.md`) è la fonte di verità per il comportamento; questo registro è la fonte di verità per la provenienza.
