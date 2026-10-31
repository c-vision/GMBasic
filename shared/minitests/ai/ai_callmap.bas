' EXPECT: cm=callers=[Main] callees=[Bar]|none=
IMPORT LLM
SUB Bar()
END SUB
SUB Foo()
  Bar()
END SUB
SUB Main()
  Foo()
  PRINT "cm="; LLM.CALLMAP("Foo")
  PRINT "none="; LLM.CALLMAP("NonEsiste")
END SUB
