' MiniTest: SYSTEM.UDTPACKED(udt) = numero di campi — parita' 4 backend.
' EXPECT: 2
IMPORT SYSTEM
TYPE TestUDT
  a AS INTEGER
  b AS DOUBLE
END TYPE
SUB Main()
  DIM u AS TestUDT
  PRINT SYSTEM.UDTPACKED(u)
END SUB
