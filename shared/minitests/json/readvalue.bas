' MiniTest: JSON.READVALUE — due arità canoniche (IlRuntime.ReadValue):
'   (file$, sezione$, chiave$ [, default$])  → dal file indicato (default se manca)
'   (chiave$)                                → chiave dal file di default app.settings.json
' Prima il test era un falso oracolo (`IF v = "default" OR LEN(v) >= 0` è sempre vero):
' passava anche con lo stub `gmb_string_alloc("")` dell'emitter C#.
' EXPECT: def=fallback|val=42|kv=hello
IMPORT JSON
IMPORT IO

SUB Main()
  ' file assente → valore di default
  DIM d$ = JSON.READVALUE("nofile.ini", "nosection", "nokey", "fallback")
  PRINT "def="; d$
  ' file INI presente → valore reale
  DIM h AS LONG
  h = IO.FCREATE("rv_probe.ini")
  IO.FWRITE(h, "[sec]" + CHR(10) + "key=42" + CHR(10))
  IO.FCLOSE(h)
  DIM v$ = JSON.READVALUE("rv_probe.ini", "sec", "key", "")
  PRINT "val="; v$
  ' forma a 1 argomento: chiave dal file di default
  h = IO.FCREATE("app.settings.json")
  IO.FWRITE(h, "{" + CHR(34) + "k" + CHR(34) + ": " + CHR(34) + "hello" + CHR(34) + "}")
  IO.FCLOSE(h)
  DIM kv$ = JSON.READVALUE("k")
  PRINT "kv="; kv$
  KILL "rv_probe.ini"
  KILL "app.settings.json"
END SUB
