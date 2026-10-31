# SDL.md — confronto gmbmedia vs SDL_image (SDL2_image)

Documento che raccoglie i **risultati** del confronto tra il decodificatore di
`gmbmedia` e **SDL_image** (che a sua volta usa libjpeg-turbo / libpng), e **come
sono stati eseguiti** i test. Serve a: (1) misurare la velocità di decodifica;
(2) verificare la **fedeltà pixel-per-pixel** rispetto a un riferimento esterno.

> Nota: i numeri assoluti dipendono dal carico della macchina (condivisa). Fai
> sempre riferimento al **rapporto `sdl/gmb`** misurato nella **stessa**
> esecuzione, non al valore assoluto.

---

## 1. Metodo

Tre strumenti, tutti auto-contenuti in `gmbmedia/tools/`:

| Tool | Cosa fa |
|---|---|
| `tools/sdl_bench.c` | misura la velocità: `mm_image_read`+`free` vs `IMG_Load`+`SDL_FreeSurface` |
| `tools/sdl_diff.c`  | confronta i **pixel** (canali RGB): `pixel>8diff` + `maxdiff` |
| `test/run_tests.c` (sezione "GOLDEN") | confronta il decode col **golden** generato da SDL (rete di sicurezza in CI) |

### 1.1 Velocità (`sdl_bench`)

- **Warmup** (1 chiamata non misurata), poi **N iterazioni**.
- Timer: `clock_gettime(CLOCK_MONOTONIC)`; si riporta il **tempo medio per
  decodifica** in microsecondi e il **rapporto `sdl/gmb`**.
- Entrambi leggono da file (`mm_image_read` vs `IMG_Load`).

```sh
# dalla cartella gmbmedia/ (con out/libgmbmedia.a appena costruita)
cc -O2 -Iinclude tools/sdl_bench.c out/libgmbmedia.a \
   -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lm -ldl -o /tmp/sdl_bench
DYLD_LIBRARY_PATH=/opt/homebrew/lib /tmp/sdl_bench <file> 200
```

### 1.2 Fedeltà (`sdl_diff`)

- Decodifica lo **stesso file** con gmbmedia e con SDL_image.
- Per ogni pixel confronta i canali **R,G,B**; riporta il numero di pixel con
  differenza **> 8** e la **differenza massima**.
- `maxdiff` **0–1** = fedele (solo arrotondamenti); **≥ 8** = divergenza reale.

```sh
cc -O2 -Iinclude tools/sdl_diff.c out/libgmbmedia.a \
   -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lm -ldl -o /tmp/sdl_diff
DYLD_LIBRARY_PATH=/opt/homebrew/lib /tmp/sdl_diff <file> [...]
```

### 1.3 Golden test (rete di sicurezza, in `test/run_tests.c`)

In `test/fixtures/`:
- `golden420.jpg` — una fixture JPEG (4:2:0);
- `golden420.raw` — il suo decode **raw RGB generato da SDL_image** (la "verità").

Il test decodifica la fixture con gmbmedia e la confronta col golden: deve
combaciare entro `maxdiff <= 4`. **Verificato**: rompendo l'upsampling (tornando a
"nearest") il test fallisce → una regressione del genere viene catturata subito.

---

## 2. Risultati (esecuzione rappresentativa)

Unità: µs per decodifica. `ratio = sdl/gmb` (>1 = gmbmedia più veloce).

| File | Formato | gmbmedia | SDL_image | ratio | maxdiff |
|---|---|---|---|---|---|
| `jpg1024.jpg` (1024²) | JPEG 4:2:0 | 1923 µs | 1285 µs | 0.67 | **1** |
| `big.jpg` (4000²) | JPEG 4:2:0 | 26627 µs | 18104 µs | 0.68 | **1** |
| `v444.jpg` (1024²) | JPEG 4:4:4 | 3437 µs | 2897 µs | **0.84** | **1** |
| `v422.jpg` (1024²) | JPEG 4:2:2 | 1168 µs | 744 µs | 0.64 | **1** |
| `sprite256.png` (256²) | PNG | 1086 µs | 651 µs | 0.60 | **0** |
| `sprite_rgb.png` (1024²) | PNG RGB | 16266 µs | 8215 µs | 0.51 | **0** |
| `sprite2048.png` (2048²) | PNG RGB | 46299 µs | 24351 µs | 0.53 | **0** |
| `rgba2048.png` (2048²) | PNG RGBA (alta entropia) | 96169 µs | 37966 µs | 0.39 | **0** |

### Lettura

- **Fedeltà**: JPEG **maxdiff 1**, PNG **maxdiff 0** → gmbmedia decodifica in modo
  **bit-fedele** al riferimento (solo arrotondamenti). Catturato dal golden test.
- **Velocità JPEG**: **0.65–0.85** del tempo di libjpeg-turbo (4:4:4 quasi a parità).
  Lo scarto residuo è nei confronti di un decoder con kernel SIMD scritti a mano;
  le ottimizzazioni applicate (IDCT islow NEON, dequant NEON, upsampling fancy NEON,
  bit-reader veloce, colore NEON) hanno ridotto il divario iniziale (~24×).
- **Velocità PNG**: **0.39–0.60**. Guadagnata con l'**Huffman DEFLATE a lookahead**
  (tabella 9 bit; bit-order LSB-first come da RFC1951) nel `inflate` e con la
  conversione d'uscita 8-bit via `memcpy` (era un loop byte-per-byte). Il collo
  residuo è il filtro **Paeth** (≈92% delle righe nei PNG tipici): ha una
  **dipendenza intra-riga** (`a = d[x-fbpp]` usa gli output appena calcolati). Un
  Paeth **NEON a 4 lane** (una per canale, `a` = pixel precedente) è stato provato:
  è corretto concettualmente ma in pratica la catena è **latency-bound** e dà solo
  **~4%** — è stato **rimosso** (non vale la complessità/rischio). Il supporto
  1/2/4-bit è presente e corretto (palette+grigio).

---

## 3. Copertura formati (gmbmedia)

`png, gif, jpg/jpeg, bmp, tga, tif/tiff, webp, ppm/pgm/pnm, qoi` (+ WAV audio).
WebP richiede `libgmbwebp.dylib/.so` (bundle: `libs/webp/`, `Resources/libs/<rid>/`);
il loader risale la CWD per trovarlo.

## 4. Riproduzione rapida

```sh
cd gmbmedia
make all
# velocità
cc -O2 -Iinclude tools/sdl_bench.c out/libgmbmedia.a -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lm -ldl -o /tmp/sdl_bench
# fedeltà
cc -O2 -Iinclude tools/sdl_diff.c  out/libgmbmedia.a -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lm -ldl -o /tmp/sdl_diff
# golden (fa parte della suite)
./out/run_tests
```

Prerequisiti SDL2/SDL2_image: `/opt/homebrew` (solo per i test esterni; gmbmedia
non li linka).
