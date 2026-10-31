' MiniTest: GREP.MATCHES(pattern, path) -> ARRAY di stringhe "file:riga:col:testo".
' EXPECT: 2|-1|-1
IMPORT GREP
SUB Main
  DIM m() AS STRING
  m = GREP.MATCHES("needle", PROJECTFOLDER() + "/data.txt")
  PRINT UBOUND(m) + 1
  PRINT INSTR(m[0], "needle") > 0
  PRINT INSTR(m[1], "needle") > 0
END SUB
