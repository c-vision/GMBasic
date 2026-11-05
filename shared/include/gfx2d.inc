' gfx2d.inc — namespace GFX2D via FFI (libgmbgraphics: gmbsuite/gmbgraphics).
' Immagini float 2D: array DOUBLE row-major di w*h valori. Le funzioni che producono una
' nuova immagine scrivono in out() (dimensionato w*h, o nw*nh per RESAMPLE). Ritorno 0 = ok.
NAMESPACE GFX2D

' --- filtri ---
' Convoluzione 2D con kernel kx*ky (ancorata al centro), bordo invariato.
DECLARE FUNCTION CONVOLVE LIB "gmbgraphics" ALIAS "ofx_gfx_img_convolve" (in() AS DOUBLE, w AS LONG, h AS LONG, k() AS DOUBLE, kx AS LONG, ky AS LONG, out() AS DOUBLE) AS LONG
' Convoluzione separabile: righe con kx(nx), colonne con ky(ny).
DECLARE FUNCTION CONVOLVESEP LIB "gmbgraphics" ALIAS "ofx_gfx_img_convolve_sep" (in() AS DOUBLE, w AS LONG, h AS LONG, kx() AS DOUBLE, nx AS LONG, ky() AS DOUBLE, ny AS LONG, out() AS DOUBLE) AS LONG
' Blur gaussiano (kernel costruito da sigma).
DECLARE FUNCTION GAUSSIAN LIB "gmbgraphics" ALIAS "ofx_gfx_img_gaussian" (in() AS DOUBLE, w AS LONG, h AS LONG, sigma AS DOUBLE, out() AS DOUBLE) AS LONG
' Gradiente di Sobel: gx() e gy() in uscita (w*h ciascuno).
DECLARE FUNCTION SOBEL LIB "gmbgraphics" ALIAS "ofx_gfx_img_sobel" (in() AS DOUBLE, w AS LONG, h AS LONG, gx() AS DOUBLE, gy() AS DOUBLE) AS LONG
' Mediana 3x3.
DECLARE FUNCTION MEDIAN3 LIB "gmbgraphics" ALIAS "ofx_gfx_img_median3" (in() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG

' --- trasformazioni ---
' Ricampionamento bilineare a nw x nh (out() = nw*nh).
DECLARE FUNCTION RESAMPLE LIB "gmbgraphics" ALIAS "ofx_gfx_img_resample" (in() AS DOUBLE, w AS LONG, h AS LONG, nw AS LONG, nh AS LONG, out() AS DOUBLE) AS LONG
' Trasposta (out() stessa dimensione, w*h valori; nuove dim = h x w).
DECLARE FUNCTION TRANSPOSE LIB "gmbgraphics" ALIAS "ofx_gfx_img_transpose" (in() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG
' Rotazione di k*90° antiorario (k mod 4); stessi w*h valori, dim scambiate se k dispari.
DECLARE FUNCTION ROTATE90 LIB "gmbgraphics" ALIAS "ofx_gfx_img_rotate90" (in() AS DOUBLE, w AS LONG, h AS LONG, k AS LONG, out() AS DOUBLE) AS LONG
' Rotazione arbitraria attorno al centro (bilineare, bg fuori immagine).
DECLARE FUNCTION ROTATE LIB "gmbgraphics" ALIAS "ofx_gfx_img_rotate" (in() AS DOUBLE, w AS LONG, h AS LONG, radians AS DOUBLE, bg AS DOUBLE, out() AS DOUBLE) AS LONG
' Flip orizzontale / verticale.
DECLARE FUNCTION FLIPH LIB "gmbgraphics" ALIAS "ofx_gfx_img_flip_h" (in() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG
DECLARE FUNCTION FLIPV LIB "gmbgraphics" ALIAS "ofx_gfx_img_flip_v" (in() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG

' --- punti / istogrammi ---
' Soglia: sotto t -> lo, sopra -> hi.
DECLARE FUNCTION THRESHOLD LIB "gmbgraphics" ALIAS "ofx_gfx_img_threshold" (in() AS DOUBLE, w AS LONG, h AS LONG, t AS DOUBLE, lo AS DOUBLE, hi AS DOUBLE, out() AS DOUBLE) AS LONG
' Normalizza min..max in lo..hi.
DECLARE FUNCTION NORMALIZE LIB "gmbgraphics" ALIAS "ofx_gfx_img_normalize" (in() AS DOUBLE, w AS LONG, h AS LONG, lo AS DOUBLE, hi AS DOUBLE, out() AS DOUBLE) AS LONG
' Istogramma a nbins classi in [lo,hi]: counts() ha nbins elementi.
DECLARE FUNCTION HISTOGRAM LIB "gmbgraphics" ALIAS "ofx_gfx_img_histogram" (in() AS DOUBLE, w AS LONG, h AS LONG, nbins AS LONG, lo AS DOUBLE, hi AS DOUBLE, counts() AS DOUBLE) AS LONG
' Equalizzazione dell'istogramma.
DECLARE FUNCTION HISTEQUALIZE LIB "gmbgraphics" ALIAS "ofx_gfx_img_hist_equalize" (in() AS DOUBLE, w AS LONG, h AS LONG, nbins AS LONG, out() AS DOUBLE) AS LONG

' --- disegno (in-place sull'array) ---
DECLARE FUNCTION DRAWLINE LIB "gmbgraphics" ALIAS "ofx_gfx_img_draw_line" (d() AS DOUBLE, w AS LONG, h AS LONG, x0 AS LONG, y0 AS LONG, x1 AS LONG, y1 AS LONG, v AS DOUBLE) AS LONG
DECLARE FUNCTION DRAWRECTF LIB "gmbgraphics" ALIAS "ofx_gfx_img_draw_rect_fill" (d() AS DOUBLE, w AS LONG, h AS LONG, x0 AS LONG, y0 AS LONG, x1 AS LONG, y1 AS LONG, v AS DOUBLE) AS LONG
DECLARE FUNCTION DRAWRECT LIB "gmbgraphics" ALIAS "ofx_gfx_img_draw_rect_stroke" (d() AS DOUBLE, w AS LONG, h AS LONG, x0 AS LONG, y0 AS LONG, x1 AS LONG, y1 AS LONG, v AS DOUBLE) AS LONG
DECLARE FUNCTION DRAWCIRCLE LIB "gmbgraphics" ALIAS "ofx_gfx_img_draw_circle" (d() AS DOUBLE, w AS LONG, h AS LONG, cx AS LONG, cy AS LONG, r AS LONG, v AS DOUBLE, fill AS LONG) AS LONG

' --- avanzate ---
' Warping all'indietro: in() = c*h*w (CHW), fx()/fy() = h*w offset, out() = c*h*w.
DECLARE FUNCTION WARP LIB "gmbgraphics" ALIAS "ofx_gfx_img_warp" (in() AS DOUBLE, c AS LONG, h AS LONG, w AS LONG, fx() AS DOUBLE, fy() AS DOUBLE, out() AS DOUBLE) AS LONG
' Poisson/seamless clone: mask() = double 0/1 (nx*ny), out() = nx*ny.
DECLARE FUNCTION POISSON LIB "gmbgraphics" ALIAS "ofx_gfx_img_poisson" (target() AS DOUBLE, source() AS DOUBLE, mask() AS DOUBLE, nx AS LONG, ny AS LONG, out() AS DOUBLE, maxit AS LONG) AS LONG
