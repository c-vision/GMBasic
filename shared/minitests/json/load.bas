' MiniTest: JSON.LOAD(file$) — carica un file JSON e legge i campi (val boxata).
' EXPECT: name=Ada|age= 36
IMPORT JSON
IMPORT IO

SUB Main()
  DIM q AS STRING
  q = CHR(34)
  DIM h AS LONG
  h = IO.FCREATE("load_probe.json")
  IO.FWRITE(h, "{" + q + "name" + q + ": " + q + "Ada" + q + ", " + q + "age" + q + ": 36}")
  IO.FCLOSE(h)
  DIM o AS JSON = JSON.LOAD("load_probe.json")
  PRINT "name="; o.name
  PRINT "age="; o.age
  KILL "load_probe.json"
END SUB
