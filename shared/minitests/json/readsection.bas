' MiniTest: JSON.READSECTION(file$, sezione$) — forma esplicita a 2 argomenti.
' Legge la sezione [net] da un file INI locale (niente rete, niente stato globale).
' EXPECT: host=example.com|port= 8080
IMPORT JSON
IMPORT IO

SUB Main()
  DIM h AS LONG
  h = IO.FCREATE("settings_rs.ini")
  IO.FWRITE(h, "[net]" + CHR(10) + "host=example.com" + CHR(10) + "port=8080" + CHR(10))
  IO.FCLOSE(h)
  DIM sec = JSON.READSECTION("settings_rs.ini", "net")
  PRINT "host="; sec.host
  PRINT "port="; sec.port
  KILL "settings_rs.ini"
END SUB
