' MiniTest: COMANDI JSON — WRITEVALUE/READVALUE, WRITESECTION/READSECTION, SAVE/LOAD.
' Verifica la controparte di SCRITTURA (i comandi), che sul VM/C erano non implementati
' (249/250 mancanti, 251 instradato a vectordb → "syscall non implementato").
' EXPECT: wv=ABC-123|ws= 5000|save=Ada
IMPORT JSON
IMPORT IO

TYPE CmdConfig
  Host AS STRING * 50
  Timeout AS LONG
END TYPE

SUB Main()
  DIM q AS STRING
  q = CHR(34)
  DIM cfg AS CmdConfig
  cfg.Host = "api.server.com"
  cfg.Timeout = 5000
  ' JSON WRITEVALUE + READVALUE (file di default app.settings.json)
  JSON WRITEVALUE "ApiKey", "ABC-123"
  PRINT "wv="; JSON.READVALUE("ApiKey")
  ' JSON WRITESECTION (da UDT) + READSECTION (sezione dal file di default)
  JSON WRITESECTION cfg, "ServerConfig"
  DIM sec AS JSON = JSON.READSECTION("ServerConfig")
  PRINT "ws="; sec.Timeout
  ' JSON SAVE + LOAD su file esplicito
  DIM j AS JSON = JSON.PARSE("{" + q + "name" + q + ": " + q + "Ada" + q + "}")
  JSON SAVE j, "cmd_probe.json"
  DIM l AS JSON = JSON.LOAD("cmd_probe.json")
  PRINT "save="; l.name
  KILL "cmd_probe.json"
  KILL "app.settings.json"
END SUB
