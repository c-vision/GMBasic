' EXPECT: m=repomap: [A->B;Main->A]
IMPORT LLM
SUB A()
  B()
END SUB
SUB B()
END SUB
SUB Main()
  A()
  PRINT "m="; LLM.REPOMAP()
END SUB
