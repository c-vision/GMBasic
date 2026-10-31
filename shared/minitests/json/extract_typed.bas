' MiniTest: JSON.EXTRACT — il valore estratto conserva il TIPO: un numero stampa con la zona
' numerica, una stringa nuda (parità IL/PCode/VM-C). Prima sui backend C99 usciva sempre
' come stringa (`42` invece di ` 42`).
' EXPECT: num= 42|str=Ada
IMPORT JSON

SUB Main()
  DIM q AS STRING
  q = CHR(34)
  DIM doc AS JSON = JSON.PARSE("{" + q + "n" + q + ": 42, " + q + "s" + q + ": " + q + "Ada" + q + "}")
  PRINT "num="; JSON.EXTRACT(doc, "n")
  PRINT "str="; JSON.EXTRACT(doc, "s")
END SUB
