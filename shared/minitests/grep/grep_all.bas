' MiniTest NAMESPACE GREP — tutte le funzioni della superficie.
' EXPECT: 2|0|2|-1|-1|1|-1
IMPORT GREP

SUB T0()
  PRINT GREP.COUNT("needle", PROJECTFOLDER() + "/data.txt")
  PRINT GREP.COUNT("assente_xyz", PROJECTFOLDER() + "/data.txt")
END SUB

SUB T1()
  DIM m() AS STRING
  m = GREP.MATCHES("needle", PROJECTFOLDER() + "/data.txt")
  PRINT UBOUND(m) + 1
  PRINT INSTR(m[0], "needle") > 0
  PRINT INSTR(m[1], "needle") > 0
END SUB

SUB T2()
  DIM f() AS STRING
  f = GREP.FILES("needle", PROJECTFOLDER() + "/data.txt")
  PRINT UBOUND(f) + 1
  PRINT INSTR(f[0], "data.txt") > 0
END SUB

SUB Main()
  T0()
  T1()
  T2()
END SUB
