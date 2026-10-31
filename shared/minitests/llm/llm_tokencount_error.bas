' MiniTest: LLM.TOKENCOUNT su modello inesistente -> -1 (nessun errore sollevato) su 4 backend.
' EXPECT: rc=-1
IMPORT LLM
SUB Main()
  PRINT "rc="; LLM.TOKENCOUNT("work/no_model", "ciao")
END SUB
