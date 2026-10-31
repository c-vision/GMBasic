' ============================================================================
' ServiceConsoleTui — console TUI di controllo (client del daemon ServiceConsole).
' Legge /status.txt dal daemon e permette di fermare/avviare i servizi.
' TASTI: [a] avvia tutti  [f] ferma tutti  [q] esci  (refresh ogni 2s)
' USO:   dotnet REPL/bin/Debug/net10.0/REPL.dll APPS/ServiceConsole/src/ServiceConsoleTui.bas
'        (GMBASIC_VM_HOST=<repo>/native/gmb_vm_c/build/gmb_vm_host)
' ============================================================================
IMPORT HTTP
IMPORT WINDOWS
IMPORT STRINGS

DIM g_host AS STRING
DIM g_lastTxt AS STRING
DIM g_rowStat(32) AS STRING
DIM g_rowCount AS INTEGER
DIM g_drawn AS INTEGER
DIM g_sel AS INTEGER
DIM g_nameOf(32) AS STRING
DIM g_portOf(32) AS STRING

SUB Main
  g_host = "http://localhost:8770"
  WINDOW DEFINE Console FROM 1, 1 TO 24, 80 TITLE "ServiceConsole - controllo"
  WINDOW Console ACTIVATE
  RefreshStatus()
  g_sel = 0
  DIM quit AS LONG
  quit = 0
  DIM k AS STRING
  DrawOnce()
  WHILE quit = 0
    RefreshStatus()
    DIM t AS LONG
    DIM got AS LONG
    got = 0
    FOR t = 1 TO 20
      SLEEP 100
      IF got = 0 THEN
        k = INKEY()
        IF LEN(k) > 0 THEN
          got = 1
          IF k = "q" THEN quit = 1
          IF k = "f" THEN DoStopAll()
          IF k = "a" THEN DoStartAll()
          IF k = "s" THEN StopSel()
          IF k = "t" THEN StartSel()
          IF k = "r" THEN RestartSel()
          IF k >= "1" AND k <= "9" THEN SetSel(VAL(k))
        END IF
      END IF
    NEXT
  WEND
  WINDOW Console CLEAR
  PRINT "console chiusa"
END SUB

' Prima stesura (frame + righe statiche). Solo all'avvio.
SUB DrawOnce
  WINDOW Console CLEAR
  WINDOW Console PRINT "ServiceConsole - controllo   (" + g_host + ")"
  WINDOW Console PRINT LEFT(PADR("[a] avvia tutti  [f] ferma tutti  [s] stop sel  [t] start sel  [r] restart sel  [q] esci", 78), 78)
  WINDOW Console PRINT ""
  WINDOW Console PRINT "  #  " + LEFT(PADR("nome", 26), 26) + " " + LEFT(PADR("porta", 7), 7) + " " + LEFT(PADR("stato", 6), 6)
  WINDOW Console PRINT "  -  " + LEFT(PADR("--------------------------", 26), 26) + " " + LEFT(PADR("-------", 7), 7) + " " + LEFT(PADR("------", 6), 6)
  DIM txt AS STRING
  txt = HTTP.GET(g_host + "/status.txt")
  DIM rest AS STRING
  rest = txt
  DIM row AS INTEGER
  row = 0
  WHILE LEN(rest) > 0
    DIM nl AS INTEGER
    nl = INSTR(rest, CHR(10))
    DIM line AS STRING
    IF nl > 0 THEN
      line = LEFT(rest, nl - 1)
      rest = MID(rest, nl + 1, LEN(rest) - nl)
    ELSE
      line = rest
      rest = ""
    END IF
    line = TRIM(line)
    IF LEN(line) > 0 THEN
      DIM c1 AS INTEGER
      c1 = INSTR(line, ",")
      IF c1 > 1 THEN
        DIM nm AS STRING
        nm = LEFT(line, c1 - 1)
        DIM r2 AS STRING
        r2 = MID(line, c1 + 1, LEN(line) - c1)
        DIM c2 AS INTEGER
        c2 = INSTR(r2, ",")
        DIM pt AS STRING
        DIM st AS STRING
        IF c2 > 0 THEN
          pt = LEFT(r2, c2 - 1)
          st = MID(r2, c2 + 1, LEN(r2) - c2)
        ELSE
          pt = r2
          st = "?"
        END IF
        IF LEFT(nm, 1) = "*" THEN
          nm = MID(nm, 2, LEN(nm) - 1) + " (console)"
        END IF
        WINDOW Console PRINT "  " + LTRIM(STR(row + 1)) + "  " + LEFT(PADR(nm, 26), 26) + " " + LEFT(PADR(pt, 7), 7) + " " + LEFT(PADR(st, 6), 6)
        g_rowStat(row) = st
        g_nameOf(row) = nm
        g_portOf(row) = TRIM(pt)
        row = row + 1
      END IF
    END IF
  WEND
  g_rowCount = row
  g_drawn = 1
END SUB

