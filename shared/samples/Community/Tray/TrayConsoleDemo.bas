' ============================================================================
' TrayConsoleDemo - sample COMPLETO del namespace TRAY (tutte le 10 funzioni).
' Dimostra: icona nella barra del SO (default GMBasic), menu con azioni, polling
' dei click, tooltip dinamico, dashboard HTTP, chiusura ordinata.
'
' USO:  dotnet REPL/bin/Debug/net10.0/REPL.dll samples/Community/Tray/TrayConsoleDemo.bas
'       (GMBASIC_VM_HOST=<repo>/native/gmb_vm_c/build/gmb_vm_host)
' PARIT-: verificato su 4 backend con tools/parity4 minitests/tray/tray_parity.bas
' ============================================================================
IMPORT TRAY
IMPORT HTTP

DIM g_stop AS LONG
DIM g_clicks AS LONG

SUB Main
  g_stop = 0
  g_clicks = 0
  PRINT "TRAYBACKEND="; TRAYBACKEND()
  PRINT "TRAYAVAILABLE="; TRAYAVAILABLE()
  PRINT "TRAYSELFTEST (prima)="; TRAYSELFTEST()
  IF TRAYAVAILABLE() = 0 THEN
    PRINT "tray non disponibile su questo backend/host (atteso su headless/CI)"
    EXIT SUB
  END IF
  DIM r AS LONG
  r = TRAYCREATE("", "TrayConsoleDemo")          ' icona default GMBasic
  r = TRAYITEM(1, "Stato")
  r = TRAYITEM(2, "Apri console")
  r = TRAYSEP()
  r = TRAYITEM(99, "Esci")
  r = TRAYTOOLTIP("TrayConsoleDemo - avviato")
  DIM cfg AS HTTPCONFIG
  cfg = NEW HTTPCONFIG
  cfg.Port = 8795
  HTTP CONFIG cfg
  HTTP ROUTE "GET", "/", HomeHandler()
  HTTP SERVER cfg.Port
  PRINT "dashboard: http://localhost:8795"
  DIM i AS LONG
  DIM last AS LONG
  FOR i = 1 TO 600
    SLEEP 1000
    r = TRAYPOLL()
    last = TRAYLAST()
    IF last > 0 THEN
      g_clicks = g_clicks + 1
      PRINT "menu click id="; last
      IF last = 1 THEN
        r = TRAYTOOLTIP("TrayConsoleDemo - click #" + LTRIM(STR(g_clicks)))
      END IF
      IF last = 2 THEN
        PRINT "apri console -> http://localhost:8795"
      END IF
      IF last = 99 THEN
        g_stop = 1
      END IF
    END IF
    IF g_stop = 1 THEN
      EXIT FOR
    END IF
  NEXT
  TRAYREMOVE()
  r = TRAYPOLL()
  HTTP SERVER STOP
  PRINT "chiuso - selftest="; TRAYSELFTEST()
END SUB

SUB HomeHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.ContentType = "text/html"
  res.Send("<html><body><h2>TrayConsoleDemo</h2><p>click dal menu: " + LTRIM(STR(g_clicks)) + "</p></body></html>")
END SUB
