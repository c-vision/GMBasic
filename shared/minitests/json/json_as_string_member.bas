' MiniTest: `DIM x AS JSON = "<stringa JSON>"` — il member access x.Campo PARSA la stringa
' on-access (parità IL/PCode: x resta una stringa, il parse avviene alla lettura del membro).
' Regressione per bug JSON-AS-JSON-String-Member-Access-No-Parse (C99Native).
' EXPECT: a= 1
IMPORT JSON

SUB Main()
  DIM x AS JSON = "{""a"": 1}"
  PRINT "a="; x.a
END SUB
