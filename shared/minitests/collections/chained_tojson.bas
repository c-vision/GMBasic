' MiniTest: metodi concatenati su un'ESPRESSIONE e .ToJson() su liste.
' Su C99 il parser SCARTAVA il metodo dopo `call(...)`: `d.Keys().ToJson()` diventava
' `d.Keys()` (lista) e il body SendJSON usciva `[]` invece di `"[]"`; `.ToJson()` su una
' lista finiva nel default 0. Ora: catena → JSON.STRINGIFY, lista → testo JSON.
' EXPECT: k=["a","b"]|l=[1,2]|s="[1,2]"
IMPORT JSON

SUB Main()
  DIM d AS DICT
  DIM l AS LIST OF LONG
  d("a") = 1 : d("b") = 2
  l.Add(1) : l.Add(2)
  PRINT "k="; d.Keys().ToJson()
  PRINT "l="; l.ToJson()
  PRINT "s="; JSON.STRINGIFY(l.ToJson())
END SUB
