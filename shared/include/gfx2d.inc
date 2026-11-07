' gfx2d.inc — namespace GFX2D via FFI (libgmbgraphics: gmbsuite/gmbgraphics).
' Immagini float 2D: array DOUBLE row-major di w*h valori. Le funzioni che producono una
' nuova immagine scrivono in out() (dimensionato w*h, o nw*nh per RESAMPLE). Ritorno 0 = ok.
NAMESPACE GFX2D

' --- filtri ---
' Convoluzione 2D con kernel kx*ky (ancorata al centro), bordo invariato.
DECLARE NATIVE FUNCTION CONVOLVE LIB "gmbgraphics" ALIAS "ofx_gfx_img_convolve__nat" (in() AS DOUBLE, w AS LONG, h AS LONG, k() AS DOUBLE, kx AS LONG, ky AS LONG, out() AS DOUBLE) AS LONG
' Convoluzione separabile: righe con kx(nx), colonne con ky(ny).
DECLARE NATIVE FUNCTION CONVOLVESEP LIB "gmbgraphics" ALIAS "ofx_gfx_img_convolve_sep__nat" (in() AS DOUBLE, w AS LONG, h AS LONG, kx() AS DOUBLE, nx AS LONG, ky() AS DOUBLE, ny AS LONG, out() AS DOUBLE) AS LONG
' Blur gaussiano (kernel costruito da sigma).
DECLARE NATIVE FUNCTION GAUSSIAN LIB "gmbgraphics" ALIAS "ofx_gfx_img_gaussian__nat" (in() AS DOUBLE, w AS LONG, h AS LONG, sigma AS DOUBLE, out() AS DOUBLE) AS LONG
' Gradiente di Sobel: gx() e gy() in uscita (w*h ciascuno).
DECLARE NATIVE FUNCTION SOBEL LIB "gmbgraphics" ALIAS "ofx_gfx_img_sobel__nat" (in() AS DOUBLE, w AS LONG, h AS LONG, gx() AS DOUBLE, gy() AS DOUBLE) AS LONG
' Mediana 3x3.
DECLARE NATIVE FUNCTION MEDIAN3 LIB "gmbgraphics" ALIAS "ofx_gfx_img_median3__nat" (in() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG

' --- trasformazioni ---
' Ricampionamento bilineare a nw x nh (out() = nw*nh).
DECLARE NATIVE FUNCTION RESAMPLE LIB "gmbgraphics" ALIAS "ofx_gfx_img_resample__nat" (in() AS DOUBLE, w AS LONG, h AS LONG, nw AS LONG, nh AS LONG, out() AS DOUBLE) AS LONG
' Trasposta (out() stessa dimensione, w*h valori; nuove dim = h x w).
DECLARE NATIVE FUNCTION TRANSPOSE LIB "gmbgraphics" ALIAS "ofx_gfx_img_transpose__nat" (in() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG
' Rotazione di k*90° antiorario (k mod 4); stessi w*h valori, dim scambiate se k dispari.
DECLARE NATIVE FUNCTION ROTATE90 LIB "gmbgraphics" ALIAS "ofx_gfx_img_rotate90__nat" (in() AS DOUBLE, w AS LONG, h AS LONG, k AS LONG, out() AS DOUBLE) AS LONG
' Rotazione arbitraria attorno al centro (bilineare, bg fuori immagine).
DECLARE NATIVE FUNCTION ROTATE LIB "gmbgraphics" ALIAS "ofx_gfx_img_rotate__nat" (in() AS DOUBLE, w AS LONG, h AS LONG, radians AS DOUBLE, bg AS DOUBLE, out() AS DOUBLE) AS LONG
' Flip orizzontale / verticale.
DECLARE NATIVE FUNCTION FLIPH LIB "gmbgraphics" ALIAS "ofx_gfx_img_flip_h__nat" (in() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION FLIPV LIB "gmbgraphics" ALIAS "ofx_gfx_img_flip_v__nat" (in() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG

' --- punti / istogrammi ---
' Soglia: sotto t -> lo, sopra -> hi.
DECLARE NATIVE FUNCTION THRESHOLD LIB "gmbgraphics" ALIAS "ofx_gfx_img_threshold__nat" (in() AS DOUBLE, w AS LONG, h AS LONG, t AS DOUBLE, lo AS DOUBLE, hi AS DOUBLE, out() AS DOUBLE) AS LONG
' Normalizza min..max in lo..hi.
DECLARE NATIVE FUNCTION NORMALIZE LIB "gmbgraphics" ALIAS "ofx_gfx_img_normalize__nat" (in() AS DOUBLE, w AS LONG, h AS LONG, lo AS DOUBLE, hi AS DOUBLE, out() AS DOUBLE) AS LONG
' Istogramma a nbins classi in [lo,hi]: counts() ha nbins elementi.
DECLARE NATIVE FUNCTION HISTOGRAM LIB "gmbgraphics" ALIAS "ofx_gfx_img_histogram__nat" (in() AS DOUBLE, w AS LONG, h AS LONG, nbins AS LONG, lo AS DOUBLE, hi AS DOUBLE, counts() AS DOUBLE) AS LONG
' Equalizzazione dell'istogramma.
DECLARE NATIVE FUNCTION HISTEQUALIZE LIB "gmbgraphics" ALIAS "ofx_gfx_img_hist_equalize__nat" (in() AS DOUBLE, w AS LONG, h AS LONG, nbins AS LONG, out() AS DOUBLE) AS LONG

' --- disegno (in-place sull'array) ---
DECLARE NATIVE FUNCTION DRAWLINE LIB "gmbgraphics" ALIAS "ofx_gfx_img_draw_line__nat" (d() AS DOUBLE, w AS LONG, h AS LONG, x0 AS LONG, y0 AS LONG, x1 AS LONG, y1 AS LONG, v AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION DRAWRECTF LIB "gmbgraphics" ALIAS "ofx_gfx_img_draw_rect_fill__nat" (d() AS DOUBLE, w AS LONG, h AS LONG, x0 AS LONG, y0 AS LONG, x1 AS LONG, y1 AS LONG, v AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION DRAWRECT LIB "gmbgraphics" ALIAS "ofx_gfx_img_draw_rect_stroke__nat" (d() AS DOUBLE, w AS LONG, h AS LONG, x0 AS LONG, y0 AS LONG, x1 AS LONG, y1 AS LONG, v AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION DRAWCIRCLE LIB "gmbgraphics" ALIAS "ofx_gfx_img_draw_circle__nat" (d() AS DOUBLE, w AS LONG, h AS LONG, cx AS LONG, cy AS LONG, r AS LONG, v AS DOUBLE, fill AS LONG) AS LONG

' --- avanzate ---
' Warping all'indietro: in() = c*h*w (CHW), fx()/fy() = h*w offset, out() = c*h*w.
DECLARE NATIVE FUNCTION WARP LIB "gmbgraphics" ALIAS "ofx_gfx_img_warp__nat" (in() AS DOUBLE, c AS LONG, h AS LONG, w AS LONG, fx() AS DOUBLE, fy() AS DOUBLE, out() AS DOUBLE) AS LONG
' Poisson/seamless clone: mask() = double 0/1 (nx*ny), out() = nx*ny.
DECLARE NATIVE FUNCTION POISSON LIB "gmbgraphics" ALIAS "ofx_gfx_img_poisson__nat" (target() AS DOUBLE, source() AS DOUBLE, mask() AS DOUBLE, nx AS LONG, ny AS LONG, out() AS DOUBLE, maxit AS LONG) AS LONG

' --- volume 3D e superfici ---
' Convoluzione separabile su volume w*h*d (stesso kernel 1D sui tre assi).
DECLARE NATIVE FUNCTION IMG3CONVOLVE LIB "gmbgraphics" ALIAS "ofx_gfx_img3_convolve_sep__nat" (in() AS DOUBLE, w AS LONG, h AS LONG, d AS LONG, k() AS DOUBLE, n AS LONG, out() AS DOUBLE) AS LONG
' Heatmap da griglia z(nx*ny): out() = nx*ny colori impacchettati (0x40000000|RGB), pronti per CANVAS SET.
DECLARE NATIVE FUNCTION HEATMAP LIB "gmbgraphics" ALIAS "ofx_gfx_heatmap__nat" (z() AS DOUBLE, nx AS LONG, ny AS LONG, zmin AS DOUBLE, zmax AS DOUBLE, out() AS DOUBLE) AS LONG

' ── statistiche e combinazioni (immagini float) ────────────────────────────
DECLARE NATIVE FUNCTION IMGMIN LIB "gmbgraphics" ALIAS "ofx_gfx_img_min__nat" (in() AS DOUBLE, w AS LONG, h AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION IMGMAX LIB "gmbgraphics" ALIAS "ofx_gfx_img_max__nat" (in() AS DOUBLE, w AS LONG, h AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION IMGABSMAX LIB "gmbgraphics" ALIAS "ofx_gfx_img_absmax__nat" (in() AS DOUBLE, w AS LONG, h AS LONG) AS DOUBLE
' Scala in-place: out = d * s.
DECLARE NATIVE FUNCTION IMGSCALE LIB "gmbgraphics" ALIAS "ofx_gfx_img_scale__nat" (d() AS DOUBLE, w AS LONG, h AS LONG, s AS DOUBLE) AS LONG
' Differenza: out = a - b.
DECLARE NATIVE FUNCTION IMGSUB LIB "gmbgraphics" ALIAS "ofx_gfx_img_sub__nat" (a() AS DOUBLE, b() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG

' ── accesso puntuale e helper ──────────────────────────────────────────────
' Valore del pixel (x,y); coordinate fuori range -> 0.
DECLARE NATIVE FUNCTION IMGGET LIB "gmbgraphics" ALIAS "ofx_gfx_img_get__nat" (in() AS DOUBLE, w AS LONG, h AS LONG, x AS LONG, y AS LONG) AS DOUBLE
' Scrittura di un pixel e riempimento dell'intera immagine (in-place).
DECLARE NATIVE FUNCTION IMGSET LIB "gmbgraphics" ALIAS "ofx_gfx_img_set__nat" (d() AS DOUBLE, w AS LONG, h AS LONG, x AS LONG, y AS LONG, v AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION IMGFILL LIB "gmbgraphics" ALIAS "ofx_gfx_img_fill__nat" (d() AS DOUBLE, w AS LONG, h AS LONG, v AS DOUBLE) AS LONG
' Copia indipendente: out() = clone di in() (w*h).
DECLARE NATIVE FUNCTION IMGCLONE LIB "gmbgraphics" ALIAS "ofx_gfx_img_clone__nat" (in() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG

' Volume 3D w*h*d (layout x + w*(y + h*z)): pixel e riempimento.
DECLARE NATIVE FUNCTION IMG3GET LIB "gmbgraphics" ALIAS "ofx_gfx_img3_get__nat" (in() AS DOUBLE, w AS LONG, h AS LONG, d AS LONG, x AS LONG, y AS LONG, z AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION IMG3SET LIB "gmbgraphics" ALIAS "ofx_gfx_img3_set__nat" (v() AS DOUBLE, w AS LONG, h AS LONG, d AS LONG, x AS LONG, y AS LONG, z AS LONG, val AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION IMG3FILL LIB "gmbgraphics" ALIAS "ofx_gfx_img3_fill__nat" (v() AS DOUBLE, w AS LONG, h AS LONG, d AS LONG, val AS DOUBLE) AS LONG

' Triangolo (pieno o contorno) su immagine float, in-place.
' Triangolo (pieno o contorno): vertici in array [x,y] (a(),b(),c()), in-place.
DECLARE NATIVE FUNCTION DRAWTRIANGLE LIB "gmbgraphics" ALIAS "ofx_gfx_img_draw_triangle__nat" (d() AS DOUBLE, w AS LONG, h AS LONG, a() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, v AS DOUBLE, fill AS LONG) AS LONG

' ── bitmap RGB8 (pixel = colori impacchettati 0xRRGGBB) ────────────────────
' Grayscale naive (media) e a luminanza; out() = w*h colori impacchettati.
DECLARE NATIVE FUNCTION GRAYNAIVE LIB "gmbgraphics" ALIAS "ofx_gfx_bmp_gray_naive__nat" (px() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION GRAYLUM LIB "gmbgraphics" ALIAS "ofx_gfx_bmp_gray_lum__nat" (px() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG
' Convoluzione 3x3 per canale (kernel 9 valori, divider); bordo invariato.
DECLARE NATIVE FUNCTION CONVOLVE3 LIB "gmbgraphics" ALIAS "ofx_gfx_bmp_convolve3__nat" (px() AS DOUBLE, w AS LONG, h AS LONG, k() AS DOUBLE, divider AS DOUBLE, out() AS DOUBLE) AS LONG
' Luminosità/contrasto: out = clamp((in-128)*contrast + 128 + brightness).
DECLARE NATIVE FUNCTION BRIGHTNESS LIB "gmbgraphics" ALIAS "ofx_gfx_bmp_brightness__nat" (px() AS DOUBLE, w AS LONG, h AS LONG, brightness AS DOUBLE, contrast AS DOUBLE, out() AS DOUBLE) AS LONG
' Luminanza RGB8 -> immagine float out() (w*h).
DECLARE NATIVE FUNCTION TOIMAGE LIB "gmbgraphics" ALIAS "ofx_gfx_bmp_to_image__nat" (px() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG
' Pixel RGB8 (packed 0xRRGGBB): lettura (fuori range -> 0), scrittura, riempimento.
DECLARE NATIVE FUNCTION BMPGET LIB "gmbgraphics" ALIAS "ofx_gfx_bmp_get__nat" (px() AS DOUBLE, w AS LONG, h AS LONG, x AS LONG, y AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION BMPSET LIB "gmbgraphics" ALIAS "ofx_gfx_bmp_set__nat" (px() AS DOUBLE, w AS LONG, h AS LONG, x AS LONG, y AS LONG, color AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION BMPFILL LIB "gmbgraphics" ALIAS "ofx_gfx_bmp_fill__nat" (px() AS DOUBLE, w AS LONG, h AS LONG, color AS DOUBLE) AS LONG
