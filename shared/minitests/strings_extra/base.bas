IMPORT STRINGS
SUB Main()
  PRINT LEN("ciao")
  PRINT LEFT("GMBasic", 2)
  PRINT RIGHT("GMBasic", 5)
  PRINT MID("GMBasic", 3, 4)
  PRINT MID("GMBasic", 3)
  PRINT INSTR("GMBasic", "Bas")
  PRINT INSTR(4, "GMBasic", "a")
  PRINT INSTR("GMBasic", "basic")
  PRINT INSTR("GMBasic", "xyz")
  PRINT SEARCH("GMBasic", "bas")
  PRINT SEARCH("GMBasic", "xyz")
  PRINT ASC("A")
  PRINT CHR(65)
  PRINT "[" + SPACE(3) + "]"
  PRINT "[" + STR(123) + "]"
  PRINT VAL("123.5")
END SUB
