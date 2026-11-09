' MiniTest: `JSON.KEYS(doc)` con un LOCALE OMONIMO (`DIM keys() AS STRING`).
' La call QUALIFICATA deve chiamare il builtin, non essere risolta come indice dell'array
' locale: su C99Native emetteva `gmb_list_get(keys, (gmb_int_t)(doc))` con doc gmb_val_t
' → "operand of type 'gmb_val_t' where arithmetic or pointer type is required" (C non
' compilabile), mentre IL/PCode/VM-C davano le chiavi giuste.
' L'array locale resta poi utilizzabile come array (seconda parte del test).
' EXPECT: ub= 1|k0=a|k1=b|mut=z
IMPORT JSON

SUB Main()
  DIM q AS STRING
  q = CHR(34)
  DIM keys() AS STRING
  DIM doc AS JSON = JSON.PARSE("{" + q + "a" + q + ": 1, " + q + "b" + q + ": 2}")
  keys = JSON.KEYS(doc)
  PRINT "ub="; UBOUND(keys)
  PRINT "k0="; keys(0)
  PRINT "k1="; keys(1)
  keys(0) = "z"
  PRINT "mut="; keys(0)
END SUB
