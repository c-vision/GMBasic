' FILE: Concurrency/GpmV2GraphicsNested.bas
' =================================================================================================
'       G6 (piano ILBackend-GPM): park grafici in contesti annidati (call chain, IF, loop, TRY)
' =================================================================================================
'
' LOAD TEXTURE dentro: un loop (FOR), un IF, un TRY/FINALLY e una SUB callee (call chain).
' Il park deve essere sospendibile e riprendere correttamente in ogni contesto (headless).
'
' Output atteso (headless, LOAD TEXTURE = 0):
'   L-0
'   T-0
'   FIN
'   L-0
'   T-0
'   FIN
'   MAIN-DONE

SUB LoadAndReport()
  DIM t AS LONG
  LOAD TEXTURE "a.png" INTO t
  PRINT "L-"; t
END SUB

SUB Main()
  SCREEN 0
  SYNC
  FOR i = 1 TO 2
    IF i = 1 THEN
      TRY
        DIM t2 AS LONG
        LOAD TEXTURE "b.png" INTO t2
        PRINT "T-"; t2
      FINALLY
        PRINT "FIN"
      END TRY
    ELSE
      CALL LoadAndReport()
    END IF
  NEXT i
  PRINT "MAIN-DONE"
END SUB
