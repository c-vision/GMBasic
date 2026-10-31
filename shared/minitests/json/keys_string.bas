' MiniTest: JSON.KEYS su una STRINGA (non un oggetto) → JSON null (parità IL/PCode/VM-C).
' Prima: i C99 serializzavano le chiavi del JSON parsato (`["a"]`) e il VM/C usciva vuoto.
' EXPECT: ks=null|nil=null
IMPORT JSON

SUB Main()
  DIM q AS STRING
  q = CHR(34)
  PRINT "ks="; JSON.STRINGIFY(JSON.KEYS("{" + q + "a" + q + ": 1}"))
  PRINT "nil="; JSON.STRINGIFY(NIL)
END SUB
