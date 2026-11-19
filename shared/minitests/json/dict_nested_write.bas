' Scrittura in un dict INTERNO portando il sotto-oggetto in una variabile (workaround
' documentato per `a[i][j] = v`, non supportato dal backend C99).
' NB: la prova e' fatta sull'HANDLE (`t`), non dal padre. La visibilita' dal padre
' (`g["bob"]["k"]`) vale su IL/PCode/C99; su VM/C un dict estratto con `g[...]` e' una
' COPIA (gap noto VM/C: la mutazione non torna su `g`).
' EXPECT: k=1|child=yes
IMPORT JSON
SUB Main()
  DIM g AS JSON
  g = JSON.PARSE("{" + CHR$(34) + "bob" + CHR$(34) + ":{}" + "}")
  DIM t AS JSON
  t = g["bob"]
  t["k"] = "1"
  PRINT "k="; t["k"]

  DIM c AS JSON
  c = NEW JSON
  c["p"] = "yes"
  t["child"] = c
  PRINT "child="; t["child"]["p"]
END SUB
