' MiniTest: JSON.TOOBJECT(tipo, json) — UDT e LIST OF INTEGER (tutto in memoria).
' EXPECT: host=test.host|timeout= 1234|count= 3
IMPORT JSON

TYPE CfgData
  Host AS STRING * 50
  Timeout AS LONG
END TYPE

SUB Main()
  DIM u AS CfgData
  u.Host = "test.host"
  u.Timeout = 1234
  DIM j AS JSON = JSON.TOJSON(u)
  DIM back AS CfgData = JSON.TOOBJECT(CfgData, j)
  PRINT "host="; back.Host
  PRINT "timeout="; back.Timeout
  DIM lst AS LIST OF INTEGER = [10, 20, 30]
  DIM lj AS JSON = JSON.TOJSON(lst)
  DIM l2 AS LIST OF INTEGER
  l2 = JSON.TOOBJECT(LIST OF INTEGER, lj)
  PRINT "count="; l2.Count
END SUB
