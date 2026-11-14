' EXPECT: [ciao]|[ciao]|[ciao]
IMPORT STRINGS
SUB Main()
  PRINT "[" + TRIM("  ciao  ") + "]"
  PRINT "[" + LTRIM("  ciao") + "]"
  PRINT "[" + RTRIM("ciao  ") + "]"
END SUB
