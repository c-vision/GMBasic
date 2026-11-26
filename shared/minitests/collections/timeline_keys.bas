' MiniTest: TIMELINE.Keys() — le chiavi di una Timeline sono DATE: Keys() rende i loro
' SERIALI OADate (numeri), forma canonica identica a First()/Last()/GetRange()
' ({"Key":<seriale>,...}) su tutte e quattro le gambe. Prima: IL sollevava
' NotSupportedException ("metodo runtime 'Keys' non trovato su 'Timeline`1'"), PCode
' rendeva null, C99 non compilava (`DIM k AS LIST` + `k = t.Keys()` →
' gmb_val_to_double(<gmb_list_t>)). VM/C era già corretto.
' EXPECT: n= 2|json=[44936,44941]|k0= 44936|y0= 2023|json2=[44936,44941]|types=DOUBLE;DOUBLE;
IMPORT JSON
IMPORT COLLECTIONS
SUB Main()
  DIM t AS TIMELINE
  t.Add(DATEVALUE("2023-01-10"), "A")
  t.Add(DATEVALUE("2023-01-15"), "B")
  DIM k AS LIST
  k = t.Keys()
  PRINT "n="; LEN(k)
  PRINT "json="; JSON.STRINGIFY(k)
  PRINT "k0="; k(0)
  PRINT "y0="; YEAR(k(0))
  DIM k2 = t.Keys()
  PRINT "json2="; JSON.STRINGIFY(k2)
  DIM s AS STRING
  FOR EACH x IN k
    s = s + TYPENAME(x) + ";"
  NEXT
  PRINT "types="; s
END SUB
