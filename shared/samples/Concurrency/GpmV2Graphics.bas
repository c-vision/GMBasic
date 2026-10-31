' FILE: Concurrency/GpmV2Graphics.bas
' =================================================================================================
'       G6 (piano ILBackend-GPM): park grafici sotto la GPM v2 (headless)
' =================================================================================================
'
' SCREEN/SYNC/POINT/LOAD TEXTURE come park grafici (Try*Park del flat). In headless
' (NullGraphicsService) SCREEN/SYNC completano subito, POINT ritorna 0, LOAD TEXTURE -1.
'
' Output atteso:
'   P-0
'   T-0
'   S-OK
'   G-OK
'   MAIN-DONE

SUB Main()
  SCREEN 0
  SYNC
  DIM c AS LONG
  POINT(1, 2), c
  PRINT "P-"; c
  DIM t AS LONG
  LOAD TEXTURE "nonexistent.png" INTO t
  PRINT "T-"; t
  DIM sheet AS LONG
  LOAD TEXTURE "s.png" INTO sheet CELL(16, 16)
  PRINT "S-OK"
  DIM px AS LONG
  SPRITE GETPIXELS 1, px
  PRINT "G-OK"
  PRINT "MAIN-DONE"
END SUB
