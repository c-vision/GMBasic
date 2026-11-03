' MiniTest PCode/4 gambe: GOMAXPROCS(n>0) seguito da un park (SHELLOUT) e da un generatore.
' Il ridimensionamento non deve far perdere il wake del task parcheggiato (worker ritirati).
' EXPECT: gomax>=0=-1|shell>0=-1|gen=1 2 3
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
  DIM s AS STRING
  DO
    TRY
      v = NEXT(it)
      s = s + LTRIM$(STR$(v)) + " "
    CATCH e AS ERR
      EXIT DO
    END TRY
  LOOP
  PRINT "gomax>=0="; (SYSTEM.GOMAXPROCS(4) >= 0)
  SET SHELL ON
  PRINT "shell>0="; (LEN(SHELLOUT("echo hi")) > 0)
  PRINT "gen="; RTRIM$(s)
END SUB
