IMPORT LLM
DIM g_ids(200000) AS LONG
FUNCTION ParseIds(s AS STRING, a() AS LONG, cap AS LONG) AS LONG
  DIM i AS LONG, k AS LONG
  k = 0
  FOR i = 1 TO LEN(s)
    IF ASC(MID$(s, i, 1)) = 44 THEN k = k + 1
  NEXT i
  a(0) = k
  ParseIds = k
END FUNCTION
SUB Main()
  DIM csv AS STRING
  csv = "1,2,3,4,5"
  DIM n AS LONG
  n = ParseIds(csv, g_ids(), 200000)
  PRINT "n=" + LTRIM$(STR$(n))
END SUB
