' EXPECT: sym=procedure Foo|nope=|mix=function Calc
IMPORT LLM
SUB Foo()
END SUB
FUNCTION Calc() AS DOUBLE
  Calc = 1
END FUNCTION
SUB Main()
  PRINT "sym="; LLM.SYMBOL("Foo")
  PRINT "nope="; LLM.SYMBOL("NonEsiste")
  PRINT "mix="; LLM.SYMBOL("Calc")
END SUB
