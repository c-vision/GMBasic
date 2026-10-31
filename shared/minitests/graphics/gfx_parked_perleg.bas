' TAG: interactive
' MiniTest: comandi grafici senza-namespace che APPENDONO l'harness in-process (park
' grafico/modal su SYNC): SCREENSET/PCOPY/LINE/CIRCLE/PAINT/DRAW/FONT/BSAVE/TILEMAP/
' POLYGON/SPRITEDEF/IMAGEGET/IMAGEPUT/IMAGEPUTS. Escluso dal cross-leg
' (`IsParityExcluded`), verificato per-gamba via CLI (exit 0). Copre i comandi
' senza-ns del census (§0.51/§0.53).
' EXPECT: gfx-parked-ok
SUB Main()
  DIM buf AS LONG
  DIM pts(6)
  DIM im AS STRING
  DIM sp AS STRING
  DIM pos AS STRING
  SCREENSET 0, 0
  PCOPY 0, 1
  PRESET (11, 11)
  LINE (0, 0)-(5, 5), 4
  CIRCLE (3, 3), 2, 1
  PAINT (1, 1), 2
  DRAW "R5D5L5U5"
  FONT "Arial", 12
  SOUND 440, 100
  PLAY "C D E"
  BSAVE (0, 0)-(5, 5), buf
  TILEMAP SET 1, 2, 3
  TILEMAP OFFSET 1, 2
  POLYGON pts, 3
  SPRITEDEF START 1
    "XX"
    "XX"
  SPRITEDEF END
  IMAGEGET (0, 0)-(5, 5), im
  IMAGEPUT (0, 0), im
  IMAGEPUTS sp, pos, 2
  PRINT "gfx-parked-ok"
END SUB