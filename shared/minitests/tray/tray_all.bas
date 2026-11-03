' MiniTest NAMESPACE TRAY — tutte le 10 funzioni della superficie.
' Headless: valori normalizzati (>= -1) e self-test del backend; nessun display richiesto.
IMPORT TRAY
SUB Main()
  PRINT "backend>0="; (LEN(TRAYBACKEND()) > 0)
  PRINT "available>=-1="; (TRAYAVAILABLE() >= -1)
  PRINT "selftest0="; TRAYSELFTEST()
  DIM r AS LONG
  r = TRAYCREATE("", "mt")
  PRINT "create>=-1="; (r >= -1)
  r = TRAYITEM(1, "Stato")
  PRINT "item>=-1="; (r >= -1)
  r = TRAYSEP()
  PRINT "sep>=-1="; (r >= -1)
  r = TRAYTOOLTIP("mt")
  PRINT "tooltip>=-1="; (r >= -1)
  PRINT "poll>=-1="; (TRAYPOLL() >= -1)
  PRINT "last>=-1="; (TRAYLAST() >= -1)
  r = TRAYREMOVE()
  PRINT "remove>=-1="; (r >= -1)
END SUB
