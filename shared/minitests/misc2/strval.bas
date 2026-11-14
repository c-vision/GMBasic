' EXPECT: [ 12]|[-5]|[ 0]|-12.5|123|0|0|5
SUB Main()
  PRINT "[" + STR(12) + "]"
  PRINT "[" + STR(-5) + "]"
  PRINT "[" + STR(0) + "]"
  PRINT VAL("  -12.5")
  PRINT VAL("123abc")
  PRINT VAL("abc")
  PRINT LEN(SPACE(0))
  PRINT LEN(SPACE(5))
END SUB
