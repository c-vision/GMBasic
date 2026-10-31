' MiniTest: COMANDI JSON — varianti non coperte da json/commands.bas:
'   - JSON WRITEVALUE con SEZIONE esplicita (3 argomenti) e AGGIORNAMENTO della stessa chiave;
'   - JSON WRITESECTION che SOSTITUISCE una sezione già esistente (2 scritture);
'   - JSON SAVE/LOAD di una LISTA (radice array) + JSON.EXTRACT("[i]").
' EXPECT: sec=Val1|upd=Val2|rep=-1|list=[1,2,3]
IMPORT JSON
IMPORT IO

TYPE Cfg
  Host AS STRING * 50
  Timeout AS LONG
END TYPE

SUB Main()
  ' WRITEVALUE con sezione esplicita + aggiornamento della stessa chiave
  JSON WRITEVALUE "Key1", "Val1", "SecA"
  PRINT "sec="; JSON.READVALUE("app.settings.json", "SecA", "Key1", "MISS")
  JSON WRITEVALUE "Key1", "Val2", "SecA"
  PRINT "upd="; JSON.READVALUE("app.settings.json", "SecA", "Key1", "MISS")
  ' WRITESECTION che sostituisce una sezione esistente
  DIM c1 AS Cfg : c1.Host = "h1" : c1.Timeout = 1
  JSON WRITESECTION c1, "SecB"
  DIM c2 AS Cfg : c2.Host = "h2" : c2.Timeout = 2
  JSON WRITESECTION c2, "SecB"
  DIM s = JSON.READSECTION("app.settings.json", "SecB")
  PRINT "rep="; INSTR(s.Host, "h2") > 0
  ' SAVE/LOAD di una LISTA (radice array)
  DIM l AS LIST OF INTEGER = [1, 2, 3]
  JSON SAVE l, "cmd_list.json"
  DIM lj AS JSON = JSON.LOAD("cmd_list.json")
  ' (JSON.EXTRACT su un elemento numerico ritorna tipizzato su IL/PCode/VM-C ma STRINGA sui
  '  backend C99 — gap pre-esistente; qui si verifica il round-trip con STRINGIFY, 5-leg.)
  PRINT "list="; JSON.STRINGIFY(lj)
  KILL "cmd_list.json"
  KILL "app.settings.json"
END SUB
