' MiniTest NAMESPACE JSON — tutte le 9 funzioni della superficie.
' Extract, KEYS, Load, Parse, ReadSection, ReadValue, Stringify, ToJson, ToObject.
' Input identici ai MiniTest per-funzione; file temporanei cancellati a fine test.
' EXPECT: name=Ada|tag=home|tag2=dev|keys=["user","tags"]|sj=[1,2]|sq={"k":"v"}|sl=[9,8,7]|tj=[1,2,3]|loadname=Ada|loadage= 36|host=example.com|port= 8080|def=fallback|val=42|val2=dflt|host=test.host|timeout= 1234|count= 3
IMPORT JSON
IMPORT IO

TYPE CfgData
  Host AS STRING * 50
  Timeout AS LONG
END TYPE

SUB TestParseExtract()
  DIM q AS STRING
  q = CHR(34)
  DIM doc AS JSON = JSON.PARSE("{" + q + "user" + q + ":{" + q + "name" + q + ":" + q + "Ada" + q + "}," + q + "tags" + q + ":[" + q + "home" + q + "," + q + "dev" + q + "]}")
  PRINT "name="; JSON.EXTRACT(doc, "user.name")
  PRINT "tag="; JSON.EXTRACT(doc, "tags[0]")
  PRINT "tag2="; JSON.EXTRACT(doc, "tags[1]")
  PRINT "keys="; JSON.STRINGIFY(JSON.KEYS(doc))
END SUB

SUB TestStringify()
  DIM a = {1, 2}
  PRINT "sj="; JSON.STRINGIFY(a)
  DIM d AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
  d.Set("k", "v")
  PRINT "sq="; JSON.STRINGIFY(d)
  DIM l AS LIST OF INTEGER = NEW LIST OF INTEGER
  l.Add(9)
  l.Add(8)
  l.Add(7)
  PRINT "sl="; JSON.STRINGIFY(l)
  PRINT "tj="; JSON.TOJSON({1, 2, 3})
END SUB

SUB TestLoad()
  DIM q AS STRING
  q = CHR(34)
  DIM h AS LONG
  h = IO.FCREATE("work/json_load.json")
  IO.FWRITE(h, "{" + q + "name" + q + ": " + q + "Ada" + q + ", " + q + "age" + q + ": 36}")
  IO.FCLOSE(h)
  DIM o AS JSON = JSON.LOAD("work/json_load.json")
  PRINT "loadname="; o.name
  PRINT "loadage="; o.age
  KILL "work/json_load.json"
END SUB

SUB TestReadSection()
  DIM h AS LONG
  h = IO.FCREATE("work/json_rs.ini")
  IO.FWRITE(h, "[net]" + CHR(10) + "host=example.com" + CHR(10) + "port=8080" + CHR(10))
  IO.FCLOSE(h)
  DIM sec = JSON.READSECTION("work/json_rs.ini", "net")
  PRINT "host="; sec.host
  PRINT "port="; sec.port
  KILL "work/json_rs.ini"

  DIM h2 AS LONG
  h2 = IO.FCREATE("work/json_rv.ini")
  IO.FWRITE(h2, "[sec]" + CHR(10) + "key=42" + CHR(10))
  IO.FCLOSE(h2)
  PRINT "def="; JSON.READVALUE("work/nofile.ini", "nosection", "nokey", "fallback")
  PRINT "val="; JSON.READVALUE("work/json_rv.ini", "sec", "key", "")
  PRINT "val2="; JSON.READVALUE("work/json_rv.ini", "sec", "missing", "dflt")
  KILL "work/json_rv.ini"
END SUB

SUB TestToObject()
  DIM u AS CfgData
  u.Host = "test.host"
  u.Timeout = 1234
  DIM j AS JSON = JSON.TOJSON(u)
  DIM back AS CfgData = JSON.TOOBJECT(CfgData, j)
  PRINT "host="; TRIM(back.Host)
  PRINT "timeout="; back.Timeout
  DIM lst AS LIST OF INTEGER = [10, 20, 30]
  DIM lj AS JSON = JSON.TOJSON(lst)
  DIM l2 AS LIST OF INTEGER
  l2 = JSON.TOOBJECT(LIST OF INTEGER, lj)
  PRINT "count="; l2.Count
END SUB

SUB Main()
  CHDIR PROJECTFOLDER()
  MKDIR "work"
  TestParseExtract()
  TestStringify()
  TestLoad()
  TestReadSection()
  TestToObject()
END SUB
