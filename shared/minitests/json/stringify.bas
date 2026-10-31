' EXPECT: sj=[1,2]|sq={"k":"v"}|sl=[9,8,7]
IMPORT JSON
SUB Main()
  ' JSON.STRINGIFY — rotta 760 (TokenKind.Stringify), distinta da TOJSON (762):
  ' stessa serializzazione runtime (gmb_val_to_json) — F2: unica implementazione.
  DIM a = {1, 2}
  PRINT "sj="; JSON.STRINGIFY(a)
  DIM d AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
  d.Set("k", "v")
  PRINT "sq="; JSON.STRINGIFY(d)
  DIM l AS LIST OF INTEGER = NEW LIST OF INTEGER
  l.Add(9)
  l.Add(8)
  l.Add(7)
  PRINT "sl="; JSON.STRINGIFY(l)
END SUB
