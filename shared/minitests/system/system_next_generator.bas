' MiniTest SYSTEM.NEXT su un GENERATOR (YIELD): 1,2,3 poi fine iterazione (TRY/CATCH).
' EXPECT: 1|2|3|fine
IMPORT SYSTEM

GENERATOR RangeGen(n AS LONG) AS LONG
  DIM i AS LONG
  FOR i = 1 TO n
    YIELD i
  NEXT i
END GENERATOR

SUB Main()
  DIM it = RangeGen(3)
  DIM v AS LONG
  DIM out AS STRING
  DO
    TRY
      v = NEXT(it)
      out = out + LTRIM$(STR$(v)) + "|"
    CATCH e AS ERR
      EXIT DO
    END TRY
  LOOP
  PRINT out + "fine"
END SUB
