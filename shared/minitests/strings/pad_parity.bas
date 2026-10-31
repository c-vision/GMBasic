' Minitest PADL/PADR/PADC — parità 4 backend: forma 2-arg (fill=spazio) e 3-arg.
IMPORT STRINGS
SUB Main()
  PRINT "[" + PADR("ab", 5) + "]"
  PRINT "[" + PADL("ab", 5) + "]"
  PRINT "[" + PADC("ab", 6) + "]"
  PRINT "[" + PADR("ab", 5, "-") + "]"
  PRINT "[" + PADL("ab", 5, ".") + "]"
  PRINT "[" + LEFT(PADR("abcdefgh", 5), 5) + "]"
END SUB
