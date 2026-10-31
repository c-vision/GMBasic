' EXPECT: s="abc"|n=5|b=true|f=false|m="v"
' JSON.STRINGIFY di uno SCALARE (non contenitore): stringa virgolettata, numero nudo,
' TRUE/FALSE → true/false (NON -1/0).
' Prima: VM/C rendeva stringa VUOTA per numero e booleano (gmb_bf_760 non aveva il ramo
' scalare) e C99Native rendeva "-1" per TRUE (emetteva gmb_double_to_string(-1)).
' Vedi Bugs/(solved) - JSON-Stringify-Scalare-Diverge-Tra-Gambe.md.
IMPORT JSON
SUB Main()
  PRINT "s="; JSON.STRINGIFY("abc")
  PRINT "n="; JSON.STRINGIFY(5)
  PRINT "b="; JSON.STRINGIFY(TRUE)
  PRINT "f="; JSON.STRINGIFY(FALSE)
  ' Accesso dinamico `d("k")` su un oggetto parsato: rende un gmb_val_t GREZZO. Sul C99Native
  ' il fallback numerico lo castava a double (`gmb_double_to_string((gmb_double_t)(<val>))`)
  ' = C invalido, quindi il programma NON COMPILAVA. Vedi
  ' Bugs/(solved) - JSON-Stringify-Scalare-Diverge-Tra-Gambe.md, difetto 4.
  DIM q AS STRING
  q = CHR(34)
  DIM d AS JSON = JSON.PARSE("{" + q + "k" + q + ": " + q + "v" + q + "}")
  PRINT "m="; JSON.STRINGIFY(d("k"))
END SUB
