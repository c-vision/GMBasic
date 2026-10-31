' EXPECT: rt={"x":true,"y":false}|arr=[true,false]|xnum=-1|ynum=0|ifx=vero|eq=-1
' BOOLEAN in JSON.PARSE: il tipo deve SOPRAVVIVERE al parse (true/false, non -1/0),
' mentre il valore in contesto numerico resta -1/0 (canone TRUE = -1 del linguaggio).
' Difetto: il parser mappava JsonValueKind.True -> -1.0, quindi ogni round-trip
' parse->stringify cambiava il TIPO nel JSON. Vedi
' Bugs/(open) - JSON-Parse-Booleani-RoundTrip.md.
' NB: le asserzioni numeriche stampano TESTO (IF ... THEN PRINT) e non il numero: PRINT
' di un valore positivo aggiunge lo spazio di allineamento ("y= 0") e l'EXPECT non lo
' tollera.
IMPORT JSON
SUB Main()
  DIM q AS STRING = CHR(34)
  DIM src AS STRING
  DIM n AS LONG
  src = "{" + q + "x" + q + ": true, " + q + "y" + q + ": false}"
  DIM o AS JSON = JSON.PARSE(src)
  PRINT "rt="; JSON.STRINGIFY(o)
  DIM a AS JSON = JSON.PARSE("[true, false]")
  PRINT "arr="; JSON.STRINGIFY(a)
  IF o("x") = -1 THEN PRINT "xnum=-1" ELSE PRINT "xnum=altro"
  IF o("y") = 0 THEN PRINT "ynum=0" ELSE PRINT "ynum=altro"
  IF o("x") THEN PRINT "ifx=vero" ELSE PRINT "ifx=falso"
  n = 0
  IF o("x") = TRUE THEN n = -1
  IF n = -1 THEN PRINT "eq=-1" ELSE PRINT "eq=altro"
END SUB
