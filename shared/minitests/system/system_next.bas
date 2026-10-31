' EXPECT: next= 55
GENERATOR RangeGen(n AS LONG) AS LONG
  DIM i AS LONG
  FOR i = 1 TO n
    YIELD i
  NEXT i
END GENERATOR
SUB Main()
  DIM it = RangeGen(10)
  DIM total AS LONG = 0
  DIM value AS LONG
  DO
    TRY
      value = NEXT(it)
      total = total + value
    CATCH e AS ERR WHEN e.Number = 144
      EXIT DO
    END TRY
  LOOP
  PRINT "next="; total
END SUB