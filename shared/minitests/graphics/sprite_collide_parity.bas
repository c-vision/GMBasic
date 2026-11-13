' MiniTest: COLLIDE(id1, id2) — collisione AABB fra due SPRITE (dimensioni dalla texture).
' Headless: il motore software decodifica il PNG anche senza finestra, quindi il rettangolo
' dello sprite è quello REALE (non 0×0) e la collisione è calcolabile su tutte e 4 le gambe.
' Prima: C99/VM-C non decodificavano la texture in null mode (COLLIDE sempre 0), IL aveva gli
' stub in NullGraphicsService (=> 0), PCode invertiva il segno (-1 trattato come "no"), VM/C
' non implementava affatto il builtin 460. Stesso programma, quattro bug diversi.
' EXPECT: lontani= 0|sovrapposti=-1|spento= 0
IMPORT GRAPHICS

SUB Main()
  DIM dir AS STRING
  dir = PROJECTFOLDER() + "/../../samples/Fixtures"

  SCREEN 12
  DIM a AS LONG
  DIM b AS LONG
  SPRITE LOAD dir + "/sprite_dot.png", a
  SPRITE LOAD dir + "/sprite_dot.png", b

  DIM r1 AS INTEGER
  DIM r2 AS INTEGER
  DIM r3 AS INTEGER

  ' 1) lontani
  SPRITE SET POSITION a, 10, 10
  SPRITE SET POSITION b, 300, 200
  SYNC
  r1 = COLLIDE(a, b)

  ' 2) stessa posizione: collisione certa qualunque sia la dimensione della texture
  SPRITE SET POSITION b, 10, 10
  SYNC
  r2 = COLLIDE(a, b)

  ' 3) sprite spento (SPRITE OFF): non partecipa alle collisioni
  SPRITE OFF b
  SYNC
  r3 = COLLIDE(a, b)

  SCREEN 0
  PRINT "lontani="; r1
  PRINT "sovrapposti="; r2
  PRINT "spento="; r3
END SUB
