' MiniTest: GREP.MATCHES/COUNT/FILES — ricerca file, ritorno ARRAY (parita' 4 backend).
' EXPECT: 2|2|1|-1
IMPORT GREP
SUB Main
  DIM f AS STRING
  f = PROJECTFOLDER() + "/data.txt"
  PRINT GREP.COUNT("needle", f)
  DIM m() AS STRING
  m = GREP.MATCHES("needle", f)
  PRINT UBOUND(m) + 1
  DIM fl() AS STRING
  fl = GREP.FILES("needle", f)
  PRINT UBOUND(fl) + 1
  PRINT INSTR(m[0], "needle") > 0
END SUB
