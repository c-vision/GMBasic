' MiniTest: GREP.COUNT(pattern, path) -> LONG (numero di hit).
' EXPECT: 2|0
IMPORT GREP
SUB Main
  PRINT GREP.COUNT("needle", PROJECTFOLDER() + "/data.txt")
  PRINT GREP.COUNT("assente_xyz", PROJECTFOLDER() + "/data.txt")
END SUB
