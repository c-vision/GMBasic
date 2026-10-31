' EXPECT: tj={"a":2,"b":3}
IMPORT JSON
IMPORT COLLECTIONS
SUB Main()
  ' JSON.TOJSON su un DICT tipizzato — F2: serializzazione dict del runtime C
  ' (gmb_json_append_dict, ordine di inserimento) riconosciuta dal box VM
  ' (kind GMB_OBJ_DICT + header.data). Builtin 762 fix: non azzerava il result.
  DIM d AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
  d.Set("a", 2)
  d.Set("b", 3)
  PRINT "tj="; JSON.TOJSON(d)
END SUB
