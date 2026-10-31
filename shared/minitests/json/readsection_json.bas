' MiniTest: JSON.READSECTION — le DUE firme canoniche (IlRuntime.ReadSection):
'   (sezione$)          → sezione dal file di default app.settings.json
'   (file$, sezione$)   → sezione dal file indicato
' Entrambe su file JSON (sezione annidata) e su INI (minitests/json/readsection.bas).
' EXPECT: def=db.local|defport= 5432|net=example.com|netport= 8080
IMPORT JSON
IMPORT IO

SUB Main()
  DIM q AS STRING
  q = CHR(34)
  DIM h AS LONG
  ' forma a 1 argomento: sezione dal file di default
  h = IO.FCREATE("app.settings.json")
  IO.FWRITE(h, "{" + q + "db" + q + ": {" + q + "host" + q + ": " + q + "db.local" + q + ", " + q + "port" + q + ": 5432}}")
  IO.FCLOSE(h)
  DIM dsec = JSON.READSECTION("db")
  PRINT "def="; dsec.host
  PRINT "defport="; dsec.port
  ' forma a 2 argomenti: file esplicito
  h = IO.FCREATE("rs_json_probe.json")
  IO.FWRITE(h, "{" + q + "net" + q + ": {" + q + "host" + q + ": " + q + "example.com" + q + ", " + q + "port" + q + ": 8080}}")
  IO.FCLOSE(h)
  DIM nsec = JSON.READSECTION("rs_json_probe.json", "net")
  PRINT "net="; nsec.host
  PRINT "netport="; nsec.port
  KILL "app.settings.json"
  KILL "rs_json_probe.json"
END SUB
