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
END SUB
