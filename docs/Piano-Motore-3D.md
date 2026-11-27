# Piano — Motore 3D di GMBasic (parità Engine1999 e oltre)

> **Stato**: piano di lavoro. **P0–P4 sono chiusi e le quattro gambe li eseguono**: target 3D e cattura headless nel runtime (`include/gmb_3d.h`, `src/gmb_3d_frame.c`), renderer a settori (`shared/libs/graphics/src/gm_gfx_sector.c`), tempo (`src/gmb_3d_time.c`), collisione, **rasterizzatore mesh** (`gmb_3d_mesh_draw`: clip, z-buffer, UV prospettici), oracolo `tools/gmb_3d_oracle.c` con **18 golden** per RID, la mappa demo di P2 e il dominio `SCENE` agganciato al linguaggio su tutte e quattro le gambe. **P4 è chiuso (2026-11-25)**: luce **per pixel** con **lampade point/spot**, **fog**, **FAR**, **SKY**, **COLORMAP** a 32 livelli e **THING**, con l'ordine sky → opachi → thing → traslucidi; 6 MiniTest e 6 golden nuove, stessi valori su tutte e quattro le gambe. Documento di riferimento per le fasi **P0–P9**, che si eseguono in quest'ordine, più `PH` (hardware) fuori numerazione. Il **ponte C#** dei comandi 3D è chiuso: i 20 MiniTest del dominio `WORLD` girano su C99Native, IL, PCode e VM/C. **P5 ha consegnato** culling per oggetto, superficie interattiva, buffer a capacità dinamica (200k triangoli) e, il 2026-11-27, i **raggi** (`SHOOT`/`USE`/`SEES`).
> **Base**: SDL2 (present di GMBasic) + `shared/libs/graphics` (vendored da `gmbsuite`) + `native/gmb_runtime_c`.
> **Riferimento esterno**: [`ekkehard-morgenstern/Engine1999`](https://github.com/ekkehard-morgenstern/Engine1999) — engine SDL2 in C, **GPL-3.0**, con due renderer 3D.
> **Regola di licenza**: di Engine1999 si replica il *comportamento*, mai il codice.
> **Revisione 2**: il piano v1 proponeva di scrivere la math da zero. **Sbagliato**: `gmbsuite/gmbgfx-3d` esiste già ed è vendorizzata qui. Ora il piano la riusa e costruisce l'engine *sopra*.
> **Revisione 3 (2026-11-19)**: il comportamento del riferimento è stato studiato e formalizzato in due documenti che ora fanno parte del piano — [`3D-Specifica-Comportamenti.md`](3D-Specifica-Comportamenti.md) (oracolo con ID: `W-*`, `M-*`, `C-*`, `T-*`, `F-*`, e contratto di crescita di `gmbsuite`) e [`Provenienza-3D.md`](Provenienza-3D.md) (cosa si può portare: semantica e numeri di compatibilità, mai espressione). Ogni funzione del motore deve avere un ID nella specifica *prima* di essere scritta.
> **Nota tecnica (prerequisito di P2/P3)**: [`Nota-Tecnica-Thread-Present-3D.md`](Nota-Tecnica-Thread-Present-3D.md) fissa chi chiama cosa (BASIC/render sul GPM, SDL/present sul main thread), se il frame è sincrono o park, cosa succede se `RENDER` è dentro un handler, e le guardie da aggiungere (`R3`–`R9`).

---

## 0. Vincoli (licenza, perimetro, regole di casa)

1. **Licenza Engine1999 = GPL-3.0** (con `LICENSE-EXCEPTION` solo per programmi nel *suo* linguaggio). GMBasic ha licenza diversa (`LICENSING.md`): **nessuna copia** di codice, header, tabelle, costanti o asset. Parità per **reimplementazione pulita** dalla specifica osservabile (`gfxops.h`, `docs/3d-subsystem.md`, `docs/3d-mesh-subsystem.md`, `docs/graphics-subsystem.md`) e dal comportamento misurato.
2. **Perimetro**: `GMBasic/` dentro il monorepo è libero; **`gmbsuite` è un progetto separato** (suo `AGENTS.md`: non toccare GMBasic; e specularmente noi non tocchiamo gmbsuite senza permesso esplicito). Il piano **prevede** modifiche a `gmbsuite/gmbgfx-3d`: vanno autorizzate prima di scriverle (§11).
3. **Regole di casa GMBasic** che il piano rispetta: logica condivisa solo in `native/gmb_runtime_c` (§0bis), **una sola implementazione**; TokenKind come unica chiave di dispatch; nessuna variabile d'ambiente; file < 1200 righe; GPM per ogni callback; niente Python (`tools/` in BASIC/C/shell); per ogni comando/funzione: **MiniTest + sample + guida** sui 4 backend.

---

## 1. Inventario di Engine1999 (cosa va replicato)

> **L'inventario esecutivo è la specifica**: [3D-Specifica-Comportamenti.md](3D-Specifica-Comportamenti.md). Le sezioni che seguono restano come panoramica; il contratto verificabile (default, limiti, sentinelle, casi limite, invarianti di authoring) è lì, voce per voce con un ID citabile nei MiniTest.
> Studiare il riferimento serve a capire **cosa succede**, non a riprodurne la scrittura: le regole di provenienza sono in [Provenienza-3D.md](Provenienza-3D.md). Sono state studiate anche fonti indipendenti (Doom/Build internals, raycasting di Lode Vandevenne, specifiche Khronos su clip/UV prospettici, formule di attenuazione e fog, passo fisso e replay, vincoli di thread di SDL) per i casi che il riferimento non copre.

Verificato sui sorgenti e sulla doc (144 commit, C 98,8%).

### 1.1 Esecuzione
Compilatore BASIC → bytecode (`bascomp/bastok/baspgm`), interprete, `BUILD` a eseguibile, `instset.h` con 4096 istruzioni estese; il dispatch grafico passa da `GFXC`/`GFXQ` con parametro = **numero di operazione** su tabella. GMBasic ha già 4 backend e `gmb build`: **niente da replicare**; il *modello a tabella* è già quello di GMBasic (TokenKind).

### 1.2 Grafica 2D (`docs/graphics-subsystem.md`)
Schermo **640×300**, **5 layer** in priorità (background → tile → text → graphics → sprites), ogni layer bitmap di **indici palette** con **palette ARGB propria**, priorità scambiabili, backdrop, ridisegno dei soli layer `modified`.
**Tile**: 41×13 celle 16×24, mappa 16×16 (256 immagini), scroll 0..15 px. **Sprite**: 256 (0..247 programma, 248..255 riservati, 255 = puntatore), priorità 0..7, sequenze (256×15), `MOVETO`, **gruppi** `n,SIZE w,h`, **path** (`X+n/W/A/E/[ ]n/R`), collisioni **per pixel opaco** sprite-sprite e sprite-tile con bit indicatori. **Text**: 80×25 celle 8×12, penna/carta per cella, `SYMBOL`, `TXT FILL/COPY/INVERT/SAVE/LOAD`. **Graphics**: penna, `MODE` SET/CLEAR/XOR/OR/AND, PSET/LINE(B/BF)/CIRCLE+archi+ellisse/PAINT/RADIUS/GRAB/BLIT/TEXT scalato/**DRAW** GW-BASIC/LOAD-SAVE BMP con quantizzazione/`VSYNC`/`RESET`. **`DESIGN`/`@`** per disegnare immagini come righe ASCII. **Screenshot** composto.

### 1.3 Toolkit UI
Label, button, check, radio, text field, list, progress, slider, panel, **window trascinabile**, editor multi-riga (selezione/clipboard/undo/caret), tab strip, canvas a celle, grid di caratteri, temi, sintassi colorata, eventi con `KIND/VALUE`, `UI JOY`.

### 1.4 Audio
8 canali logici a 44100 Hz: note/frequenze/glissando, wave 0..3 + **strumenti 4..63**, **ADSR**, vibrato, tremolo, **pan**, **cutoff con glide**, `playchord`, `syncchans`, BPM, **`MML PLAY`**, `busy/silence`.

### 1.5 Altro linguaggio
`JSON` (14 op), `PRINT USING`/`FORMAT$`, `INPUT` a campi, `PEEK/POKE/PEKS`, `ON KEY/JOY/MOUSE/COLLISION/UI/SPRITE` eseguiti **tra due istruzioni**, timer, `SECURITY` sui path.

### 1.6 `W3D` — settori/portali (software, stile Doom/Duke)

* Settori (pavimento/soffitto/texture/luce) + pareti; parete con settore dietro = **portale**; **thing** billboard.
* **Un ray 2D per colonna di schermo** con ricorsione nei portali (max 64/raggio): nessun BSP, nessun sort; pareti textured per colonna, pavimenti/soffitti **per pixel prospettici**; **depth buffer float**; pitch = shear dell'orizzonte.
* Luce: livello settore × distanza → **color map a 32 righe** dalla palette; traslucenza con tabella 256×256 lazy.
* Limiti: settori 0..4095, thing 0..1023, 4 traslucidi per colonna; player 56/occhio 46/step 24; 1 unità = 1 pixel texture.
* Nel motore: **movimento** (sliding, gradini, portali abbastanza alti, `HIT` a bit), **shoot** (raycast), **use** (portale davanti), **sees** (LOS).
* Tempo: `TICK`, `SECMOVE` (porte/ascensori), `ANIM`, `SCROLL`, `LIGHTFX`, `LAMP`, `THINGTRANS`, `SLOPE`, `FAR`, `SKY`, `COLORMAP`, `GHOST`, `WORLD` (JSON).
* Misurato: **0,5 ms/frame a 320×150**, **2 ms a 640×300**.
* Non fatto: hardware, buchi tra piani sovrapposti, tilt vero, luci colorate, sprite multi-angolo, >4 traslucidi/colonna.

### 1.7 `M3D` — mesh (software + OpenGL 2)

* **Oggetti 0..16383** (posizione, yaw, luce, flag), **thing** billboard, **8 lampade** (point o **cono spot**), **zone** AABB con tag, world da JSON.
* Pipeline: cull (back/fuori schermo/dietro), clip **near = 2**, **z-buffer**, **UV prospettici**, luce per pixel (oggetto + lampade) × fog → riga color map; ordine sky → opachi/masked → thing far→near → traslucidi far→near; `FAR`; `DRAWN`.
* Flag: 1 masked, 2 translucent (mix 50/50, con alpha di palette per texel), 4 passable, 8 see-through, 16 two-sided.
* **Due renderer** dietro `m3drend_t`: software a palette e **OpenGL 2 caricato a runtime** (`SDL_GL_LoadLibrary` + `GetProcAddress`, nessuna lib GL linkata), parità verificata (media <1,2/255, solo bordi).
* Misurato: ~1,6 ms a 320×150 software; hardware 1,7–2,9 ms a 640×264 su GPU reale (readback dominante).
* Non fatto: culling spaziale, skeletal, LOD, ombre, HDR.

---

## 2. Cosa ha **già** GMBasic (evidenza)

### 2.1 Runtime grafico 2D

| Area | Stato | File |
|---|---|---|
| Framebuffer software, pagine, palette VGA/EGA, primitive, TTF, texture PNG, canvas, tilemap, chart, collisioni, screenshot | **Sì** | `gmb_runtime_c/src/gmb_graphics.c` (1071) |
| Present SDL2: framebuffer → `SDL_Texture` ARGB8888; sprite via `SDL_RenderCopyEx` | **Sì** | `gmb_gfx_present_sdl.c` (643) |
| Present alternativo Vulkan/Metal | **Abbozzo** | `include/gmb_gfx_vulkan.h`, `tests/gmb_gfx_vulkan_selfcheck.c` |
| Input tastiera/mouse/pad | **Sì** | `gmb_gfx_input.h`, `gmb_joy.c` |
| Math matriciale | **Sì** | `MATRIX`/`VECTOR`, `LINALG.MATRIX.{TRANSLATE,ROTATE,SCALE,MULTIPLY,WORLD,VIEW,PROJECTION}` |
| Dispatch TokenKind (1918 token) → parser per dominio → emitter `gmb_gfx_*` | **Sì** | `frontend/src/parser/gmb_parser_stmt_*.c`, `gmb_c99_emit.c` |

### 2.2 La suite già vendorizzata: `shared/libs/graphics` (= `gmbsuite/gmbgraphics`)

`VENDOR.md` documenta la catena: **`gmbsuite/gmbgfx-*` → copia in `shared/libs/graphics/{src,include}` → `bash build.sh` (`libgmbgraphics.dylib`) → verifica `tools/sh/gmb_lib.sh check graphics`**. Sorgenti condivisi (compute/math) **non** copiati: una sola copia nel monorepo.

Domini: `gmbgfx-color`, `gmbgfx-2d` (image/bitmap/shape), **`gmbgfx-3d`**, `gmbgfx-surface` (colormap, heatmap, mesh da heightfield), `gmbgfx-codec` (BC1). Test d'integrazione `gmbgraphics/test/run_tests.c`: **220 check eseguiti** dopo P0 (**191** prima); contando il testo, le righe con `check(` erano **198** con 191 eseguiti (1 è la definizione `static void check(`, 3 sono `else check(...)` su rami non percorsi): il numero da citare è quello che stampa il binario, non il conteggio del testo.

**`gmbgfx-3d` oggi (misurato: 1.248 righe nei 5 `src/gm_*.c` + 336 righe di header)**: `gm_gfx_geom` (AABB, `mat4_perspective/ortho/look_at`, frustum, decompose SRT, camera make/view/projection/view_projection/orbit/translate/ndc_ray_dir), `gm_gfx_intersect` (9 coppie sfera/box/piano/segmento/triangolo/cerchio), `gm_gfx_ray` (ray make/at, sphere/plane/triangle/aabb/box, **DDA su griglia** = base Wolfenstein, reflect/refract, cilindro/cono + normali, `gm_shade_phong`, volume render), `gm_gfx_mesh` (sfera UV, box, piano, cilindro: i generatori originali restano a **sole pos + indici**; **P0 ha aggiunto** `gm_mesh_attrs` con normali/UV e 4 generatori `*_uv` **senza modificarli**), `gm_gfx_raster` (target **float RGB + depth float**, `triangle` con colore per vertice, `clip_near`; **P0 ha aggiunto** `gm_tex`, `gm_raster_triangle_tex` con UV prospettici e `gm_raster_to_argb`).

**Superficie BASIC `GFX3D`** (`shared/include/gfx3d.inc`, **60 righe `DECLARE`** — misura: `grep -cE '^DECLARE'`; le 62 occorrenze della parola includono 2 righe di commento — tutte `DECLARE FUNCTION`, tutte con `LIB "gmbgraphics"` e `ALIAS "ofx_gfx_*"`): prospettiva/ortho/lookat, AABB, frustum, decompose, camera (view/projection/view-projection/orbit/translate/ndc-ray), raggi e intersezioni, Phong, volume, mesh primitive con handle (`MESHNV/MESHNT/MESHPOS/MESHTRI/MESHFREE`), **raster con handle** (`RASTERNEW/.../RASTERTRI/RASTERCLIPNEAR`), `MESHSURFACE` (terreno da heightfield), `RAYCASTDDA(EX)`, cilindro/cono, `AABBTRANSFORM`, `RAYAT`, `CAMERAORBIT/TRANSLATE/NDCRAY`. Contratto dichiarato nei test: **«Solo calcolo: nessun display, parità 4 backend»**.

Copertura esistente (misurata): `shared/minitests/graphicsffi/gfx3d_core.bas` **139 righe / 30 campi `EXPECT`**, `gfx3d_more.bas` **110 righe / 23 campi** (in totale 249 righe e 53 asserzioni), più `gfx2d_*`, `gfxcodec_bc1`, `gfx_color_shape`.

**Conclusione**: GMBasic ha già la **fondazione 3D** (math, camera, mesh primitive, ray, raster con depth, DDA, terrain, colormap) esposta a BASIC. **Non** ha: render target con **texture/UV prospettici**, scena/oggetti, renderer **a settori**, modello di **luce/fog**, animazione, IO modelli, integrazione col **framebuffer/present**, e nessun comando `SCENE`/`WORLD`. L'engine va costruito **sopra** questa base, non riscritto.

---

## 3. Architettura (rivista)

### 3.1 Decisioni

1. **Fondazione = `gm_gfx_*`** (stessa sorgente della suite): niente `gmb_3d_math.c`. Si riusano `gm_vec/mat/quat` (`shared/libs/math`), `gm_gfx_geom`, `gm_gfx_intersect`, `gm_gfx_ray`, `gm_gfx_mesh`, `gm_gfx_raster`, `gm_gfx_surface`, `gm_bc1`, e il decoder `shared/libs/media` (già vendorizzato: PNG/JPEG/BMP) per le texture.
2. **L'engine vive nella suite** (`gmbsuite/gmbgfx-3d`, nuovi sorgenti di dominio) perché è lì che vivono le librerie grafiche, è testabile stand-alone (`make -C gmbgraphics test`) ed è riusabile fuori da GMBasic. Poi si **vendorizza** in `shared/libs/graphics` (riga in `VENDOR.md`).
3. **Lo stato (scena, settori, camera, luci) vive nel runtime**, non nella dylib. Motivo misurato nel codice: le FFI caricano `libgmbgraphics.dylib` via `dlopen` con cache (`gmb_ffi_dl.c`): dylib + copia statica = **due istanze**, e una scena creata da BASIC non sarebbe quella renderizzata. Quindi: `GFX3D` resta una superficie **stateless** (solo calcolo, com'è oggi); l'engine ha un unico stato dentro `gmb_runtime_c`, che **compila gli stessi sorgenti** vendored (una sola *sorgente*, nessuna *doppia istanza*).
4. **Renderer software = riferimento** (determinismo, golden image, oracoli MiniTest); hardware opzionale con tolleranza dichiarata.
5. **Nessuna dipendenza nuova**: SDL2, zlib, cJSON, `gm_gfx_*`, `libmedia`. Niente GL linkato (eventualmente caricato a runtime come fa Engine1999).
6. **Thread**: BASIC e render **sul GPM**, present e SDL **sul main thread**, una sola via di comunicazione (frame pubblicato per puntatore + contatore atomico). Il render è **sincrono** (CPU pura, nessuna attesa); il park serve per I/O, mai per disegnare. Dettagli e invarianti in [`Nota-Tecnica-Thread-Present-3D.md`](Nota-Tecnica-Thread-Present-3D.md). **Correzione del 2026-11-19**: il GPM è **multi-worker** (`gmb_gpm_start` avvia `min(ncpu, 8)` thread e i task migrano), quindi «il thread del GPM» non esiste: la proprietà è **del task** (`gmb_cur()`), e lo stato 3D ha **un solo task proprietario alla volta** — il secondo che renderizza riceve un errore, non un'attesa (`R3`).
7. **Target ARGB** (`0x00RRGGBB`, come le pagine 2D), non indicizzato: la luce si quantizza su 32 livelli in **fixed point**, l'alpha vive nelle texture, la palette serve solo ai colori *nominati* (`palette_rgb`). Decisione e cosa si perde: specifica §11.
8. **Un solo schema JSON** (`gmb-3d` v1, `kind: "sectors"|"mesh"` obbligatorio e unico, un documento per file, estensione `.gmbscene`), non due dialetti: un loader, un reporter di errori, una migrazione, un round-trip — quindi un solo `F-*` per tipo invece di due copie che divergono. Il `kind` sbagliato è un errore immediato (`WORLD` rifiuta `mesh`, `SCENE` rifiuta `sectors`). Motivi, costo e cosa la ribalterebbe: specifica §12.1.
9. **Nessun comando `WORLD CHECK`**: le invarianti `W-INV-*` sono **fatali nel load** (elemento nominato, `ON ERROR`) e il controllo in blocco è il tool `tools/c/world_check.c` in `tools_verify.sh`. Motivi: non pagare ~8 artefatti di superficie (§3.4) per un'operazione di sviluppo, e non avere due punti d'ingresso alla stessa funzione. Dettagli: specifica §12.2.
10. **`SEES(n)` = `1` vede, `0` non vede, `−1` thing inesistente**, non la sentinella invertita del riferimento: `−1` = *assente* in tutta la superficie (`TON`, `SECTORAT`, `ZONE`, `USE`), e l'inversione è la trappola n.1 per chi scrive un gioco. Divergenza dichiarata (`C-DIVERGE-SEES`), migrazione in guida (P9). Motivi: specifica §12.3.
11. **Tutto il motore è compatibile GPM** (deciso il 2026-11-19): comandi e `RENDER` solo dentro un task (`gmb_gpm_in_task()` = `gmb_cur()!=NULL`), I/O che **parca** il task (`gmb_c99gpm_extjob_*`) con ripiego sincrono fuori da un task, ciclo di gioco **park-based** come `gmb_form_frame` (niente `gmb_sleep_ms`, niente spin), eventi dispatchati dal GPM (mai sul thread SDL), cap del present **senza backpressure**, stato 3D letto da T0 solo tramite atomi pubblicati dal task, tool nativi (`world_check.c`, oracolo P1) che non avviano lo scheduler. Motivo: è AGENTS §0ter applicato al 3D; l'alternativa — un worker che aspetta, o un handler invocato su T0/T2 — è la classe di bug che il GPM esiste per impedire (precedente: `faa0ae27`). Obbligazione per decisione e invarianti: nota tecnica §3 (`R1`–`R11`) e **§14**, riga `C-GPM` della specifica.

### 3.2 Strati

```
        BASIC:  SCENE ... / WORLD ... / SCENE.FUNC() / WORLD.FUNC()
                              │
        parser C + C# (TokenKind + sotto-verbo) ── emitter C99 / VM ──► gmb_3d_*()
                              │
   ┌──────────────────────────▼───────────────────────────────────────────┐
   │ native/gmb_runtime_c (stato unico, presenti, frame, errori)          │
   │  gmb_3d_api.c     facade, handle, opzioni (settings, no env)         │
   │  gmb_3d_frame.c   VIEW/clip, target (pagina, canvas), present        │
   │  gmb_3d_scene.c   oggetti, thing, lampade, zone, culling, LOD        │
   │  gmb_3d_mesh3.c   geometria con attributi (nrm/uv), materiali        │
   │  gmb_3d_rt.c      render target: ARGB 0xRRGGBB, z, UV prosp.         │
   │  gmb_3d_sector.c  settori/portali, moving sectors, sky               │
   │  gmb_3d_collide.c move/shoot/use/sees                                │
   │  gmb_3d_light.c   luce 32 livelli, lampade, fog, traslucenza         │
   │  gmb_3d_anim.c    keyframe TRS, skinning (64 ossa)                   │
   │  gmb_3d_io.c      JSON world/scene (cJSON), OBJ/GLB subset           │
   │  gmb_3d_audio.c   audio posizionale (attenuazione + pan)             │
   └───────▲──────────────────────────────▲───────────────────────────────┘
           │ compilato dagli stessi sorgenti (statico)
   ┌───────┴──────────────────────────────┴───────────────────────────────┐
   │ shared/libs/graphics  (vendored da gmbsuite/gmbgraphics)             │
   │  gm_gfx_geom/intersect/ray/mesh/raster/surface/color/codec           │
   │  + NUOVI (in gmbsuite/gmbgfx-3d, poi vendored):                      │
   │    gm_gfx_rt.c     target ARGB, UV prospettici, mip, wrap            │
   │    gm_gfx_scene.c  gerarchia, ordine, culling per oggetto            │
   │    gm_gfx_sector.c portali (ray per colonna) riusando DDA            │
   │    gm_gfx_light.c  colormap, lampade, fog, traslucenza               │
   │    gm_gfx_anim.c   keyframe/skinning                                 │
   └──────────────────────────────────────────────────────────────────────┘
                              │
        present: SDL2 (framebuffer → texture, invariato) · SDL_RenderGeometry (opz.) · Vulkan/Metal (opz.)
```

### 3.3 Due mondi, due comandi

| Namespace | Modello | Parità |
|---|---|---|
| `WORLD` | settori + pareti/portali + thing | 1:1 `W3D` |
| `SCENE` | mesh + billboard + lampade + zone | 1:1 `M3D` |

Condivisi: math (suite), camera/player, color map, fog, texture, present, audio, IO. Il raycast per colonna di `WORLD` **non** è il raster di `SCENE`: stessa camera, pipeline diversa.

### 3.4 Aggiunta di un comando: pipeline obbligatoria

1. `TokenKind.cs` (+ `Lexer.Keywords.cs` se riservata) — 2 soli gruppi nuovi (`Scene`, `World`), sotto-verbi su token esistenti dove possibile.
2. `tools/basic/gen_c_tokenkind.bas` → `gmb_tokenkind.h/.c` (`--check` in gate).
3. `frontend/src/gmb_lexer.c` se riservata.
4. parser: `gmb_parser_stmt_scene.c` / `gmb_parser_stmt_world.c` (pattern `RegisterCommand(TokenKind.Screen, ...)` di `StdLib.Graphics.cs`).
5. emitter: `gmb_c99_emit_3d.c` (solo chiamate `gmb_3d_*`).
6. StdLib C# (`RegisterCommand`/`RegisterFunction`) per IL/PCode via **P/Invoke** alle stesse funzioni C.
7. `gmb_audit` (superficie canonica 982 → N) + `tools/README.md`.
8. MiniTest + sample + guida (§6).

---

## 4. API: parità 1:1 e oltre

### 4.1 `WORLD` (da `W3D`)

`WORLD RESET` · `TEXTURE n, a$` · `SECTOR s, floor, ceil [, tf [, tc [, light]]]` · `WALL s, x1, y1, x2, y2, tex [, next]` · `WALLTEX s, i, mid, upper, lower` · `WALLOFF s, i, xo, yo` · `WALLFLAG s, i, flags` (1 masked, 2 no-pass, 4 no-shot/sight, 8 sky, 16 translucent) · `HEIGHT s, floor, ceil` · `LIGHT s, l` · `TAG s, v` · `SKY tex [, repeat [, shift]]` · `CAMERA x, y, angle [, pitch [, lift]]` · `TURN dangle [, dpitch]` · `PLAYER radius, height, eye, step` · `VIEW x, y, w, h [, scale [, fov]]` · `FOG d` · `FAR d` · `COLORMAP` · `RENDER` · `MOVE f, r` · `LIFT h` · `THING n, x, y, tex, w, h` (+ `THINGZ/TEX/OFF/MOVE/LIGHT/TRANS`) · `SLOPE s, kind, gx, gy [, ox, oy]` · `TICK s` · `SECMOVE s, speed, floor [, ceil]` · `ANIM n, first, count, fps` · `SCROLL s, kind, dx, dy` · `LIGHTFX s, mode [, amount [, rate]]` · `LAMP n, x, y, z, radius, amount` · `GHOST s, flag` · `CLEAR` · `WORLD` (da JSON).

Funzioni: `WORLD.X/Y/Z/ANGLE/PITCH/SECTOR/SECTORAT(x,y)/FLOORAT/CEILAT/FLOOR/CEIL/LIGHT/TAG/WALLS/SECTORS/TX/TY/TSECTOR/TON/DIST/BEARING/HIT/SHOOT(range)/SHOTKIND/SHOTDIST/SHOTX/SHOTY/USE(reach)/SEES(n)/TEXW/TEXH/TZ/FLOORZ/VISITED/SLOPE/SECTORZ(x,y,z)/MOVING/TIME/LIGHTNOW/WALLINFO/SECINFO/THINGINFO`.

### 4.2 `SCENE` (da `M3D`)

`RESET` · `TEXTURE` · `CLEAR` · `VIEW x,y,w,h [, scale [, fov]]` · `FOG` · `FAR` · `SKY tex [, repeat [, shift]]` · `COLORMAP` · `RENDER` · `OBJECT o [, flags [, light]]` · `VERTEX o, x, y, z [, u [, v]]` · `FACE o, a, b, c, tex` · `QUAD o, a, b, c, d, tex` · `BOX o, x1, y1, z1, x2, y2, z2, tex` · `OBJPOS o, x, y, z [, yaw]` · `OBJSHOW/OBJLIGHT/OBJFLAGS` · `THING n, x, y, z, tex, w, h` (+ `POS/TEX/OFF/LIGHT/TRANS/MOVE`) · `CAMERA x, y, z, yaw [, pitch]` · `TURN` · `PLAYER` · `MOVE` · `LIFT` · `LAMP n, x, y, z, radius, amount [, dx, dy, dz, angle]` · `ZONE id, x1, y1, x2, y2, z1, z2 [, tag]` · `WORLD` · `SOFTWARE` / `HARDWARE`.

Funzioni: `SCENE.X/Y/Z/EYE/YAW/PITCH/LIFT/HIT/FLOORZ/FLOORAT/CEILAT/TON/TX/TY/TZ/DIST/BEARING/SHOOT/SHOTKIND/SHOTOBJ/SHOTDIST/SHOTX/SHOTY/SHOTZ/USE/SEES/TEXW/TEXH/OBJON/OBJX/OBJY/OBJZ/OBJYAW/VERTS/TRIS/DRAWN/ZONE/ZONETAG/RENDERER/HWAVAIL`.

> **Agganciate a P3 (2026-11-24) e P4 (2026-11-25)**: dalla superficie di §4.2 sono reali
> `RESET`, `VIEW w, h, fov`, `TEXTURE celle | "file.png"`, `OBJECT r, g, b [, flags [, luce]]`,
> `VERTEX x, y, z, u, v`, `QUAD a, b, c, d`, `BOX x0, y0, z0, x1, y1, z1`, `OBJPOS x, y, z, yaw`,
> `CAMERA x, y, z, yaw, pitch`, `LIGHT dx, dy, dz, livello`, `SMOOTH|ALPHA|TWOSIDED on|off`,
> `LAMP n, x, y, z, raggio, quantità [, dx, dy, dz, angolo]`, `FOG d`, `FAR d`,
> `SKY repeat, shift`, `COLORMAP`, `THING n, x, y, z, w, h`, `RENDER`, `PRESENT`, `SAVE path$`;
> letture `SCENE.VERTS/TRIS/DRAWN/CULLED/PIXELS/HASH/PIXEL(x, y)`. Le forme con l'id oggetto
> esplicito (`VERTEX o, …`, `OBJECT o, …`, `BOX o, …`), `FACE`, `CLEAR`, `TURN`, `PLAYER`,
> `MOVE`, `LIFT`, `ZONE`, `WORLD`, `SOFTWARE`/`HARDWARE` e le letture di camera/oggetti non ci
> sono ancora. `SKY` richiede **entrambi** gli argomenti e usa la texture corrente; `THING` usa la
> texture corrente. Divergenze e stato completo: §6 P4. La superficie è documentata in
> `Guide/utente/SCENE-Guida-Utente.md`.

### 4.3 In più (oltre Engine1999)

| Funzionalità | API | Perché |
|---|---|---|
| Skinning (64 ossa) | `SCENE BONES`, `WEIGHT o,v,bone,w`, `POSEBONE`, `ANIM skeleton` | assente là |
| Keyframe TRS | `SCENE KEY o, t, x,y,z,yaw,pitch,roll` | animazioni dichiarative |
| LOD + culling spaziale | `SCENE LOD o, dist, mesh` (culling interno) | Engine1999: "no spatial culling" |
| Luci colorate + tonemap | `SCENE LIGHT color`, opzione `tone` | là solo additive neutre |
| Ombre (shadow map low-res, PCF-lite) | `SCENE SHADOWS 0..2` | assente |
| Tilt vero | `WORLD CAMERA … roll` | là shear ~30° |
| Buchi tra piani | `WORLD PORTALHOLE` | là non fatto |
| Terrain con LOD | `SCENE TERRAIN` (su `gm_mesh_surface`) | riuso `gmbgfx-surface` |
| Particelle | `SCENE PARTICLE` | — |
| Audio 3D | `SCENE SOUND n, x,y,z, …` | — |
| Post-FX | `SCENE FX` (vignette, FXAA-lite) | — |
| IO modelli | `SCENE LOAD obj|glb` | là solo a mano/JSON |
| Debug | `SCENE DEBUG 0..3` (wireframe, frustum, normali) | — |
| Statistiche | `SCENE.STATS("tris"|"draw"|"ms"|"bytes")` | misure, non impressioni |

---

## 5. Parità 2D residua (Engine1999)

| Gap | Intervento | Fase |
|---|---|---|
| Layer multipli con palette propria + priorità | `gmb_layers.c` sopra `gmb_graphics.c` | P7 |
| Tile/sprite a griglia con sotto-scroll 0..15 px | estensione `tilemap` | P7 |
| Gruppi di sprite (w×h come un oggetto) | `SPRITE GROUP n, w, h` | P7 |
| Path di sprite (`X+n/M/W/A/E/[ ]n/R`) | interprete path nel runtime | P7 |
| `MOVETO ticks` | motore per-frame | P7 |
| Collisioni per pixel opaco | riuso del test pixel dei canvas | P7 |
| Text screen 80×25 a celle con colori | `gmb_gfx_render_cells*` + modalità cella | P7 |
| `DESIGN`/`@` | parser + `gmb_gfx_sprite_def` | P7 |
| `GFX DRAW` (GW-BASIC) | interprete in runtime | P7 |
| `PRINT USING` / `FORMAT$` | formattatore | P7 |
| `INPUT` a campi, `PEEK/POKE` | verifica esistenza, altrimenti nuovi | P7 |
| Synth 8 canali + MML | `gmb_synth.c` sopra SDL audio | P6 |
| UI dichiarativa | sopra i controlli TUI | P7 |
| `JSON` namespace | wrapper cJSON con path | P7 |
| `SCREENSHOT` composto | composizione dei layer | P7 |

---

## 6. Fasi (con accettazione)

> Stime in **giorni-uomo** focalizzati, AI-assistito. Riuso della suite incluso: **~80–120 gg** (v1: 85–130 senza riuso).
>
> **La numerazione è l'ordine di esecuzione.** `P0 … P9` si eseguono in sequenza: P0, poi P1, poi P2, e così via. Non ci sono inversioni da ricostruire: se un blocco viene prima, ha un numero più basso. L'hardware è **`PH`**, fuori numerazione e in fondo, perché è l'ultima cosa e può anche non esserci.
>
> Per chi ha appunti con la numerazione della revisione 2: `vecchio P0.5 → P1`, `vecchio P3 → P2`, `vecchio P1 → P3`, `vecchio P2 → P4`, `vecchio P4 → P5`, `vecchio P5 → PH`; `P6`–`P9` sono invariati.
>
> **Perché quest'ordine**: (a) i settori riusano una primitiva già verificata (la DDA dello spike) e producono la demo che somiglia al riferimento senza z-buffer né UV prospettici; (b) senza l'oracolo di P1 nessuna fase successiva è verificabile; (c) l'hardware non compra nulla a 320×150 e richiede la stessa astrazione di target, quindi resta fuori dalla sequenza.
>
> **MVP giocabile** (P0+P1+P2): **~16–24 gg**.

### P0 — Fondamenta, linkage e spike ABI — **FATTO** (misurato)

**Spike di linkage** (`tools/c/gmb_3d_link_spike.c` + `tools/sh/check_3d_link.sh`): 19 controlli, tutti PASS, con **`cc` e `tools/sh/zcc`**. Misure:

* gli **11 file di dominio** `shared/libs/graphics/src/gm_*.c` + i 5 sorgenti di `shared-src.txt` linkano **statici con la sola `-lm`**: nessuna dipendenza nascosta;
* i valori coincidono con gli oracoli di `gfx3d_core.bas` (perspective 2/3, look_at −5, box 8/12, raster 8×4 a (1,1) = 0.285714/0.214285/0.5, DDA 2,0,0,1.5) → la copia statica **è** la stessa implementazione;
* la dylib esporta **331 simboli `gm_*`** e `dlsym` restituisce un indirizzo diverso: **due istanze confermate**. Il test disegna sulla raster statica e verifica che quella della dylib **non veda nulla** → stato separato. **Confermata la decisione §3.1.3**: l'engine tiene lo stato nel runtime, `GFX3D` resta stateless;
* nessuna variabile statica mutabile nei domini: il renderer può essere puro.

Nota A/B: con i soli file usati, `zig cc` **elimina le sezioni morte** e mascherava un simbolo mancante (`gm_bitmap_to_image` → `gm_image_alloc`). Il check compila quindi **tutto** lo strato `gm_*.c`, così il gate A/B non passa per ottimizzazione.

**Estensioni in `gmbsuite/gmbgfx-3d`** (additive, nessuna funzione esistente toccata):

* `gm_gfx_mesh`: `gm_mesh_attrs { nrm, uv }` **separato** + `gm_mesh_attrs_none/free/valid`, `gm_mesh_normals` (lisce, pesate per area), `gm_mesh_box_uv` (24 vertici, normali piatte), `gm_mesh_plane_uv`, `gm_mesh_sphere_uv`, `gm_mesh_cylinder_uv`. **Perché separato e non campi in coda**: `gm_mesh` è costruita anche da `gm_mesh_surface` (dominio `gmbgfx-surface`, fuori perimetro) che inizializza solo 4 campi; allungare la struct avrebbe fatto liberare puntatori spazzatura a `gm_mesh_free`. Il blocco a parte è additivo al 100% e i generatori esistenti restano bit-identici;
* `gm_gfx_raster`: `gm_tex` (RGBA8, riga 0 in alto), `gm_tex_alloc/free/set/sample` (wrap repeat, nearest/bilineare), `gm_raster_triangle_tex` (**UV prospettici** via `1/w` per vertice, alpha-test `masked`, z-buffer), `gm_raster_to_argb` (**0x00RRGGBB**, il formato del framebuffer GMBasic).

**Verifica eseguita**: suite `make -C gmbgraphics test` → **220 check, 0 FAIL** (191 prima; il README della suite dichiarava 191 e sul disco le presenze testuali di `check(` erano 198 — ho allineato il README a 220, che è il numero stampato dal binario); dylib ricostruita (`build.sh`) e `gmb_lib.sh check graphics` ok; MiniTest `graphicsffi/gfx3d_core.bas` **PASS** e `gfx3d_more.bas` **PASS**; `check_file_size.sh --quiet` ok; `tools_verify.sh` ok (`readme:conteggi` allineato).

**Rinviato a P3/P4** (dichiarato, non dimenticato): mip-lite, alpha blending traslucido (qui solo alpha-test), luce a **32 livelli** in fixed point (qui RGB float + export ARGB), verifica del **winding** delle facce del box (struttura sì, orientamento visivo no).

**Restano da P0**: opzioni via settings (`engine.3d.*`, nessuna env) e il render headless `SCENE` → canvas/PNG, che presuppongono P1 (oracolo) e P3 (target e present).

### P1 — Oracolo headless — **FATTO** (2026-11-19)

Senza oracolo, P2–P4 non si possono verificare: si fa prima di scrivere un renderer.

* `RENDER` in **null mode** (precedente: l'host console del riferimento esegue il mondo e non disegna): lo stato avanza, il disegno non avviene.
* Cattura del fotogramma **dal target 3D** (o da `gmb_gfx_visible_pixels()` per il 2D) → PNG → confronto. **Non** `gmb_gfx_present_capture`: richiede video reale e main thread e in headless restituisce `-1` (nota tecnica §7).
* **Verificato prima di scrivere: la cattura dal target non esisteva** (`grep` di `gmb_3d*` = 0 file, nessuna API 3D nel runtime). Aggiunta qui, come chiesto: `include/gmb_3d.h` + `src/gmb_3d_frame.c` — `gmb_3d_target_alloc/clear/blit_argb/capture_argb/capture_png`, `gmb_3d_present_mode_*` (null mode), guardie `gmb_3d_render_begin/end` (`R3`/`R5`, senza attese) e `gmb_3d_alloc_count()` per la regola «nessuna allocazione per frame». Il modulo non include header SDL e non dipende da `gm_*`: il raster della suite si compila solo nell'oracolo.
* **Golden per RID** (`osx-arm64`, `osx-x64`, `linux-x64`, `linux-arm64`): `cc` e `zcc` possono divergere nei float; una golden non riproducibile fuori dal RID che l'ha generata si dichiara e resta riferimento visivo, non gate.
* **L'oracolo è T3 per la cattura**: `--check`/`--bless` girano fuori da un task (processo di test o tool C) e non leggono stato di proprietà. `--selftest` invece avvia **davvero** `gmb_gpm_start` per la sola parte `R3`/`R5`: due task veri sono l'unico modo di dimostrare che la chiusura da non-proprietario non blocca il renderer. Lo stesso codice di cattura resta usabile **dentro** un task, dove però il salvataggio su file **parca** (`R11`).
* Prima scena d'oro: triangolo texturizzato con `gm_raster_triangle_tex` (già pronto da P0) → pipeline completa di verifica senza aspettare il motore.
* **Accettazione**: 3 golden generate e confrontate; un fotogramma con 1 pixel alterato viene respinto; `RENDER` in null mode non alloca per frame.

**Risultato misurato** (`tools/sh/check_3d_oracle.sh`, build in cartella fuori dal repo): gate A/B sul file C con `cc` **e** `tools/sh/zcc`; `--selftest` → **37 check, 0 FAIL** (ricorsione rifiutata `R5`; **due task GPM veri**: il secondo riceve `E_BUSY` senza attesa e il suo `RENDER END` non tocca il claim del proprietario vivo, mentre con il proprietario **finito senza `END`** lo stesso `END` libera il claim orfano e il renderer torna apribile; null mode che non scrive e non alloca su 100 frame, cattura identica bit per bit al target, `alloc_count` invariato, errori espliciti sul blit di dimensioni diverse). Mutazione di prova: togliendo il recupero dell'orfano il selftest diventa **36 ok / 1 FAIL, exit 1**. I riferimenti a GPM sono **forti**: un link senza scheduler fallisce su `_gmb_c99gpm_active`/`_gmb_cur`/`_gmb_task_finished`, quindi la guardia non può tornare inerte in silenzio; `--bless` → 3 golden in `shared/minitests/3d/golden/` (`tri_tex`, `tri_mask`, `tri_depth`, **19200 px** ciascuna); `--check` → **diff=0** su tutte e tre; `--perturb 1` (un pixel alterato) → **3 DIFF, exit 1**. Le golden sono per RID: qui esiste `osx-arm64`; `osx-x64`, `linux-x64`, `linux-arm64` si generano con `--bless` sul RID corrispondente (nessun artefatto locale su questa macchina: §0 del protocollo).

### P2 — Settori/portali, tempo, mappa demo (10–15 gg) — **FATTO** (2026-11-19)

**Prerequisito**: [`Nota-Tecnica-Thread-Present-3D.md`](Nota-Tecnica-Thread-Present-3D.md) — il primo renderer introduce il frame pubblicato, la guardia di ricorsione e il resize differito (`R3`–`R5`, `R7`).

**Decisioni ereditate, già chiuse** (non sono più scelte da prendere dentro P2): schema JSON unico con `kind` (§3.1.8, specifica §12.1); invarianti fatali nel load e `WORLD CHECK` solo come tool (§3.1.9, specifica §12.2); `SEES` 1/0/−1 (§3.1.10, specifica §12.3); cap del present unico con `auto` e triple buffer solo dietro misura (nota §8, §13); identità **del task** invece del thread, quindi `R3` = proprietà e rifiuto, non lock (nota §3, §13). **Tutte con l'obbligazione GPM esplicita** (§3.1.11, nota §14, riga `C-GPM`): il `LOAD` parca, il ciclo parka, T0 legge solo atomi, nessun worker attende.

* `gm_gfx_sector` / `gmb_3d_sector`: ray per colonna, portali ricorsivi (≤64), pareti per colonna, pavimenti/soffitti per pixel, depth float, shear pitch, sky, `FAR`, `SLOPE`, `GHOST`; riuso di `gm_raycast_dda` per i test di punto/cella dove serve.
* `gmb_3d_collide`: player, `MOVE` (sliding/gradini/portali), `LIFT`, `SHOOT`, `USE`, `SEES`, `HIT`.
* Tempo: `TICK`, `SECMOVE`, `ANIM`, `SCROLL`, `LIGHTFX`, `LAMP`.
* Invarianti di authoring (`W-INV-*`) come validatore: è il momento in cui servono, prima che le mappe si moltiplichino.
* **Accettazione**: mappa demo (3 stanze + porta + ascensore + finestra traslucida) a ≤4 ms/frame a 640×300; 20 MiniTest; 6 golden; 10 casi di collisione headless.

**Stato al 2026-11-19 (chiusa).**

| Pezzo | Dove | Cosa fa |
|---|---|---|
| renderer | `shared/libs/graphics/{include,src}/gm_gfx_sector.*` (101 + 372 righe) | un raggio 2D per colonna, portali ricorsivi (≤64), piani per pixel ancorati alle x,y del mondo, pareti 1 unità = 1 texel, shear del pitch, cielo per angolo, fog lineare; luce di settore × lampade (fino a 8, falloff lineare), scroll di piani e pareti, parete traslucida (flag 16) = ricorsione nel settore dietro + media per canale; scrive `unsigned int*` `0x00RRGGBB` + `float*` depth del target P1 |
| load | `src/gmb_3d_io.c` (362) | schema `gmb-3d` v1 `kind:"sectors"`, invarianti fatali e nominate (`W-INV-FLOOR`, `W-INV-HEIGHT`, `W-INV-CCW`), `R11`: file su thread esterno + task parcheggiato (`gmb_3d_load_park_count`), load atomico |
| binding | `src/gmb_3d_sector.c` (42) + `include/gmb_3d.h` | `gmb_3d_world_load/free/render`, `gmb_3d_camera_t`; in null mode non disegna |
| movimento | `src/gmb_3d_move.c` (332) + `include/gmb_3d.h` | `gmb_3d_player_t` (`W-PLAYER`), `MOVE` relativo alla vista: scivolamento, gradini ≤ `step`, portali alti `height` e larghi `2×raggio`, `HIT` a bit (1/2/4), sotto-passi ≤ raggio/2; `place`/`view` con `W-EYE-CLAMP`; le pareti con flag **2** fermano il movimento (`W-FLAGS`) |
| tempo | `src/gmb_3d_time.c` (270) + `include/gmb_3d.h` | `TICK` a passo fisso **1/60** con accumulatore e catch-up ≤ 10 s, `TIME`/`TICKS`; `SECMOVE`/`SECMOVE_CEIL` (mover di pavimento, o rigido col soffitto) con `HEIGHT` che ferma e `MOVING` `−1` in moto; `ANIM` (picture del settore), `SCROLL` (0 piani / 1 pareti), `LIGHTFX` 1 flicker (xorshift32 a seed fisso per settore) / 2 pulse / 3 strobe, `LIGHTNOW`, `LAMP` a runtime; `rt` per settore allocato dal loader e liberato con il mondo |
| mappa demo | `shared/minitests/3d/worlds/demo.gmbscene` | 5 settori (3 stanze **A/B/C**, porta **D**, ascensore **E**), 30 pareti, 8 texture, 1 lampada; porta `1↔0` e `1↔2`, ascensore `3↔2`, finestra traslucida `A↔C` (flag **18** = 2+16, apertura `24…48`); camera `(20,30,46)` angolo `0.6`, fov 90° |
| oracolo | `tools/gmb_3d_oracle{,_world}.c` (534 + 667), `tools/sh/check_3d_oracle.sh` | 7 scene (3 P1 + `sector_room`/`sector_door`/`sector_pitch` + `demo`), 7 golden per RID, selftest **95** check: `E_BUSY` a due task (P1), `R11` dentro e fuori da un task, **10 casi `W-MOVE`** (muro frontale/obliquo/angolo, gradini 24/40, portale basso/stretto/alto, `W-EYE-CLAMP`) più la vista del giocatore dopo il `MOVE`, e **16 `T-*`** (60 passi al secondo, tempo 1.000 s, catch-up ≤ 601, porta a metà in 0.5 s con `MOVING` `−1`→`0`, `HEIGHT` che ferma, ascensore che sale con il soffitto, ANIM a 4 fps, SCROLL piani/pareti, strobe, flicker ripetibile, lampada che schiarisce, demo caricata e fotogramma coperto); `--bench` misura la demo |

**Risultato misurato** (`bash tools/sh/check_3d_oracle.sh`): gate A/B sul file C con `cc` **e** `tools/sh/zcc`; `P1-SELFTEST ok=95 fail=0`; 7 golden `diff=0` (`tri_tex`, `tri_mask`, `tri_depth`, `sector_room`, `sector_door`, `sector_pitch`, `demo`, 19200 px ciascuna per `osx-arm64`); `--bench` sulla mappa demo a **640×300**, 120 frame con 5 di warm-up (tick + render, dinamica vera): **min 0.63 ms, mediana 0.66 ms** contro il limite di accettazione **4 ms**; `gmb_runtime_min` ricompila con `gmb_3d_time.c`. I file toccati restano sotto le 1200 righe (max 667).

Difetti trovati e corretti dall'oracolo: il pavimento non veniva disegnato quando l'occhio è sopra di esso (profondità negativa nel piano), visibile come fondo nero in `sector_door`/`sector_pitch`; la fascia solida sopra il portale mancava (condizione su `y` invertita, il raggio si allargava al soffitto dietro) e il pavimento non veniva disegnato una volta sola; la collisione applica lo spostamento prima della spinta e la spinta riporta il centro dal lato in cui si trova già (il sotto-passo senza spostamento lasciava il corpo fermo, la spinta orientata all'esterno lo espelleva). **P2 chiuso**: settori, portali, collisione, tempo e mappa demo sono verificati dallo stesso gate che resterà la rete di P3.

**Aggancio al linguaggio (2026-11-19).** I comandi del tempo sono raggiungibili dal BASIC su **tutte e quattro le gambe**, con TokenKind canonici e senza un solo dispatch per lessema. `WORLD` **non è** un namespace implicito: senza `IMPORT WORLD` il comando è un errore dichiarato su ogni gamba.

```basic
IMPORT WORLD                        ' obbligatoria: WORLD non è fra i namespace sempre disponibili
WORLD LOAD "worlds/demo.gmbscene"   ' un mondo corrente per processo (come DBF/ISAM: il BASIC non maneggia handle)
WORLD TICK 0.5                      ' orologio a passo fisso 1/60, accumulatore, catch-up ≤ 10 s
WORLD SECMOVE 1, 56, 28             ' pavimento a 28 a 56 u/s; con il quarto argomento anche il soffitto
PRINT WORLD.FLOORZ(1), WORLD.MOVING(1)
WORLD HEIGHT 1                      ' ferma il mover (le letture FLOORZ/CEILZ NON lo fermano)
WORLD ANIM 0, 6, 2, 4               ' picture del settore a 4 fps
WORLD SCROLL 0, 0, 8, 0             ' 0 = piani, 1 = pareti
WORLD LIGHTFX 0, 3, 0.5, 1          ' 1 flicker / 2 pulse / 3 strobe, 0 spegne; si legge in WORLD.LIGHTNOW
WORLD LAMP 40, 40, 50, 90, 0.6
WORLD CLOSE
```

| Pezzo | Dove |
|---|---|
| TokenKind | `common/Ast/TokenKind.cs` + `frontend/include/gmb_tokenkind.h` in coda (`SecMove`, `Moving`, `Anim`, `Lamp`, `LightFx`, `LightNow`, `FloorZ`, `CeilZ`; `World`/`Scroll`/`Height`/`Time` esistevano già), `GMB_TOK_COUNT` 2677 → 2685 |
| lessico | `gmb_lexer.c`: keyword `SECMOVE/MOVING/ANIM/LAMP/LIGHTFX/LIGHTNOW/FLOORZ/CEILZ` |
| comandi (statement) | `gmb_parser_stmt_domain.c` (`GMB_TOK_World` in `g_domain_ns`) + `native/gmb_c99native/src/gmb_c99_emit_3d.c` (`emit_world_domain`) |
| letture (funzioni) | stesso emitter (`emit_bf_world`, tabella builtin); `WORLD.MOVING/LIGHTNOW/TIME/FLOORZ/CEILZ` si risolvono con le tabelle **generate** `gmb_ns_lookup.h`/`gmb_fn_lookup.h` (nessuna tabella dichiarata) e richiedono `IMPORT WORLD` |
| ponte C# — superficie | `common/StdLib/StdLib.World.cs` (`RegisterCommand(TokenKind.World, …)` + 5 `RegisterFunction`), `common/Ast/World3D.cs` (`WorldCommandStmt`), `common/Parser/Parser.Stmt.World.cs`, case in `Linker.Binding/Resolver/Rewrites` |
| ponte C# — P/Invoke | `common/CommonLib/Numerics/World3DNative.cs` → le STESSE `gmb_world_*` di `gmb_3d_cmd.c`, via `libgmb_3d` (target CMake `gmb_3d`, che compila `gmb_3d_cmd.c` + il raster della suite) |
| ponte C# — backend | `Compiler/Compiler.World.cs` (10 `SysCall` `WORLD_*` + `WORLD_SECMOVE_CEIL` separata), `IlProgramCompiler.World.cs`/`IlRuntime.World.cs`, `PCodeProcessor.Syscalls.cs`/`PCodeProcessor.Builtins.World.cs` |
| ponte C — VM/C | `gmb_vm_core.c` (10 handler syscall), `gmb_builtin.c` (5 builtin 3903–3907), `gmb_syscall_codes.h` rigenerato (`GMB_SC_WORLD_*`) |
| facciata C | `native/gmb_runtime_c/src/gmb_3d_cmd.c` (`gmb_world_*`): solo chiamate alle `gmb_3d_*`, nessuna logica duplicata (§0bis) |
| MiniTest | `shared/minitests/3d/*.bas`, **20 file**, tutti con `IMPORT WORLD` e oracolo `' EXPECT:`, **verdi su tutte e quattro le gambe** (nessun marcatore) |
| Guida utente | vault, `Guide/utente/WORLD-Guida-Utente.md` — i 9 comandi e le 5 letture: sintassi, parametri, semantica, gate `IMPORT WORLD`, disponibilità per backend, errori e diagnosi |
| Manuale (libro) | `docs/manual/capitoli/IV.21b–e` — il volume di studio come capitoli della serie: il mondo a settori e il tempo simulato, mover e superfici, luce e programma completo, diagnosi con 20 esercizi e glossario; **non versionato** (`docs/manual/` è in `.gitignore`) |
| Sample completo | `shared/samples/3d/world_demo.bas` (+ `worlds/demo.gmbscene`) — usa **tutti** i comandi e **tutte** le letture su un mondo reale; output atteso di 13 righe, identico sulle quattro gambe |

`WORLD.FLOORZ`/`CEILZ` sono letture pure (`gmb_3d_sector_z`), separate da `HEIGHT` che invece ferma il mover: un HUD che mostra la quota di un ascensore non lo blocca. I comandi non hanno valore di ritorno leggibile dal BASIC (§1): gli errori si verificano **dallo stato** (un `SECMOVE` con soffitto sotto il pavimento non avvia nessun mover), e questo è quello che fanno i MiniTest.

**Ponte chiuso: quattro gambe.** Lo stato 3D resta **solo** nel runtime C; le gambe gestite non ne tengono una copia e inoltrano alla stessa facciata `gmb_world_*` tramite `libgmb_3d` (P/Invoke per IL e PCode, chiamata diretta per la VM/C, che linka la stessa `gmb_3d_cmd.c`). Duplicare caricatore o mover avrebbe significato due mondi divergenti (§0bis).

Conseguenze verificate il 2026-11-19: i 20 MiniTest girano su **c99n, IL, PCode e vmc** (20/20 per gamba, `tools/sh/parity4`); il marcatore `' TAG: native-only` è **ritirato** da test e runner (`parity4`, `MiniTestsParityTests`, che non ha più il filtro); le due tabelle dichiarate — `Moving/LightNow/FloorZ/CeilZ` in `frontend/include/gmb_members.h` e `gmb_p_native_ns_lookup` in `gmb_parser_expressions.c` — sono **rimosse**: le letture qualificate si risolvono con `gmb_ns_lookup.h` generato (`WORLD.TIME` → `GMB_TOK_WorldTime`, un TokenKind dedicato perché `TIME` era già la keyword omonima).

Due dettagli che il ponte ha reso necessari, e che vanno ricordati se si tocca il dominio:

* **`IMPORT WORLD` è obbligatoria — per i comandi E per le letture.** `WORLD` non sta fra i namespace disponibili senza import (solo `SYSTEM`/`CONSOLE`/`CONSTANTS`/`TRAIN`/`GMB` lo sono): la voce in `frontend/include/gmb_ns.h` serve soltanto a far riconoscere `IMPORT WORLD` come namespace **builtin**, cioè a non cercarlo su disco come modulo. Il gate è esplicito in **tutte e quattro le gambe**, sui due punti di risoluzione:
  * **comandi** (`WORLD LOAD/CLOSE/TICK/SECMOVE/HEIGHT/ANIM/SCROLL/LIGHTFX/LAMP`): `ParseStatement` lato C# (`_program.Namespaces`, nessun codice dedicato a WORLD) e il dispatch dei domini in `gmb_parser_statements.c`;
  * **letture qualificate** (`WORLD.TIME()`, `WORLD.FLOORZ()`, `WORLD.CEILZ()`, `WORLD.MOVING()`, `WORLD.LIGHTNOW()`): il caso `QualifiedNameExpr` in `Parser.Expressions.cs` e il punto di risoluzione delle call qualificate in `gmb_parser_expressions.c`. Senza, bastava nominare il namespace per leggere il mondo e `WORLD` sarebbe rimasto implicito di fatto (le altre gambe risolvono i namespace non importati dal registro canonico, es. `TRADING.PORTFOLIO.*`: il gate stringe **solo** su WORLD).

  Lato C il gate è **una funzione sola**, `gmb_p_require_import` in `gmb_parser_ns_guard.c`, chiamata dai due punti di risoluzione: unica sede del messaggio e unica definizione di "caricato" (la stessa del gate dei nomi nudi, `ns_is_loaded`: default del linguaggio + `p->imports` dal pre-scan), così non esistono due tabelle di import da tenere allineate.

  Messaggio unico sulle due fronti: `namespace non importato: WORLD (aggiungi IMPORT WORLD).` Verifica del caso negativo: un programma con le sole cinque letture e senza `IMPORT` esce `rc=2` su c99n, IL, PCode e vmc. I 20 MiniTest lo dichiarano in testa.
* `WORLD.TIME()` ha un **TokenKind dedicato** (`WorldTime`): la keyword `TIME` esiste già e significa secondi dal giorno. Senza la voce in `gmb_ns.h` la qualifica non veniva scomposta e la gamba nativa ripiegava proprio su quella keyword — divergenza misurata e corretta.
* `WORLD_SECMOVE_CEIL` è una syscall **separata** (471) perché l'esecutore non riceve l'arità; in VM/C il vecchio range console `470..475` la ombreggiava (la tabella si scandisce in ordine) e il `SECMOVE` a 4 argomenti non muoveva nulla — le voci `470/471` sono state ritirate dal delegate console.

### P3 — Rasterizzatore mesh, target e present (6–9 gg) — **FATTO**

**Prerequisito**: [`Nota-Tecnica-Thread-Present-3D.md`](Nota-Tecnica-Thread-Present-3D.md) — invarianti `R2`–`R5`, `R8`, ciclo del frame, `RENDER` negli handler. Qui il frame si consuma su **T0** (`frame_acquire`), il cap del present diventa configurabile — **deciso**: una sola opzione `engine.present.max_fps`, default `auto` (60 col 3D, 30 col solo 2D; nota §8) — e il formato del buffer pubblicato è **deciso**: `0x00RRGGBB` come le pagine 2D (specifica §11).

* Decisione **chiusa** (specifica §11; confronto di costo fra le due opzioni in §11.6): target **ARGB `0x00RRGGBB`** + depth, alpha nelle texture. Il ciclo caldo è quindi: test depth → campiona texel → salta alpha 0 → moltiplica l'RGB per il livello di luce (fixed point) → scrivi la word. Nessun lookup di palette, nessuna tabella da costruire.
* Clip near/far + frustum, back-face, z-buffer, **UV prospettici**, texture nearest/bilinear, scissor al `VIEW`, batching per materiale.
* `gmb_3d_frame.c`: target = pagina visibile o canvas; colore `0x00RRGGBB`; la palette entra solo per i colori *nominati* (`palette_rgb`), mai nel ciclo caldo.
* **Accettazione**: sample `Samples3D/MeshBasics.bas` (cubo texturizzato, piano, 100 oggetti) 60 fps a 320×150; 8 golden scene (0 pixel di differenza in software); `SCENE.DRAWN` coerente con i tris rasterizzati; misura del tearing eseguita e riportata (`tools/sh/check_3d_present.sh`, **SKIP** dichiarato senza video) — il triple buffer si implementa **solo** se supera la soglia della nota §8.3.

**Stato al 2026-11-25 — il rasterizzatore c'è e la superficie `SCENE` è agganciata alle quattro gambe.** Il kernel di P3 è scritto e misurato e i verbi che il kernel sa eseguire (16 comandi + 7 letture) passano dal linguaggio su C99Native, IL, PCode e VM/C; `Samples3D/MeshBasics.bas` usa quella sintassi, senza una sola `DECLARE`. Restano fuori i comandi di §4.2 che presuppongono uno stato che il kernel non ha ancora: `THING`, lampade, `FOG` e `SKY` sono arrivati con P4; zone e player restano a P5.

* **Fatto — kernel**: `gmb_3d_mesh_draw` in `src/gmb_3d_frame.c` (con `gmb_3d_mesh_t`, `gmb_3d_material_t`, `gmb_3d_draw_stats_t` in `include/gmb_3d.h`): clip dei triangoli contro i 6 piani `|x|,|y|,|z| ≤ w` in **clip space** (fan `n ≤ 6`, i vertici nuovi interpolati su `w`), divisione prospettica, back-face sul segno dell'area in screen space, **z-buffer** alla distanza di vista (0 = niente, vince la più piccola — la stessa convenzione di `gm_sector_render`, quindi un pass di mesh dopo i settori testa i loro valori senza conversioni), **UV prospettici** (`u/w`, `v/w`, `1/w` interpolati), nearest e bilinear via `gm_tex_sample`, `masked`, `two_sided`, luce a 32 livelli, alpha della texture, null mode con contatori. Contatori: `tris_in/drawn/culled/clipped/pixels`.
* **Perché il clip in clip space e non `gm_frustum_*`**: `gm_gfx_geom.c` **non è** fra i sorgenti del runtime, quindi `gm_mat4_perspective`/`gm_frustum_from_matrix` non sono linkabili da `gmb_3d_frame.c` senza toccare il CMake (che richiede permesso). Il clip contro i 6 piani in clip space fa lo stesso lavoro con zero dipendenze nuove, e il test è banale: `|x| ≤ w`, `|y| ≤ w`, `|z| ≤ w`.
* **Due difetti reali trovati dai check, non a occhio**: il **winding** era invertito (una faccia frontale, CCW vista da fuori, con la y ribaltata dello schermo dà area **positiva**, non negativa) e il **confronto di depth** era al contrario (si scrive la distanza, quindi vince la più piccola). Entrambi fissati nel kernel e documentati in `gmb_3d.h`, entrambi sarebbero passati inosservati a una demo guardata a vista.
* **Accettazione numerica — verde**: `bash tools/sh/check_3d_oracle.sh` → gate C `cc`+`zcc` sui 5 file 3D, `P1-SELFTEST ok=121 fail=0` (26 check nuovi: winding, two-sided, z-buffer nei **due** ordini di disegno, frustum con la geometria dietro la camera, clip near, UV distinti ai quattro angoli su texture 2×2, errori espliciti `GMB_3D_E_ARG` per matrice mancante / texture senza UV / indice fuori range, null mode che non scrive) e `P1-ORACLE ok=12 diff=0`: 5 scene nuove (`mesh_cube`, `mesh_plane`, `mesh_clip`, `mesh_depth`, `mesh_cull`) con golden per RID più le 7 di P1/P2 ancora a 0 pixel di differenza. Le matrici delle scene si costruiscono **dentro l'oracolo** (perspective + look-at indipendenti), non con il codice del kernel: il test non si autoconferma.
* **Prestazioni misurate**: `--bench` 640×300, 120 frame → **med 0,65 ms** (limite della nota 4,00 ms), minimo 0,64. È il kernel, non il sample.
* **Superficie `SCENE` agganciata (2026-11-24) ed estesa (2026-11-25)** — sintassi di §4.2, stato solo nel runtime C, quattro gambe:

| Pezzo | Dove |
|---|---|
| TokenKind | `common/Ast/TokenKind.cs` + `gmb_tokenkind.h` rigenerato (`Scene`, 16 verbi `Scene*`, 7 letture `SceneVerts…ScenePixel`), `GMB_TOK_COUNT` 2692 → 2711 il 2026-11-24, → **2716** con `e53cbfcc`, → **2722** con P4 |
| lessico | `gmb_lexer.c` / `Lexer.Keywords.cs`: **solo** `SCENE` è keyword; i verbi no (vedi sotto), quindi nessun nome comune diventa riservato |
| comandi (statement) | `gmb_parser_stmt_domain.c` (`GMB_TOK_Scene` in `g_domain_ns` + normalizzazione verbo→TokenKind) + `native/gmb_c99native/src/gmb_c99_emit_3d.c` (`emit_scene_domain`) |
| letture (funzioni) | stesso emitter (`emit_bf_scene`) e tabelle **generate** `gmb_ns_lookup.h`/`gmb_fn_lookup.h`; richiedono `IMPORT SCENE` |
| ponte C# — superficie | `common/StdLib/StdLib.Scene.cs` (`RegisterCommand(TokenKind.Scene, …)` + 7 letture), `common/Ast/Scene3D.cs` (`SceneCommandStmt`), `common/Parser/Parser.Stmt.Scene.cs`, case in `Linker.Binding/Resolver/Rewrites` |
| ponte C# — P/Invoke | `common/CommonLib/Numerics/Scene3DNative.cs` → le STESSE `gmb_scene_*` di `gmb_3d_cmd.c`, via `libgmb_3d` |
| ponte C# — backend | `Compiler/Compiler.Scene.cs` (16 `SysCall` `SCENE_*`), `IlProgramCompiler.Scene.cs`/`IlRuntime.Scene.cs`, `PCodeProcessor.Syscalls.cs`/`PCodeProcessor.Builtins.Scene.cs` |
| ponte C — VM/C | `gmb_vm_core.c` (16 handler), `gmb_builtin.c` (7 builtin), `gmb_syscall_codes.h` (`GMB_SC_SCENE_*`) |
| facciata C | `native/gmb_runtime_c/src/gmb_3d_cmd.c` (`gmb_scene_*`): accumulo + `gmb_3d_mesh_draw`, nessuna logica duplicata (§0bis) |
| MiniTest | `shared/minitests/3d/scene_mesh.bas` — geometria, contatori (`DRAWN`/`CULLED`), determinismo dell'hash, `PIXEL` fuori target: **stessi valori su c99n, IL, PCode e vmc** |
| Guida utente | `Guide/utente/SCENE-Guida-Utente.md` — sintassi, parametri, semantica, gate `IMPORT SCENE`, errori e diagnosi |
| Sample | `Samples3D/MeshBasics.bas` — piano, cubo texturizzato, 100 oggetti, PNG e anteprima ASCII, **senza `DECLARE`** |

* **Accettazione misurata (2026-11-24)**: `Samples3D/MeshBasics.bas` su **tutte e quattro le gambe** (`tools/sh/gmb_c99native_run.sh ../Samples3D/MeshBasics.bas`, `test_ilrun --engine=il|pcode|vmc`) produce lo **stesso** output: 2428 vertici, 1214 triangoli dichiarati, **601 disegnati**, 620 scartati, 23 683 pixel dal depth test, hash FNV-1a **957 827 934** identico fra due render consecutivi, PNG in `Samples3D/MeshBasics.png`. Il diff riga per riga delle sei misure è vuoto su IL, PCode e VM/C rispetto alla gamba nativa.
* **Estensione del 2026-11-25 (`e53cbfcc`) — primo blocco di P4**: `SCENE LIGHT dx, dy, dz, livello` è luce **direzionale per faccia** (normale della faccia × direzione, ambiente 40, stessa quantizzazione intera su tutte le gambe); `SCENE SMOOTH|ALPHA|TWOSIDED on|off` espone i flag già presenti in `gmb_3d_material_t` (bilineare, alpha-test, niente back-face) e sopravvive a `RESET`; `SCENE TEXTURE "file.png"` carica un PNG con `png_decode`, l'unico decoder del runtime (limite 4096 px per lato, alpha 0 = trasparente, una texture sbagliata lascia la precedente); `SCENE PRESENT` porta il target nella pagina 2D corrente (la finestra la mostra il pump SDL sul main thread) e la guardia `gmb_scene_present_check` separa la copia dal gate GPM (`E_NO_TASK` fuori dal task). Con questi arrivano 4 MiniTest (`scene_light`, `scene_material_flags`, `scene_texture_file`, `scene_present`) e 3 sample (`LightFaces`, `MaterialFlags`, `TextureFile`); la guida cresce di 142 righe. Il dominio `SCENE` contava allora **16 verbi e 7 letture** (`GMB_TOK_COUNT` 2716) e `shared/minitests/3d` **25 MiniTest** (20 `WORLD` + 5 `SCENE`); il blocco successivo di P4 (2026-11-25) porta il dominio a **22 verbi e 7 letture** (`GMB_TOK_COUNT` 2722) e i MiniTest a **31**.
* **Verbi e letture agganciati**: `SCENE RESET | VIEW | TEXTURE | OBJECT | VERTEX | QUAD | BOX | OBJPOS | CAMERA | LIGHT | SMOOTH | ALPHA | TWOSIDED | RENDER | PRESENT | SAVE`; letture `SCENE.VERTS/TRIS/DRAWN/CULLED/PIXELS/HASH/PIXEL(x, y)`. `SAVE` e `PRESENT` sono aggiunte rispetto a §4.2 (cattura PNG e passaggio nella pagina 2D per l'accettazione); `TEXTURE` ha due forme — `TEXTURE celle` (scacchi procedurale) e `TEXTURE "file.png"` — e `OBJECT r, g, b` resta la tinta dell'oggetto: `TEXTURE n, a$` con handle numerico, `OBJECT o, flags, light` e `VERTEX o, …` arrivano con P4/P5, quando esistono materiali per oggetto e handle di texture.
* **Nessun verbo nuovo riservato.** `VIEW`, `TEXTURE`, `QUAD`, `BOX` e `SAVE` erano già parole del linguaggio; `RESET`, `OBJECT`, `VERTEX`, `OBJPOS`, `CAMERA` e `RENDER` sono nomi che i programmi usano come variabili. I due parser traducono il lessema del verbo **una volta sola** nei `TokenKind` `Scene*` (stesso schema di `CLOCK FPS`): AST, linker, compiler ed emitter dispatchano solo per TokenKind (§12), e nessun programma esistente perde un nome. Lato C la normalizzazione è `gmb_p_scene_verb_tk` in `gmb_parser_stmt_domain.c`, lato C# `SceneVerbKind` in `Parser.Stmt.Scene.cs`.
* **`IMPORT SCENE` è obbligatoria**, come per WORLD: la voce in `gmb_ns.h` fa riconoscere l'import come namespace **builtin** (niente ricerca su disco), il gate chiude comandi e letture. Caso negativo verificato: senza IMPORT il programma esce `rc=2` su tutte e quattro le gambe (C: `namespace non importato: SCENE (aggiungi IMPORT SCENE).`; C#/IL/PCode/VM-C: il `Token 'SCENE' inatteso` — la stessa asimmetria che WORLD ha già).
* **Non fatto, dichiarato**: i restanti comandi di §4.2 che presuppongono stato che il kernel non ha (`CLEAR`, `FOG`, `FAR`, `SKY`, `COLORMAP`, `THING`, `TURN`, `PLAYER`, `MOVE`, `LIFT`, `LAMP`, `ZONE`, `WORLD`, `SOFTWARE`/`HARDWARE`) e le letture della camera (`SCENE.X/Y/Z/EYE/YAW/PITCH/HIT/…`). Quindi restano non misurati: i **60 fps a 320×150** del ciclo completo, il **tearing** (`check_3d_present.sh` è SKIP senza video), il **batching per materiale** e lo **scissor al `VIEW`**. La convenzione «y è l'altezza» usata dal sample appartiene alla superficie (il kernel riceve la matrice ed è agnostico).

### P4 — Luce, materiali, ordine (5–8 gg) — **FATTO** (2026-11-25)

**Consegnato.** Luce **per pixel** (non per faccia), valutata nel punto del mondo interpolato:
`gmb_3d_light_eval` è l'unica formula, usata dal rasterizzatore e dai check. Il punto di partenza
era `e53cbfcc` (luce direzionale per faccia, flag di materiale, texture PNG, `PRESENT`); P4
aggiunge:

| Pezzo | Superficie BASIC | Dove |
|---|---|---|
| Lampada point (8 slot) | `SCENE LAMP n, x, y, z, raggio, quantità` | `gmb_scene_lamp` → `gmb_3d_env_t.lamps[]` |
| Lampada spot (cono) | `SCENE LAMP n, x, y, z, raggio, quantità, dx, dy, dz, angolo` | `gmb_scene_lamp_spot` |
| Fog lineare | `SCENE FOG d` | `gmb_scene_fog` → `env.fog` |
| Taglio di distanza | `SCENE FAR d` | `gmb_scene_far` → `env.far` |
| Cielo per angolo | `SCENE SKY repeat, shift` | `gmb_scene_sky` + `gmb_3d_sky_fill` |
| Rampa a 32 livelli | `SCENE COLORMAP` | `gmb_scene_colormap` → `env.colormap` |
| Billboard | `SCENE THING n, x, y, z, w, h` | `gmb_scene_thing` + `gmb_3d_thing_draw` |
| Flag e luce per oggetto | `SCENE OBJECT r, g, b, flags, luce` | `gmb_scene_object_flags` |

* **Formule, una sola sede** (`gmb_3d_frame.c`): lampada `amount × (1 − d/radius)`, cono
  `spot = (cos θ − cos α)/(1 − cos α)` limitato a `0…1`, fog `1 − dist/d`, colormap
  `q = (lv·31 + 127)/255` e `lv = q·255/31`, ambiente `40`, somma clampata a `255`.
* **Ordine (`M-ORDER`)**: cielo (solo pixel con depth 0, non scrive profondità) → opachi e masked →
  thing da lontano a vicino (depth test) → traslucidi da lontano a vicino con blend (alpha 255 =
  50/50, alpha < 255 = quella frazione).
* **Superficie su quattro gambe**: 6 `TokenKind` nuovi (`SceneLamp`, `SceneFog`, `SceneFar`,
  `SceneSky`, `SceneColormap`, `SceneThing`), `GMB_TOK_COUNT` **2716 → 2722**; parser C
  (`gmb_p_scene_verb_tk`) e C# (`SceneVerbKind`), emitter C99 (`emit_scene_domain`), syscall
  **499–506** (`SCENE_LAMP`, `SCENE_LAMP_SPOT`, `SCENE_FOG`, `SCENE_FAR`, `SCENE_SKY`,
  `SCENE_COLORMAP`, `SCENE_THING`, `SCENE_OBJECT_FLAGS`), P/Invoke `Scene3DNative`, IL/PCode,
  VM/C (`gmb_vm_core.c` + `gmb_syscall_codes.h`). Dominio: **22 verbi e 7 letture**.
* **Accettazione**: `bash tools/sh/check_3d_oracle.sh` → `P1-SELFTEST ok=139 fail=0` (18 check
  nuovi: attenuazione, cono dentro/fuori, fog a metà e oltre, FAR, quantizzazione, cielo
  avvolgente, thing, ordine di disegno), `P1-ORACLE ok=18 diff=0` con **6 golden nuove**
  (`lamp_point`, `lamp_spot`, `fog`, `sky`, `thing`, `order`) per RID, gate A/B `cc`+`zcc` verde,
  bench 640×300 med **0,65 ms**. I 6 MiniTest P4 danno gli **stessi valori** su C99Native, IL,
  PCode e VM/C.
* **Due difetti trovati dai MiniTest, non a occhio**: `gmb_scene_render` usciva se non c'era
  nessuna mesh (una scena di **solo cielo** o di **soli thing** non disegnava nulla) e la luce base
  dell'oggetto veniva sovrascritta dal livello per-triangolo anche **senza** `LIGHT`. Entrambi
  corretti nel kernel (`gmb_3d_cmd.c`), con il default `255` che lascia identiche le scene P3.
* **Divergenze dichiarate**: `SKY` richiede **entrambi** gli argomenti (`repeat, shift`) e usa la
  **texture corrente** invece di un indice (`SKY tex [, repeat [, shift]]` in §4.2); `THING` usa la
  texture corrente; `LAMP` è un solo verbo con due arità (6 e 10), non due comandi; la luce del
  thing non ha ancora un verbo BASIC (`gmb_scene_thing_light` è solo facciata C).

### P5 — Scena, animazione, scalabilità (8–12 gg) — **IN CORSO** (primo blocco + superficie interattiva, 2026-11-26)

* Culling spaziale + frustum per oggetto, LOD, instancing per materiale, `KEY` TRS, skinning 64 ossa, zone, `SCENE.STATS`.
* `tools/sh/bench_3d.sh`: tabella misurata (tris, draw, ms, MB) su 3 risoluzioni; prima overhead, poi fill (§0septies).
* **Accettazione**: 500 oggetti / 200k tris a ≥30 fps a 640×360 software; curva di degradazione misurata.

**Consegnato il 2026-11-26 (primo blocco: culling per oggetto e banco).**

| Pezzo | Dove | Verifica |
|---|---|---|
| Culling per **oggetto** (AABB + 6 piani del frustum, conservativo) | `src/gmb_3d_scene.c` (`gmb_3d_frustum_planes`, `gmb_3d_object_outside`), chiamato da `gmb_3d_cmd.c` prima di ogni draw | `gmb_3d_oracle --cullcheck`: **P5-CULL ok=10 fail=0** |
| Matrici del renderer fuori dalla facciata | `gmb_3d_scene.c` (`gmb_3d_m4_*`), prototipi in `gmb_3d_internal.h` | gate A/B `cc`+`zcc` sul file; `gmb_3d_cmd.c` **1178** righe (era 1305) |
| Tetto oggetti 256 → **512**, poi capacità **dinamica** (vedi terzo blocco) | `GMB_SCENE_OBJS_INIT/MAX` (`gmb_3d_internal.h`) | 500 oggetti nel banco |
| Banco misurato a 3 risoluzioni | `tools/sh/bench_3d.sh` → `gmb_3d_oracle --bench-scene` | tabella sotto |
| Accettazione «500 oggetti a ≥30 fps» | `tools/sh/check_3d_oracle.sh` (3quater) | **mediana ≤ 33,3 ms** a 640×360 |

**Perché il culling non si vede**: il rasterizzatore scarta già i triangoli fuori dal frustum
uno per uno e li conta in `CULLED`; un oggetto interamente fuori ha tutti i triangoli fuori. Il
culling per oggetto prende la stessa decisione una volta sola, con un AABB e un margine, e salta
la preparazione. Prova: con gli oggetti fuori campo l'hash del frame è **identico** a quello di
una scena in cui quegli oggetti non sono mai stati dichiarati (`--cullcheck`), e le **20 golden
restano `diff=0`**.

**Misura** (`bash tools/sh/bench_3d.sh 500 60`, `osx-arm64`, macchina condivisa — min e mediana):

| risoluzione | tris | drawn | culled | min ms | med ms | fps (mediana) |
|---|---|---|---|---|---|---|
| 320×180 | 6000 | 2989 | 3059 | 0,823 | 0,842 | 1188 |
| 640×360 | 6000 | 2989 | 3059 | 1,714 | 1,753 | 571 |
| 960×540 | 6000 | 2989 | 3059 | 3,149 | 3,233 | 309 |

`culled=3059` sono le facce posteriori dei 500 box chiusi (back-face), non oggetti scartati: la
scena del banco è tutta davanti alla camera. La demo a settori resta a **0,69 ms** mediana
(limite 4 ms).

**Consegnato il 2026-11-26 (secondo blocco: superficie interattiva).**

| Pezzo | Dove | Verifica |
|---|---|---|
| Corpo, `MOVE`/`TURN`/`LIFT`, collisione con scivolamento, gradini ≤ step, soffitto, `HIT` | `src/gmb_3d_scene_actor.c` (nuovo: `gmb_scene_player/move/turn/lift`) | `gmb_3d_oracle --interactcheck`: **P5-INTERACT ok=45 fail=0** |
| `FLOORAT`/`CEILAT` sulle AABB MONDO degli oggetti, flag 4 passabile | `gmb_3d_scene_bounds` (`gmb_3d_cmd.c`) + `gmb_scene_floor_at/ceil_at` | stessi check; MiniTest `scene_zones.bas` |
| Zone (`ZONE`/`ZONETAG`), prima creata vince, tolleranza `−2` | `gmb_scene_zone/zone_at/zonetag_text` (`gmb_3d_scene_actor.c`) | MiniTest `scene_zones.bas` |
| Camera/corpo leggibili: `SCENE.X/Y/Z/EYE/YAW/PITCH/HIT` | facciata C + 4 gambe | MiniTest `scene_move.bas` |
| 15 TokenKind nuovi, parser C#/C, emitter C99, IL, PCode, VM/C (510–514 e 3920–3930) | `TokenKind.cs`/`gmb_tokenkind.h`, `gmb_c99_emit_3d.c`, `IlRuntime.Scene.cs`, `PCodeProcessor.*`, `gmb_vm_core.c`/`gmb_builtin.c` | le 36 prove di `shared/minitests/3d` PASS; 2 MiniTest nuovi identici su 4 gambe |

`CAMERA` continua a posizionare l'**occhio** (le 20 golden restano `diff=0`): i piedi stanno
`EYE` sotto. Divergenza di assi dichiarata in specifica (`C-DIVERGE-AXES`, y-up).

**Consegnato il 2026-11-27 (terzo blocco: buffer a capacità dinamica e accettazione 200k).**

| Pezzo | Dove | Verifica |
|---|---|---|
| Buffer geometria da statici a **capacità dinamica** (raddoppio alla dichiarazione) | `src/gmb_3d_scene_buf.c` (nuovo): `gmb_3d_scene_geom()`, `reserve_verts/tris/objs`; partono da 32768 vertici / 16384 triangoli / 512 oggetti | tetti alzati a **4.194.304 / 2.097.152 / 65.536**; gate A/B `cc`+`zcc` sul file |
| **Nessuna allocazione per frame** | scratch dei traslucidi (ordine/distanza) e AABB mondo riservati con gli oggetti; `SCENE PICK` alloca una volta per target e azzera a ogni frame | contatore `gmb_3d_alloc_count()`: il banco stampa `allocs=0` e **fallisce** se un frame alloca |
| AABB mondo in buffer persistente (non più `boxes[512]`) | `gmb_3d_scene_bounds_all` in `gmb_3d_scene_buf.c`, usato da `FLOORAT`/`CEILAT`/`MOVE` | `P5-INTERACT ok=45 fail=0`; MiniTest `scene_move.bas`/`scene_zones.bas` invariati |
| Banco a triangoli | `gmb_3d_oracle --bench-scene --tris N` (griglia frontale, 12 triangoli per box) + `tools/sh/bench_3d.sh --tris N` | accettazione **200k dentro `check_3d_oracle.sh`** (3quater-bis) |

**Misura dei 200k** (`bash tools/sh/bench_3d.sh --tris 200000`, `osx-arm64`, macchina condivisa,
20 frame per risoluzione):

| risoluzione | tris | drawn | culled | min ms | med ms | fps (mediana) |
|---|---|---|---|---|---|---|
| 320×180 | 200.004 | 99.742 | 100.262 | 16,881 | 17,286 | **57,9** |
| 640×360 | 200.004 | 99.742 | 100.262 | 19,061 | 19,162 | **52,2** |
| 960×540 | 200.004 | 99.742 | 100.262 | 21,296 | 21,466 | **46,6** |

`culled=100.262` sono le facce posteriori (6 su 12 per box): `drawn` ~100k è la metà frontale dei
200k dichiarati. **Accettazione P5 soddisfatta**: 52 fps mediana a 640×360, contro il limite di 30.
Curva di degradazione a 640×360 (stessa scena, solo densità): 50k → 5,209 ms (192 fps),
100k → 10,088 ms (99 fps), 200k → 19,162 ms (52 fps) — lineare, nessun ginocchio.

**Memoria** della scena da 200k triangoli (capacità riservate, non la dimensione usata: 524.288
vertici e 262.144 triangoli, il raddoppio sopra il necessario): vertici 23,1 MB (pos 6,3 + UV
4,2 + normali 6,3 + normali-mondo 6,3), triangoli 3,7 MB (indici 3,1 + due luci 0,5), oggetti,
scratch e AABB 3,3 MB → **≈ 30 MB** più il target (1,8 MB a 640×360). Tutto riservato alla
dichiarazione e riusato: `SCENE RESET` non libera e non rialloca.

**Consegnato il 2026-11-27 (quarto blocco: raggi).**

| Pezzo | Dove | Verifica |
|---|---|---|
| `SHOOT(range)`, `USE(reach)`, `SEES(n)` e lo stato dell'ultimo colpo (`SHOTKIND/SHOTOBJ/SHOTDIST/SHOTX/Y/Z`) | `src/gmb_3d_ray.c` (nuovo), facciata in `include/gmb_3d.h` | `gmb_3d_oracle --raycheck`: **P5-RAYS ok=22 fail=0**, dentro `check_3d_oracle.sh` |
| Ostacoli = AABB MONDO degli oggetti (una sola fonte con `MOVE`/`FLOORAT`), id dell'oggetto come `SCENE.PICK` | `gmb_3d_scene_bounds_all` + nuovo campo `id` in `gmb_3d_aabb_t` | nessuna allocazione: il buffer è quello riservato dalla scena |
| Flag 8 (see-through) attraversato da colpi e vista; flag 4 (passabile) **non** è trasparenza; `USE` ignora i flag | `gmb_3d_ray.c` | `scene_rays.bas`: stessi valori su C99Native, IL, PCode e VM/C |
| Thing = cilindro verticale (`w/2`, da `y` a `y+h`); `SEES` parte dal 70% dell'altezza | `gmb_3d_scene_things` (accessore in sola lettura, l'array resta in `gmb_3d_cmd.c`) | stesso MiniTest; `SHOTDIST` = 15 con thing a `z = 20` e `w = 10` |
| 9 TokenKind nuovi (2748 in C), 9 builtin (3931–3939) su 4 gambe | `TokenKind.cs`/`gmb_tokenkind.h`, `gmb_ns_lookup.h`, `StdLib.Scene.cs`, IL/PCode/VM-C/C99 | `check_enum_ids` verde; 1 MiniTest nuovo identico su 4 gambe |

**Non ancora fatto, dichiarato**: LOD, instancing per materiale, `KEY` TRS, skinning 64 ossa
e `SCENE.STATS`.

### P6 — Audio 3D e input (5–7 gg)

* Audio posizionale (distanza + pan) sopra il mixer; listener = camera; `WORLD/SCENE SOUND`.
* Mouse-look, pad, jump/crouch; handler solo su task GPM.
* **Accettazione**: MiniTest deterministici su attenuazione/pan; sample "walk".

### P7 — Parità 2D Engine1999 (12–18 gg)

* Tutto §5. Shim `shared/libs/e99compat.inc` per le parti esprimibili come routine.
* **Accettazione**: checklist §1.2–1.5 verde con un MiniTest per riga e una demo per sottosistema.

### P8 — Tooling (6–9 gg)

* `tools/basic/scene_census.bas`, `tools/sh/bench_3d.sh`, `check_3d_renderers.sh`, `tools/c/gltfdump.c`, convertitore `obj→gmbscene`, `tools/sh/check_3d_assets.sh`.
* **Accettazione**: gate in `tools_verify.sh`; `tools/README.md` aggiornato.

### P9 — Documentazione e release (5–8 gg)

* Guide `SCENE-*`, `WORLD-*`, polimorfismo, tabella di migrazione da Engine1999; schede tool con `tools_docs.sh`.
* **Accettazione**: ogni comando ha MiniTest + sample + guida (§4.1 AGENTS); `check_minitests.sh` verde; nessun file > 1200 righe.

### PH — Hardware con parità (8–12 gg) — **fuori numerazione: ultimo, o mai**

Sta in fondo di proposito: non compare nella sequenza P0–P9 e non è un requisito per nessuna fase precedente.

* Livello 1: `SDL_RenderGeometry` (SDL ≥2.0.18): attributi per vertice, **senza depth** → sort per oggetto; HUD/particelle/UI 3D.
* Livello 2: present **Vulkan/Metal** esistente o GL caricato a runtime (proc address) con pipeline depth.
* `SCENE HARDWARE|SOFTWARE`, default `auto`, `SCENE.HWAVAIL`.
* **Gate**: `tools/sh/check_3d_renderers.sh` confronta 10 scene: media ≤2/255, pixel diversi ≤2% (bordi); GPU assente = **SKIP dichiarato**, non verde finto.
* **Accettazione**: parità nei limiti; nessun crash senza GPU; `SCENE.RENDERER` coerente.

---

## 7. File: mappa concreta

### 7.1 `gmbsuite/gmbgfx-3d` (nuovi sorgenti, poi vendored)

| File | Righe | Contenuto |
|---|---|---|
| `src/gm_gfx_mesh.c` + h | **292 + 64 misurate** (erano 118 + 33) | normali/UV `gm_mesh_attrs`, 4 generatori `*_uv`, `gm_mesh_normals` — **fatto in P0** |
| `src/gm_gfx_raster.c` + h | **267 + 80 misurate** (erano 115 + 41) | `gm_tex`, `gm_raster_triangle_tex`, `gm_raster_to_argb` — **fatto in P0** |
| resto del render target (blend, mip, luce a livelli) | **~200 stimate** (≈8 funzioni × 25) | alpha blending traslucido, mip-lite, luce in fixed point |
| `src/gm_gfx_scene.c` + h | ~900 (≈30 funzioni × 30) | gerarchia, ordine, culling, LOD |
| `src/gm_gfx_sector.c` + h | ~1000 (≈35 funzioni × 28) | settori/portali, moving sectors, sky |
| `src/gm_gfx_light.c` + h | ~500 (≈18 funzioni × 28) | color map, lampade, fog, traslucenza |
| `src/gm_gfx_anim.c` + h | ~500 (≈18 funzioni × 28) | keyframe, skinning |
| `test/` (estensione) | ~400 (**misurato: 124 righe per 29 check** ⇒ ~4 righe/check ≈ 90 check) | nuovi check nella suite |

> **Base delle stime** (misurata, non citata): 1.248 righe / 62 prototipi C nei 5 header (`geom` 22, `intersect` 9, `ray` 10, `mesh` 13, `raster` 8) = **~20 righe per funzione**; i file di motore, con gestione errori, stanno sui **28-30**. Le righe in grassetto sono contate con `wc -l`; le altre sono `funzioni previste × 20-30` e vanno rilette a ogni fase.

### 7.2 Monorepo — runtime (stato e bridge)

| File | Righe | Contenuto |
|---|---|---|
| `include/gmb_3d.h` | ~300 (~15 funzioni × 20) | API `gmb_3d_*` |
| `src/gmb_3d_api.c` | ~800 | facade, handle, opzioni, errori |
| `src/gmb_3d_frame.c` | ~500 | VIEW/clip, target, present 3D |
| `src/gmb_3d_scene.c` | **143 misurate** (2026-11-26) | matrici del renderer e culling per oggetto — **fatto in P5**; oggetti/thing/lampade/zone restano in `gmb_3d_cmd.c` |
| `src/gmb_3d_mesh3.c` | ~600 | attributi, materiali, upload texture |
| `src/gmb_3d_sector.c` | ~400 | binding su `gm_gfx_sector` |
| `src/gmb_3d_collide.c` | ~600 | move/shoot/use/sees |
| `src/gmb_3d_light.c` | ~300 | binding su `gm_gfx_light` |
| `src/gmb_3d_anim.c` | ~300 | binding su `gm_gfx_anim` |
| `src/gmb_3d_io.c` | ~700 | JSON (cJSON) world/scene, OBJ/GLB |
| `src/gmb_3d_audio.c` | ~300 | audio posizionale |
| `src/gmb_gfx_present_sdl3d.c` | ~500 | `SDL_RenderGeometry` (opz.) |
| `frontend/src/parser/gmb_parser_stmt_scene.c` | ~700 | parser `SCENE` |
| `frontend/src/parser/gmb_parser_stmt_world.c` | ~700 | parser `WORLD` |
| `native/gmb_c99native/src/gmb_c99_emit_3d.c` | ~800 (≈9 righe × 90 comandi) | emissione |
| `GMBasicSharp/common/StdLib/StdLib.Scene3D.cs` | ~700 (**verificato**: `StdLib.Graphics.cs` = 289 righe / 36 `RegisterCommand` = **8,0 righe per comando**; ~90 comandi × 8 ≈ 720) | registrazione + P/Invoke |

> **Base delle stime runtime** (misurata): parser per dominio **385-754 righe** (`gmb_parser_stmt_net.c` 754 per NET, `controls` 490, `assign` 385) → ~700 per `SCENE`/`WORLD` è coerente; `gmb_c99_emit.c` è **37.914 righe** (core dispatcher, eccezione documentata) quindi l'emissione va in un file nuovo; registrazione C# **8,0 righe/comando** misurate.

### 7.3 Monorepo — modifiche puntuali

`TokenKind.cs` (+`Scene`,`World` e sotto-verbi) · `gmb_tokenkind.h/.c` (rigenerati) · `gmb_lexer.c` + `Lexer.Keywords.cs` · `gmb_parser_statements.c` (hook) · `gmb_c99_emit.c` (dispatch) · `shared/libs/graphics/{src,include}` + **`VENDOR.md`** · `shared/libs/graphics/build.sh` (se serve) · `native/gmb_runtime_c/CMakeLists.txt` · `tools/README.md` · `gmb_audit` · `docs/manual/`.

---

## 8. Workflow a due repo (obbligatorio)

1. Si scrive in **`gmbsuite/gmbgfx-3d`** (con la sua `make -C gmbgraphics test` verde).
2. Si copia in **`shared/libs/graphics`** e si aggiorna la riga in `VENDOR.md`.
3. `bash shared/libs/graphics/build.sh` → `libgmbgraphics.dylib`.
4. `bash tools/sh/gmb_lib.sh check graphics` (simboli e superficie).
5. MiniTest del monorepo: `gfx3d_core`, `gfx3d_more` + i nuovi.
6. Commit **separati** nei due repo, con path espliciti, senza footer.

Nessuno dei due passi 1–5 si salta: la copia vendored è l'unica che il runtime compila.

---

## 9. Verifica

1. **MiniTest per ogni comando/funzione** (`shared/minitests/3d/*.bas`), oracolo deterministico in null mode (§4.1 AGENTS).
2. **Golden image**: 20+ scene su canvas, confronto `IMGDIFF` (0 differenze in software).
3. **Parità software/hardware**: soglia dichiarata, SKIP motivato senza GPU.
4. **Parità fra backend**: stesso MiniTest su IL/PCode/VM-C/C99Native (runtime unico → per costruzione; il test lo dimostra).
5. **A/B C**: `--cc cc` e `--cc tools/sh/zcc` sui sorgenti C toccati.
6. **Prestazioni misurate** (mai stimate): `tools/sh/bench_3d.sh` con tabella.
7. **Suite**: `make -C gmbgraphics test` (220 oggi, + i nuovi) e `gmb_lib.sh check graphics`.
8. **Gate statici**: `check_file_size.sh --quiet`, `check_enum_ids`, `gen_c_tokenkind --check`, `tools_docs.sh --check`.
9. **Nessuno sweep senza permesso**: si esegue il singolo file toccato.
10. **Misure, non citazioni**: ogni numero nel piano si riverifica con un comando, e il numero che conta è quello **stampato dal binario**, non il conteggio del testo:
    ```bash
    make -C gmbgraphics test                                  # "PASS: N FAIL: M" (N = check eseguiti)
    grep -o 'check(' gmbgraphics/test/run_tests.c | wc -l      # presenze testuali (include definizione e rami non percorsi)
    grep -cE '^DECLARE' shared/include/gfx3d.inc               # 60 righe DECLARE (62 le occorrenze della parola: 2 nei commenti)
    wc -l shared/libs/graphics/src/gm_gfx_{mesh,raster}.c      # 292 e 267 dopo P0 (erano 118 e 115)
    wc -l GMBasic/native/gmb_compiler_c/frontend/src/parser/gmb_parser_stmt_*.c   # base stime parser
    grep -c 'RegisterCommand(' GMBasicSharp/common/StdLib/StdLib.Graphics.cs     # 36 su 289 righe = 8,0/comando
    ```
    Un numero che non si riesce a misurare si scrive come **stima con la sua base** (`funzioni × 20-30`, `righe/comando`), mai come cifra secca.

---

## 10. Rischi e mitigazioni

| Rischio | Gravità | Mitigazione |
|---|---|---|
| **Contaminazione GPL** (Engine1999) | altissima | solo specifica; revisione separata; nessun file GPL nel repo |
| **Drift suite ↔ vendored** | alta | procedura §8 con 3 gate (`VENDOR.md`, `build.sh`, `gmb_lib.sh check`) |
| **Doppia istanza** (dylib FFI + runtime statico) | alta | stato solo nel runtime; `GFX3D` resta stateless; spike P0 con accettazione esplicita |
| **ABI `gm_mesh`/`gm_raster` rotta** | media | estensioni additive (in P0: `gm_mesh_attrs` separato invece dei campi in coda); le 249 righe / 53 asserzioni di `gfx3d_*` sono il contratto |
| Prestazioni CPU a 640×480 | media | scale (320×150 default), `FAR`, colormap precalcolata, `VIEW` stretto, P5 GPU |
| Determinismo software vs GPU | media | software = reference; tolleranza; nessun golden GPU |
| TokenKind churn (1918 token, audit 982) | media | 2 gruppi nuovi; sotto-verbi su token esistenti; `--check` in gate |
| Quattro backend divergono | media | logica in C, C# via P/Invoke |
| File monolitici | media | split per dominio previsto; gate 1200 righe |
| **Permesso su gmbsuite** | alta | §11: le modifiche alla suite si fanno solo dopo autorizzazione esplicita |

---

## 11. Cosa serve per partire (aggiornato)

* ~~Autorizzazione a scrivere in `gmbsuite/gmbgfx-3d`~~ — **concessa**: P0 è stato eseguito (spike + estensioni additive).
* ~~Un solo schema JSON, `WORLD CHECK`, semantica di `SEES`~~ — **chiuse il 2026-11-19** (specifica §10 e §12; qui §3.1.8–10). ~~Kernel di comandi~~ — **consegnato per P0–P2** il 2026-11-19 (9 comandi + 5 letture del dominio `WORLD`, 20 MiniTest su tutte e quattro le gambe); **P3** è consegnato (motore mesh + superficie `SCENE`, 16 verbi e 7 letture su quattro gambe) e **P4 è consegnato** per intero (luce direzionale e per pixel, lampade point/spot, fog, FAR, sky, `COLORMAP`, `THING`, ordine di disegno; §12 riga 6).
* **Revisione indipendente** della specifica da parte di chi non ha letto il riferimento, se si vuole una garanzia di licenza più forte di quella del registro.
* Permesso per i test di fase (singoli file) — implicito nelle regole di casa.

---

* **Autorizzazione a scrivere in `gmbsuite/gmbgfx-3d`** (progetto separato; anche il suo `AGENTS.md` impone il permesso) — è il punto bloccante n.1.
* Conferma delle scelte: `SCENE`+`WORLD`; 320×150 di default con scale; software di default finché P5 non è verde; parità **semantica** (non pixel-perfect) con Engine1999; porting solo dei **dati** (mappe JSON), non del codice.
* Permesso per la build di verifica e per i MiniTest della fase P0 (singoli file, non sweep).

Con P0+P1 (≈1 settimana) si ha la pipeline di verifica; con P2 (≈3–4 settimane cumulative) un mondo a settori giocabile con porte, ascensori, luci e collisioni — la parte del riferimento che si vede; con P3+P4 (≈6–7 settimane) anche il cubo texturizzato con z-buffer, UV prospettici e luci su tutti e quattro i backend **riusando `gm_gfx_*`**.

---

## 12. Follow-up (stato al 2026-11-25)

| # | Follow-up | Stato | Dove |
|---|---|---|---|
| 1 | Licenza e provenienza (la "clean room" non reggeva) | **Fatto**: registro + regole + checklist; resta aperta la revisione indipendente | `Provenienza-3D.md` |
| 2 | Handle fra le due istanze (dylib vs runtime) | **Deciso**: trasferimento **per dati**, mai per handle; da riflettere nella superficie | piano §3.1.3, specifica §8 |
| 3 | Modello di thread e present (GPM + main thread SDL) | **Fatto e chiuso**: nota con attori, `R1`–`R9`, ciclo del frame, sincrono vs park, `RENDER` negli handler, headless, vincolo GL; le 3 domande finali sono decise (nota §13): cap unico `auto` (60 col 3D, 30 senza), triple buffer solo dietro misura (nota §8.3), identità **del task** (`gmb_cur()`) perché il GPM è multi-worker | `Nota-Tecnica-Thread-Present-3D.md` |
| 4 | Oracolo headless prima dei renderer | **Fatto e chiuso in P1 (2026-11-19), esteso in P3 (2026-11-24) e P4 (2026-11-25)**: `tools/sh/check_3d_oracle.sh` con gate A/B `cc`+`zcc`, `--selftest` **139 check / 0 fail** (95 di P1/P2 + 26 di P3 + 18 di P4), **18 golden** per RID (`--check` → `diff=0`, incluse le 5 scene mesh e le 6 scene P4), `--perturb 1` respinto con `exit 1` | piano §6 P1, P3, P4 |
| 5 | Palette vs ARGB | **Deciso: ARGB** — target `0x00RRGGBB` come le pagine, luce a 32 livelli fixed point, alpha dalle texture, palette solo per i colori nominati. Il fill indicizzato guadagna ~2–5 decimi di ms (stima) e costa un secondo modello di colore: non paga | specifica §11 (motivo) e §11.6 (confronto di costo) |
| 6 | Budget di superficie (90 comandi × 4 backend) | **In corso**: kernel di ~25 comandi per P0–P4 (MVP + i due renderer + luci), resto in "superficie v2". Consegnato il kernel di **P0–P2** il 2026-11-19 — 9 comandi (`WORLD LOAD/CLOSE/TICK`, `SECMOVE`, `HEIGHT`, `ANIM`, `SCROLL`, `LIGHTFX`, `LAMP`) e 5 letture (`WORLD.MOVING/LIGHTNOW/TIME/FLOORZ/CEILZ`) con TokenKind canonici, agganciati **a tutte e quattro le gambe** (ponte C# via `libgmb_3d`) e coperti da 20 MiniTest. Per **P3** sono consegnati il **motore** (`gmb_3d_mesh_draw`: target ARGB+depth, clip/frustum, z-buffer, UV prospettici, back-face, nearest/bilinear, masked, two_sided) con 5 golden e 26 check, e la **superficie BASIC** `SCENE`: dal 2026-11-24 11 verbi + 7 letture su tutte e quattro le gambe, dal 2026-11-25 (`e53cbfcc`) `LIGHT`, `SMOOTH`/`ALPHA`/`TWOSIDED`, `TEXTURE "file.png"` e `PRESENT`, per **16 verbi e 7 letture**; P4 (2026-11-25) aggiunge `LAMP` (6/10 argomenti), `FOG`, `FAR`, `SKY`, `COLORMAP`, `THING` e `OBJECT` a 5 argomenti, per **22 verbi e 7 letture**. Copertura: 22 MiniTest `SCENE` in `shared/minitests/3d`, 18 golden per RID, 139 check di selftest. Il sample d'accettazione `Samples3D/MeshBasics.bas` gira su tutte e quattro le gambe con lo stesso output. **P4 è consegnato** (lampade point/spot, fog, FAR, sky, `COLORMAP`, `THING`, ordine di disegno) con 6 MiniTest e 6 golden, stessi valori su quattro gambe. **P5** ha consegnato il culling per oggetto, il tetto di 512 oggetti, il banco misurato e la **superficie interattiva** (`PLAYER`/`MOVE`/`TURN`/`LIFT`, `ZONE`/`ZONETAG`, `FLOORAT`/`CEILAT`, letture `SCENE.X/Y/Z/EYE/YAW/PITCH/HIT`) con i MiniTest `scene_move.bas` e `scene_zones.bas`: le 36 prove di `shared/minitests/3d` PASS, le 20 golden `diff=0`. Il terzo blocco (2026-11-27) porta i buffer a **capacità dinamica** e consegna l'**accettazione dei 200k triangoli** (52 fps mediani a 640×360, `allocs=0`, dentro `check_3d_oracle.sh`). Il **quarto blocco** (2026-11-27) consegna i **raggi** (`SHOOT`/`USE`/`SEES` + stato dell'ultimo colpo): ostacoli sugli AABB mondo degli oggetti, flag 8 attraversato e flag 4 no, thing come cilindri, letture senza errori di superficie, con `P5-RAYS ok=22` e il MiniTest `scene_rays.bas` su quattro gambe. Restano LOD, instancing, `KEY` TRS, skinning e `SCENE.STATS` | specifica §7; piano §6 P2, P3, P4, P5 |
| 7 | Ordine delle fasi | **Applicato come numerazione**: P0 → P1 oracolo → P2 settori → P3 mesh → P4 luce → P5 scena → P6–P9 → `PH` hardware fuori numerazione | piano §6 |
| 8 | Formati versionati e passo fisso | **Deciso in v1**: un solo schema `gmb-3d` con `kind` obbligatorio e un documento per file (`.gmbscene`), migrazione dichiarata, `F-ROUNDTRIP` come test del formato; `T-FIXED`, `T-CLOCK` come da specifica | specifica §4, §7, §12.1 |
| 9 | Inventario come oracolo (non prosa) | **Fatto**: specifica con ID e tabella ID→test | `3D-Specifica-Comportamenti.md` |
| 10 | Rischi non coperti (gate 1200 righe, asset, regressione 2D, bench, Windows, dead-strip) | **Parzialmente chiuso**: asset vincolati (`F-TEXTURES`), dead-strip già adottato, regressione 2D del present evitata dal cap `auto` (30 senza 3D); restano gate righe per i nuovi file, golden 2D, perimetro Windows | specifica §9 |
| 11 | `WORLD CHECK`: comando o tool | **Deciso: tool** (`tools/c/world_check.c`), con le invarianti **fatali dentro il load**: nessun comando, nessun secondo punto d'ingresso | specifica §12.2 |
| 12 | Sentinella di `SEES` | **Deciso: `1`/`0`/`−1`** (thing inesistente), divergenza dichiarata `C-DIVERGE-SEES` | specifica §12.3 |
| 13 | `MOVING(s)` = `−1` mentre si muove | **Chiuso in P2 (2026-11-19) con deroga scritta**: `−1` in moto, `0` fermo — è la sentinella che gli script BASIC del riferimento già usano, e la deroga alla regola "`−1` = assente" sta accanto alla riga `T-SECMOVE` della specifica | specifica `T-SECMOVE` |
| 14 | **Compatibilità GPM di tutto il motore 3D** | **Fatto e chiuso**: comandi/`RENDER` solo in un task, I/O che parca con ripiego sincrono fuori, ciclo park-based (`R10`), load atomico con validazione nel task (`R11`), stato 3D verso T0 solo per atomi, nessuna seconda via di dispatch | piano §3.1.11; nota §2, §3, §8.1, §8.3, §10, §14; specifica `C-GPM` |
| 15 | **Ponte C# dei comandi 3D (IL/PCode/VM-C)** | **Chiuso (2026-11-19)**: `StdLib.World.cs` (`RegisterCommand`/`RegisterFunction`), nodo `WorldCommandStmt`, linker dei tre backend, P/Invoke `World3DNative` → le stesse `gmb_world_*` via `libgmb_3d`, syscall/builtin in VM/C. Ritirati il marcatore `native-only`, `gmb_p_native_ns_lookup` e le righe WORLD di `gmb_members.h`; i 20 MiniTest girano su tutte e quattro le gambe | piano §6 P2 (aggancio al linguaggio), §7.2 |

**I tre che valgono più di tutti**, nell'ordine: (1) provenienza — fatto; (2) oracolo headless — fatto in P1; (3) handle fra istanze — deciso. La **nota di thread/present** (3) e la **decisione palette/ARGB** (5, → ARGB) sono fatte. Dal 2026-11-19 sono chiuse anche le sei domande che P2 ereditava: le tre di formato/semantica — schema JSON unico (§3.1.8), `WORLD CHECK` come tool con validazione nel load (§3.1.9), `SEES` 1/0/−1 (§3.1.10) — e le tre della nota tecnica (cap `auto`, triple buffer solo dietro misura, identità del **task** invece del thread). Restano aperti la **revisione indipendente** della provenienza (riga 1) e il **kernel di P5** (culling spaziale, LOD, instancing, animazione; riga 6). Dal 2026-11-19 c'è anche l'obbligazione trasversale della **compatibilità GPM** (riga 14, §3.1.11): non è una scelta, è un vincolo che ogni fase verifica in accettazione. **P5 è in corso**: il primo blocco (2026-11-26) consegna il **culling per oggetto** (AABB + sei piani, conservativo: le 20 golden restano `diff=0`), il tetto di **512 oggetti**, il banco misurato `tools/sh/bench_3d.sh` e l'accettazione «500 oggetti a ≥30 fps a 640×360» dentro `check_3d_oracle.sh`. Il secondo blocco (2026-11-26) consegna la **superficie interattiva**: `PLAYER`/`MOVE`/`TURN`/`LIFT` con collisione a scivolamento, gradini e soffitto, `HIT`, `ZONE`/`ZONETAG`, `FLOORAT`/`CEILAT` e le letture `SCENE.X/Y/Z/EYE/YAW/PITCH` — su tutte e quattro le gambe, con i MiniTest `scene_move.bas` e `scene_zones.bas`. Il terzo blocco (2026-11-27) porta i buffer della scena da statici a **capacità dinamica**: 200.000 triangoli dichiarati rendono a **52 fps mediani** a 640×360 (limite 30) con `allocs=0` per frame, e l'accettazione è dentro `check_3d_oracle.sh`. Il quarto blocco (2026-11-27) consegna i **raggi** (`SHOOT`/`USE`/`SEES`, 22 controlli headless e il MiniTest `scene_rays.bas` su quattro gambe). Restano LOD, instancing per materiale, `KEY` TRS, skinning 64 ossa e `SCENE.STATS`. La superficie `SCENE` resta agganciata e verde su quattro gambe.
