# libgmbgraphics (FFI) — grafica computazionale

Sorgenti venduti da `gmbsuite/gmbgraphics` (colore e geometria 2D; gli altri moduli si
aggiungono per dominio). Build: `bash build.sh` → `libgmbgraphics.dylib`.

Export BASIC (in `shared/include/`):

- `gfxcolor.inc` → namespace `GFXCOLOR` (conversioni RGB/HSV/HSL/CMYK, mix, lighten/darken/saturate)
- `gfxshape.inc` → namespace `GFXSHAPE` (point-in-polygon/circle/rect/triangle, area/centroide,
  closest point su segmento)

## Confine con il linguaggio

Nel linguaggio restano **solo** il motore grafico e le sue primitive (schermo/pagine,
PSET/LINE/CIRCLE/PAINT/POLYGON/DRAW, testo, palette, sprite, `CANVAS` come target di disegno).
Colore, hit-test e ogni altro algoritmo sono **qui**, non builtin: una sola implementazione
(`gmbgraphics`), distribuita come libreria FFI.

## Convenzioni

- colori in componenti `0..255` (H in `[0,360)`, S/V/L in `[0,1]`);
- poligoni: array piatto `x0,y0,…`, `n` = numero di **valori** (pari);
- predicati `1`/`0`; funzioni con `out()` ritornano `0` = ok, `-1` = errore.
