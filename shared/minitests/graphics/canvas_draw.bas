' EXPECT: pset= 1084926634  line= 1084882944  circle= 1073785344
' CANVAS + primitive: PSET/LINE/CIRCLE disegnano sul canvas attivo (CANVAS USE) e i pixel
' si rileggono con CANVAS GET. Verifica che il canvas sia il target del motore grafico
' condiviso (CGfx), non solo delle operazioni native SET/GET.
IMPORT GRAPHICS

SUB Main()
  SCREEN 12
  DIM c AS LONG, a AS LONG, b AS LONG, d AS LONG
  CANVAS NEW 16, 8 INTO c
  CANVAS USE c
  LINE (0, 0)-(15, 7), 4
  PSET (2, 2), 7
  CIRCLE (8, 4), 2, 2
  CANVAS GET c, 2, 2 INTO a
  CANVAS GET c, 0, 0 INTO b
  CANVAS GET c, 8, 2 INTO d
  CANVAS END
  SCREEN 0
  PRINT "pset="; a; " line="; b; " circle="; d
END SUB
