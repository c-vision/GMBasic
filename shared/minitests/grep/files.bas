' MiniTest: GREP.FILES(pattern, path) -> ARRAY di nomi file (distinti).
' EXPECT: 1|-1
IMPORT GREP
SUB Main
  DIM f() AS STRING
  f = GREP.FILES("needle", PROJECTFOLDER() + "/data.txt")
  PRINT UBOUND(f) + 1
  PRINT INSTR(f[0], "data.txt") > 0
END SUB
