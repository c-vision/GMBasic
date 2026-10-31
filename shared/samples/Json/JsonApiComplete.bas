' JsonApiComplete — caso reale completo che esercita OGNI funzione e comando del namespace
' JSON implementato/corretto in questa sessione, con verifica compatta (una riga di esito
' per blocco). Copre: PARSE, EXTRACT, KEYS, STRINGIFY, TOOBJECT, LOAD, READVALUE (2 arità),
' READSECTION (2 arità), i COMANDI SAVE / WRITEVALUE / WRITESECTION e i metodi DICT.
' Parità verificata su IL, PCodeVM, VM/C, C99Native (stesso output, tools/parity4).
' EXPECT: parse=ok|extract=ok|keys=ok|stringify=ok|toobject=ok|load=ok|readvalue=ok|readvalue_file=ok|readsection=ok|readsection_file=ok|dict=ok|dict_pair=ok|dict_values=ok|save=ok|writevalue=ok|writesection=ok|cmd_variant=ok|list_roundtrip=ok|ALL=ok
IMPORT JSON
IMPORT COLLECTIONS
IMPORT IO

TYPE ConfigData
  Host AS STRING * 50
  Timeout AS LONG
END TYPE

SUB Main()
  DIM q AS STRING
  q = CHR(34)
  DIM okc AS INTEGER
  DIM h AS LONG

  ' Output di categoria 4 (AGENTS §11): in `work/` accanto al sample. La cartella va creata
  ' dal sample stesso: senza, `IO.FCREATE("work/...")` fallisce sui backend gestiti
  ' (IL: "Handle file non valido") mentre la gamba C99Native crea il file comunque →
  ' divergenza IL vs C99Native.
  IF NOT IO.DIREXISTS("work") THEN MKDIR "work"

  ' 1. PARSE + accesso campo
  DIM doc AS JSON = JSON.PARSE("{" + q + "user" + q + ": {" + q + "name" + q + ": " + q + "Ada" + q + "}, " + q + "roles" + q + ": [" + q + "admin" + q + ", " + q + "dev" + q + "]}")
  okc = (doc.user.name = "Ada")
  PRINT "parse="; IIF(okc, "ok", "FAIL")

  ' 2. EXTRACT con percorso annidato e indice
  okc = (JSON.EXTRACT(doc, "roles[1]") = "dev")
  PRINT "extract="; IIF(okc, "ok", "FAIL")

  ' 3. KEYS dell'oggetto radice (TokenKind condiviso con dict.Keys)
  okc = (JSON.STRINGIFY(JSON.KEYS(doc)) = "[" + q + "user" + q + "," + q + "roles" + q + "]")
  PRINT "keys="; IIF(okc, "ok", "FAIL")

  ' 4. STRINGIFY di un DICT costruito a mano (metodi DICT Set/Get/Count)
  DIM d AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
  d.Set("a", 1)
  d.Set("b", 2)
  okc = (JSON.STRINGIFY(d) = "{" + q + "a" + q + ":1," + q + "b" + q + ":2}") AND (d.Get("b") = 2) AND (d.Count() = 2)
  PRINT "stringify="; IIF(okc, "ok", "FAIL")

  ' 5. TOOBJECT: JSON -> UDT tipizzata (Host è STRING * 50: confronto per contenuto)
  DIM cfg AS ConfigData
  cfg.Host = "test.host"
  cfg.Timeout = 1234
  DIM cfgJson AS JSON = JSON.TOJSON(cfg)
  DIM cfgBack AS ConfigData = JSON.TOOBJECT(ConfigData, cfgJson)
  okc = (cfgBack.Timeout = 1234) AND (INSTR(cfgBack.Host, "test.host") > 0)
  PRINT "toobject="; IIF(okc, "ok", "FAIL")

  ' 6. LOAD da file (scritto con IO: nessun comando JSON, resta 5-leg)
  h = IO.FCREATE("work/api_cfg.json")
  IO.FWRITE(h, "{" + q + "Host" + q + ": " + q + "test.host" + q + ", " + q + "Timeout" + q + ": 1234}")
  IO.FCLOSE(h)
  DIM loaded AS JSON = JSON.LOAD("work/api_cfg.json")
  okc = (INSTR(loaded.Host, "test.host") > 0)
  PRINT "load="; IIF(okc, "ok", "FAIL")

  ' 7. READVALUE (file, sezione, chiave, default) con file assente -> default
  okc = (JSON.READVALUE("nofile.ini", "nosection", "nokey", "fallback") = "fallback")
  PRINT "readvalue="; IIF(okc, "ok", "FAIL")

  ' 8. READVALUE (file, sezione, chiave) su file INI reale
  h = IO.FCREATE("work/api_rv.ini")
  IO.FWRITE(h, "[sec]" + CHR(10) + "key=42" + CHR(10))
  IO.FCLOSE(h)
  okc = (JSON.READVALUE("work/api_rv.ini", "sec", "key", "") = "42")
  PRINT "readvalue_file="; IIF(okc, "ok", "FAIL")

  ' 9. READSECTION (sezione) dal file di default app.settings.json
  ' (forma INI: coerente con i comandi WRITEVALUE/WRITESECTION, che scrivono INI)
  h = IO.FCREATE("app.settings.json")
  IO.FWRITE(h, "[db]" + CHR(10) + "host=db.local" + CHR(10))
  IO.FCLOSE(h)
  DIM dsec = JSON.READSECTION("db")
  okc = (dsec.host = "db.local")
  PRINT "readsection="; IIF(okc, "ok", "FAIL")

  ' 10. READSECTION (file, sezione) su file JSON annidato
  h = IO.FCREATE("work/api_sec.json")
  IO.FWRITE(h, "{" + q + "user" + q + ": {" + q + "name" + q + ": " + q + "Ada" + q + ", " + q + "age" + q + ": 36}}")
  IO.FCLOSE(h)
  DIM fsec = JSON.READSECTION("work/api_sec.json", "user")
  okc = (fsec.name = "Ada")
  PRINT "readsection_file="; IIF(okc, "ok", "FAIL")

  ' 11. Metodi DICT: Set/SetKeyValue/Get/Count/ContainsKey
  DIM n AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
  n.Set("a", 1) : n.Set("b", 3) : n.SetKeyValue("c", 2)
  okc = (n.Get("b") = 3) AND (n.Count() = 3) AND (n.ContainsKey("a") = -1)
  PRINT "dict="; IIF(okc, "ok", "FAIL")
  okc = (n.MaxPair() = "b") AND (n.MinPair() = "a")
  PRINT "dict_pair="; IIF(okc, "ok", "FAIL")
  okc = (JSON.STRINGIFY(n.Values()) = "[1,3,2]")
  PRINT "dict_values="; IIF(okc, "ok", "FAIL")

  ' 12. COMANDO JSON SAVE + LOAD
  DIM j AS JSON = JSON.PARSE("{" + q + "name" + q + ": " + q + "Ada" + q + "}")
  JSON SAVE j, "work/api_save.json"
  DIM jl AS JSON = JSON.LOAD("work/api_save.json")
  okc = (jl.name = "Ada")
  PRINT "save="; IIF(okc, "ok", "FAIL")

  ' 13. COMANDO JSON WRITEVALUE + READVALUE (file di default)
  JSON WRITEVALUE "ApiKey", "ABC-123"
  okc = (JSON.READVALUE("ApiKey") = "ABC-123")
  PRINT "writevalue="; IIF(okc, "ok", "FAIL")

  ' 14. COMANDO JSON WRITESECTION (da UDT) + READSECTION
  JSON WRITESECTION cfg, "ServerConfig"
  DIM wsec AS JSON = JSON.READSECTION("ServerConfig")
  okc = (INSTR(wsec.Host, "test.host") > 0)
  PRINT "writesection="; IIF(okc, "ok", "FAIL")

  ' 15. VARIANTI dei comandi: WRITEVALUE con SEZIONE esplicita, SAVE/LOAD di una LISTA
  JSON WRITEVALUE "Key1", "Val1", "SecA"
  okc = (JSON.READVALUE("app.settings.json", "SecA", "Key1", "MISS") = "Val1")
  PRINT "cmd_variant="; IIF(okc, "ok", "FAIL")
  DIM lst AS LIST OF INTEGER = [1, 2, 3]
  JSON SAVE lst, "work/api_list.json"
  DIM lj2 AS JSON = JSON.LOAD("work/api_list.json")
  okc = (JSON.STRINGIFY(lj2) = "[1,2,3]")
  PRINT "list_roundtrip="; IIF(okc, "ok", "FAIL")

  ' 16. (JSON OPEN non incluso: è ancora st_noop/non gestito sui backend C99 — gap tracciato.)

  PRINT "ALL=ok"

  KILL "work/api_cfg.json"
  KILL "work/api_rv.ini"
  KILL "work/api_sec.json"
  KILL "work/api_save.json"
  KILL "work/api_list.json"
  KILL "app.settings.json"
END SUB
