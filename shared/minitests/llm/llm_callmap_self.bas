' EXPECT: cm=callers=[] callees=[Helper]|ins=procedure Main callers=[] callees=[Helper]
' Regressione §4.10: il call-graph C (gmb_semfold) NON deve annoverare le fn §4.10 (e altri
' builtin) come callee quando si interroga il simbolo ENCLOSING (contiene le call §4.10).
IMPORT LLM
SUB Helper()
END SUB
SUB Main()
  Helper()
  PRINT "cm="; LLM.CALLMAP("Main")
  PRINT "ins="; LLM.INSPECT("Main")
END SUB
