REM FILE: HeatmapLab.bas — GFX2D + GFXCOLOR: campo scalare -> heatmap -> canvas -> PNG.
REM Scenario: una "collina" gaussiana 8x8 viene colorata con la colormap e scritta in un
REM canvas con CANVAS SET; il risultato si salva in PNG e si rilegge. Deterministico.
IMPORT GRAPHICS
IMPORT GFX2D
IMPORT GFXCOLOR

SUB Main()
  DIM z(63) AS DOUBLE, hm(63) AS DOUBLE
  DIM c AS LONG, x AS LONG, y AS LONG, col AS LONG, ok AS LONG

  ' collina: z = exp(-distanza^2 / 8)
  FOR y = 0 TO 7
    FOR x = 0 TO 7
      DIM dx AS DOUBLE, dy AS DOUBLE
      dx = x - 3.5 : dy = y - 3.5
      z(y * 8 + x) = EXP(-(dx * dx + dy * dy) / 8.0)
    NEXT x
  NEXT y

  PRINT "heat="; GFX2D.HEATMAP(z(), 8, 8, 0, 1, hm())

  CANVAS NEW 8, 8 INTO c
  FOR y = 0 TO 7
    FOR x = 0 TO 7
      col = hm(y * 8 + x)
      CANVAS SET c, x, y, col
    NEXT x
  NEXT y
  CANVAS GET c, 3, 3 INTO col
  PRINT "centro="; col
  CANVAS SAVE c, "heatmap.png" INTO ok
  PRINT "save="; ok
  CANVAS FREE c

  DIM o(2) AS DOUBLE
  PRINT "cmap="; GFXCOLOR.COLORMAP(0, o()); ":";
  PRINT o(0); ","; o(1); ","; o(2)

  ' helper immagine: normalizzazione, lettura pixel, clone e riempimento
  DIM zn(63) AS DOUBLE, zc(63) AS DOUBLE
  PRINT "norm="; GFX2D.NORMALIZE(z(), 8, 8, 0, 255, zn()); ":";
  PRINT GFX2D.IMGGET(zn(), 8, 8, 3, 3); ","; GFX2D.IMGGET(zn(), 8, 8, 9, 9)
  PRINT "clone="; GFX2D.IMGCLONE(zn(), 8, 8, zc()); ":";
  PRINT GFX2D.IMGGET(zc(), 8, 8, 0, 0)
  PRINT "fill="; GFX2D.IMGFILL(zc(), 8, 8, 7); ":"; zc(0); ","; zc(63)

  ' bitmap RGB8: scrittura e rilettura di un pixel
  DIM px(63) AS DOUBLE
  PRINT "bset="; GFX2D.BMPFILL(px(), 8, 8, 0); ":"; GFX2D.BMPGET(px(), 8, 8, 5, 5)
  PRINT "bput="; GFX2D.BMPSET(px(), 8, 8, 5, 5, 16711680); ":";
  PRINT GFX2D.BMPGET(px(), 8, 8, 5, 5)
END SUB
