' MiniTest: una DATA usata come CHIAVE di un DICT. Canone scelto (più efficiente e già
' presente nel runtime): ISO-8601 "yyyy-MM-ddTHH:mm:ss", la STESSA forma del JSON delle
' date (`gmb_dt_datetime_iso`, unica implementazione). Prima: IL/PCode ISO, VM/C numero
' ("44936"), C99 seriale ("44936") — tre rese e lookup incoerenti fra loro.
' EXPECT: keys=["2023-01-10T00:00:00"]|lookup= 1|has=-1
IMPORT JSON

SUB Main()
  DIM d AS DICT
  d(DATEVALUE("2023-01-10")) = 1
  PRINT "keys="; JSON.STRINGIFY(d.Keys())
  PRINT "lookup="; d(DATEVALUE("2023-01-10"))
  PRINT "has="; d.ContainsKey(DATEVALUE("2023-01-10"))
END SUB
