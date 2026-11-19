' Scrittura in un dict INTERNO portando il sotto-oggetto in una variabile (workaround
' documentato per `a[i][j] = v`, non supportato dal backend C99).
' Il valore estratto e' un RIFERIMENTO al dict interno (IL/PCode/C99/VM/C): la mutazione
' deve restare visibile dal PADRE (`g["bob"][...]`). VM/C copiava il contenitore in
' gmb_adapter_val_to_stack → scrittura su una copia, dato perso in silenzio.
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