' Aggiorna SOLO i campi "stato" cambiati, con LOCATE (nessun rewrite del contenuto).
SUB RefreshStatus
  DIM txt AS STRING
  txt = HTTP.GET(g_host + "/status.txt")
  DIM rest AS STRING
  rest = txt
  DIM row AS INTEGER
  row = 0
  WHILE LEN(rest) > 0
    DIM nl AS INTEGER
    nl = INSTR(rest, CHR(10))
    DIM line AS STRING
    IF nl > 0 THEN
      line = LEFT(rest, nl - 1)
      rest = MID(rest, nl + 1, LEN(rest) - nl)
    ELSE
      line = rest
      rest = ""
    END IF
    line = TRIM(line)
    IF LEN(line) > 0 THEN
      DIM c1 AS INTEGER
      c1 = INSTR(line, ",")
      IF c1 > 1 THEN
        DIM r2 AS STRING
        r2 = MID(line, c1 + 1, LEN(line) - c1)
        DIM c2 AS INTEGER
        c2 = INSTR(r2, ",")
        DIM st AS STRING
        IF c2 > 0 THEN
          st = MID(r2, c2 + 1, LEN(r2) - c2)
        ELSE
          st = "?"
        END IF
        IF row < g_rowCount THEN
          IF st <> g_rowStat(row) THEN
            g_rowStat(row) = st
            ' LOCATE e' WINDOW-RELATIVE (viewport): riga contenuto 6+row (1-based), colonna stato 38.
            LOCATE 6 + row, 41
            PRINT LEFT(PADR(st, 6), 6)
          END IF
        END IF
        row = row + 1
      END IF
    END IF
  WEND
END SUB

' Selezione (1..9): aggiorna marcatore riga + riga tasti (parziale).
SUB SetSel(n AS INTEGER)
  IF n < 1 OR n > g_rowCount THEN
    EXIT SUB
  END IF
  IF g_sel >= 1 AND g_sel <= g_rowCount THEN
    LOCATE 6 + (g_sel - 1), 3
    PRINT " "
  END IF
  g_sel = n
  LOCATE 6 + (g_sel - 1), 3
  PRINT ">"
  DIM ln AS STRING
  ln = "[a] avvia tutti  [f] ferma tutti  [s] stop sel  [t] start sel  [r] restart sel  [q] esci   sel: " + g_nameOf(g_sel - 1)
  LOCATE 2, 1
  PRINT LEFT(PADR(ln, 78), 78)
END SUB

' Azioni sul servizio selezionato.
SUB StopSel
  IF g_sel < 1 THEN
    EXIT SUB
  END IF
  DIM p AS STRING
  p = g_portOf(g_sel - 1)
  DIM b AS STRING
  b = HTTP.GET("http://localhost:" + p + "/control/stop")
  SLEEP 1200
  RefreshStatus()
END SUB
SUB StartSel
  IF g_sel < 1 THEN
    EXIT SUB
  END IF
  DIM b AS STRING
  b = HTTP.GET(g_host + "/control/start?svc=" + g_nameOf(g_sel - 1))
  SLEEP 2000
  RefreshStatus()
END SUB
SUB RestartSel
  IF g_sel < 1 THEN
    EXIT SUB
  END IF
  DIM b AS STRING
  b = HTTP.GET(g_host + "/control/restart?svc=" + g_nameOf(g_sel - 1))
  SLEEP 3000
  RefreshStatus()
END SUB

' Ferma i servizi (esclude il console: nome con "*").
SUB DoStopAll
  DIM txt AS STRING
  txt = HTTP.GET(g_host + "/status.txt")
  DIM rest AS STRING
  rest = txt
  WHILE LEN(rest) > 0
    DIM nl AS INTEGER
    nl = INSTR(rest, CHR(10))
    DIM line AS STRING
    IF nl > 0 THEN
      line = LEFT(rest, nl - 1)
      rest = MID(rest, nl + 1, LEN(rest) - nl)
    ELSE
      line = rest
      rest = ""
    END IF
    line = TRIM(line)
    IF LEN(line) > 0 AND LEFT(line, 1) <> "*" THEN
      DIM c1 AS INTEGER
      c1 = INSTR(line, ",")
      IF c1 > 1 THEN
        DIM r2 AS STRING
        r2 = MID(line, c1 + 1, LEN(line) - c1)
        DIM c2 AS INTEGER
        c2 = INSTR(r2, ",")
        DIM pt AS STRING
        IF c2 > 0 THEN
          pt = LEFT(r2, c2 - 1)
        ELSE
          pt = r2
        END IF
        DIM b AS STRING
        b = HTTP.GET("http://localhost:" + TRIM(pt) + "/control/stop")
      END IF
    END IF
  WEND
  SLEEP 1500
END SUB

' Avvia i servizi via il daemon (che usa il daemon manager).
SUB DoStartAll
  DIM r AS STRING
  r = HTTP.GET(g_host + "/control/start")
  SLEEP 2500
END SUB
