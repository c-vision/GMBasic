' MiniTest: JSON.KEYS con argomenti NON-oggetto e display di un oggetto con un bool.
' Canone (IL/PCode/VM-C e ora anche C99): JSON.KEYS restituisce le chiavi SOLO per un
' OGGETTO; per stringa (anche JSON in una variabile), numero, valore dinamico scalare e
' array il risultato è null. In un dict il valore booleano si stampa -1/0 (non True/False).
' EXPECT: str=null|num=null|dyn=null|dict=["a","b"]|obj=["a"]|prn=[]|disp=[[ok, -1]]
IMPORT JSON

SUB Main()
  DIM q AS STRING
  q = CHR(34)
  DIM s AS STRING
  DIM doc AS JSON
  DIM bdoc AS JSON
  DIM d AS DICT
  s = "{" + q + "a" + q + ": 1}"
  doc = JSON.PARSE(s)
  bdoc = JSON.PARSE("{" + q + "ok" + q + ": true}")
  d("a") = 1 : d("b") = 2
  PRINT "str="; JSON.STRINGIFY(JSON.KEYS(s))
  PRINT "num="; JSON.STRINGIFY(JSON.KEYS(1))
  PRINT "dyn="; JSON.STRINGIFY(JSON.KEYS(doc("a")))
  PRINT "dict="; JSON.STRINGIFY(JSON.KEYS(d))
  PRINT "obj="; JSON.STRINGIFY(JSON.KEYS(doc))
  PRINT "prn=["; JSON.KEYS(doc("a")); "]"
  PRINT "disp="; bdoc
END SUB
