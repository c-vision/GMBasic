' OpsDashboard.bas — dashboard GRAFICA (tema glass): OGNI controllo ha un renderer PIXEL
' nativo (nessun blit di celle TUI). Grafica custom (accento, sparkline, LED) sopra il form.
IMPORT CONTROLS
IMPORT GRAPHICS

SCREEN 12, 1280, 860, 32
WINDOW THEME "glass"
FONT "font.ttf", 16
COLOR 15, 0
CLS 0

DIM n AS INTEGER, n2 AS INTEGER
DIM s AS STRING, s2 AS STRING
DIM i AS INTEGER
DIM sx AS INTEGER, sy AS INTEGER, px AS INTEGER, py AS INTEGER

' ── HEADER: barra-card + titolo, sottotitolo, menubar ─────────────────────
PANEL 0, 0, 1280, 68, ""
BIGTEXT 28, 12, "GMBASIC" STYLE "font=30"
LABEL 316, 34, 340, "CONTROL ROOM · live telemetry"
MENU 760, 26, 492, "Overview|Fleet|Events|Reports"

' ── KPI ROW (4 card, 84..252) ────────────────────────────────────────────
PANEL 28, 84, 294, 168, "REVENUE"
LABEL 44, 124, 200, "MRR · mese corrente"
BIGTEXT 44, 138, "48.2k" STYLE "font=30"
PROGRESS 44, 224, 262, 78

PANEL 340, 84, 294, 168, "LATENCY"
LABEL 356, 124, 200, "p95 · ultimi 5 min"
BIGTEXT 356, 138, "38ms" STYLE "font=30"
PROGRESS 356, 224, 262, 38

PANEL 652, 84, 294, 168, "THROUGHPUT"
LABEL 668, 124, 200, "req/s · picco"
BIGTEXT 668, 138, "2.4k" STYLE "font=30"
CHART 668, 190, 262, 62, "1,12;2,16;3,14;4,22;5,19;6,28;7,24;8,31;9,27;10,36"

PANEL 964, 84, 288, 168, "HEALTH"
LABEL 980, 124, 200, "servizi · 5 host"
STARRATING 980, 144, 200, 5, 4 INTO n
TOGGLE 980, 198, 250, "autoscale" INTO n2

' ── TRAFFIC + FLEET ──────────────────────────────────────────────────────
PANEL 28, 268, 600, 332, "TRAFFIC · 24H"
CHART 44, 300, 568, 232, "1,18;2,22;3,19;4,28;5,26;6,34;7,30;8,38;9,35;10,44;11,40;12,48"
FUNCTIONKEYBAR 558, 44, 568, "1d|1w|1m|1y"

PANEL 644, 268, 608, 332, "FLEET"
GRID 660, 300, 576, 168, "Node|Region|Load|Status", "alpha|eu-west|62%|ok;beta|us-east|48%|ok;gamma|ap-south|71%|warn;delta|eu-north|33%|ok;epsilon|us-west|89%|crit" INTO n
TAB 660, 474, 300, "All|Healthy|Degraded" INTO n
PROPERTYGRID 980, 474, 256, 64, "policy=rolling|window=5m"
BUTTON 660, 540, 180, 36, "restart"
BUTTON 852, 540, 180, 36, "drain"

' ── BOTTOM: eventi / hex / comandi ───────────────────────────────────────
PANEL 28, 616, 392, 176, "EVENTS"
LOGVIEW 44, 646, 360, 128, "boot ok|warmup done|cache primed|sync complete|replica attached"

PANEL 436, 616, 392, 176, "HEX"
HEXVIEW 452, 646, 360, 128, "474D4261736963204F70733A206F6E6C696E65"

PANEL 844, 616, 408, 176, "COMMAND"
COMBOBOX 860, 648, 180, "deploy|restart|scale|rollback" INTO s
NUMERICUPDOWN 1052, 648, 184, 1, 9, 3 INTO n
TEXTBOX 860, 690, 180, 34 INTO s2
BUTTON 860, 736, 180, 38, "RUN" STYLE "primary"
BUTTON 1052, 736, 184, 38, "CANCEL"

STATUSBAR 28, 800, 1224, "ready · 128 clients · build 4.2.1"

' ── Overlay grafico: ridisegnato a OGNI frame, altrimenti il redraw del form
'    (qualsiasi evento "dirty": mouse, hover, timer) lo cancella. ──────────
SUB PaintOverlay()
  DIM i AS INTEGER
  DIM sx AS INTEGER, sy AS INTEGER, px AS INTEGER, py AS INTEGER
  ' Accento superiore (4px) + filo sotto l'header: non coprono alcun controllo.
  LINE (0, 0)-(1279, 3), RGB(79, 195, 247), "BF"
  LINE (0, 68)-(1279, 68), RGB(28, 72, 130), "BF"

  ' Sparkline REVENUE (ciano).
  px = 44 : py = 210
  FOR i = 0 TO 20
    sx = 44 + i * 13
    sy = 210 - INT(8 * SIN(i * 0.7) + i * 0.45)
    IF i > 0 THEN LINE (px, py)-(sx, sy), RGB(79, 195, 247)
    px = sx : py = sy
  NEXT i

  ' Sparkline LATENCY (corallo).
  px = 356 : py = 210
  FOR i = 0 TO 20
    sx = 356 + i * 13
    sy = 210 - INT(5 * SIN(i * 0.5 + 1.4) + i * 0.5)
    IF i > 0 THEN LINE (px, py)-(sx, sy), RGB(255, 107, 129)
    px = sx : py = sy
  NEXT i

  ' LED di stato sulle quattro card.
  LINE (296, 100)-(303, 107), RGB(70, 220, 130), "BF"
  LINE (608, 100)-(615, 107), RGB(255, 200, 70), "BF"
  LINE (920, 100)-(927, 107), RGB(255, 107, 129), "BF"
  LINE (1232, 100)-(1239, 107), RGB(70, 220, 130), "BF"
END SUB

' ── frame: pump del form e poi l'overlay (sopra i controlli) ──────────────
FOR n = 1 TO 60
  WINDOW PUMP 16
  PaintOverlay()
NEXT

SAVE SCREEN "work/ops_dashboard.png"
SCREEN 0
PRINT "OPS-OK"
