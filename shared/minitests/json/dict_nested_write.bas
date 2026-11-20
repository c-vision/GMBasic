' Scrittura in un dict INTERNO portando il sotto-oggetto in una variabile.
' Storia: `a[i][j] = v` sul C99 prima perdeva la scrittura in silenzio (nome artificiale
' "a[]"), poi è stata rifiutata con errore esplicito, ora è SUPPORTATA (il parser la desugara
' in `DIM t = a[i]` + `t[j] = v` con un riferimento al dict interno). Questo test verifica la
' forma esplicita con la variabile; la forma nativa è in json/nested_index_assign.bas.
' EXPECT: k=1|child=yes
IMPORT JSON
SUB Main()
  DIM g AS JSON
  g = JSON.PARSE("{" + CHR$(34) + "bob" + CHR$(34) + ":{}" + "}")
  DIM t AS JSON
  t = g["bob"]
  t["k"] = "1"
  PRINT "k="; g["bob"]["k"]

  DIM c AS JSON
  c = NEW JSON
  c["p"] = "yes"
  t["child"] = c
  PRINT "child="; g["bob"]["child"]["p"]
END SUB
