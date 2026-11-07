' TabsDashboard.bas — dashboard GRAFICA con tutte le famiglie di controlli su 5 schede.
' Ogni controllo ha NAME "p<Pagina>_<lettera>"; ShowPage() accende solo la pagina attiva
' (CONTROL SET name,"visible",0/1). Ogni scheda è salvata in work/tabN.png.
IMPORT CONTROLS
IMPORT GRAPHICS

SCREEN 12, 1280, 940, 32
WINDOW THEME "glass"
FONT "font.ttf", 16
COLOR 15, 0
CLS 0

DIM n AS INTEGER, n2 AS INTEGER, i AS INTEGER
DIM s AS STRING, s2 AS STRING

DIM suf(12) AS STRING
suf(0) = "a" : suf(1) = "b" : suf(2) = "c" : suf(3) = "d"
suf(4) = "e" : suf(5) = "f" : suf(6) = "g" : suf(7) = "h"
suf(8) = "i" : suf(9) = "j" : suf(10) = "k" : suf(11) = "l"

' ── cornice fissa ────────────────────────────────────────────────────────
PANEL 0, 0, 1280, 62, ""
BIGTEXT 24, 12, "GMBASIC · CONSOLE" STYLE "font=26"
LABEL 24, 44, 640, "tutte le famiglie di controlli · tema glass"
TAB 24, 74, 1100, "Overview|Data|Files & Print|Clipper|Charts" INTO n NAME "tabs"
STATUSBAR 24, 896, 1232, "ready · 5 schede · 41 controlli"

' ══ PAGINA 1 · OVERVIEW (a-l) ═══════════════════════════════════════════
PANEL 24, 126, 600, 360, "KPI" NAME "p1_a"
LABEL 40, 162, 300, "MRR · mese corrente" NAME "p1_b"
BIGTEXT 40, 180, "48.2k" STYLE "font=30" NAME "p1_c"
PROGRESS 40, 262, 568, 78 NAME "p1_d"
LABEL 40, 300, 300, "throughput · req/s" NAME "p1_e"
BIGTEXT 40, 318, "2.4k" STYLE "font=30" NAME "p1_f"
PROGRESS 40, 400, 568, 64 NAME "p1_g"
PANEL 644, 126, 612, 360, "CONTROLLI" NAME "p1_h"
SLIDER 660, 182, 580, 0, 100, 64 INTO n NAME "p1_i"
TOGGLE 660, 242, 280, "live updates" INTO n2 NAME "p1_j"
CHECKBOX 660, 302, 280, "autoscaling" INTO n NAME "p1_k"
RADIO 660, 362, 580, "Fast|Balanced|Safe" INTO n NAME "p1_l"

' ══ PAGINA 2 · DATA (a-l) ═══════════════════════════════════════════════
PANEL 24, 126, 600, 274, "RECORDS" NAME "p2_a"
GRID 40, 158, 568, 154, "Node|Region|Load|Status", "alpha|eu-west|62%|ok;beta|us-east|48%|ok;gamma|ap-south|71%|warn;delta|eu-north|33%|ok;epsilon|us-west|89%|crit" INTO n NAME "p2_b"
LISTBOX 40, 320, 280, 72, "Overview|Metrics|Logs" INTO n NAME "p2_c"
PANEL 644, 126, 300, 260, "TREE" NAME "p2_d"
TREEVIEW 660, 158, 268, 210, "root/api|root/api/v1|root/db|root/db/primary" INTO n NAME "p2_e"
PANEL 960, 126, 296, 260, "FILTERS" NAME "p2_f"
COMBOBOX 976, 178, 264, "deploy|restart|scale|rollback" INTO s NAME "p2_g"
DATALIST 976, 242, 264, 60, "restart|drain|scale" INTO n NAME "p2_h"
PANEL 24, 402, 600, 240, "SCROLLVIEW" NAME "p2_i"
SCROLLVIEW 40, 434, 568, 190, "alpha|beta|gamma|delta|epsilon|zeta|eta|theta|iota|kappa" NAME "p2_j"
SCROLLBAR 660, 456, 260, 0, 100, 40 INTO n NAME "p2_k"
PROPERTYGRID 976, 434, 264, 60, "policy=rolling|window=5m" NAME "p2_l"

' ══ PAGINA 3 · FILES & PRINT (a-l) ══════════════════════════════════════
PANEL 24, 126, 600, 150, "FILES" NAME "p3_a"
FILEEXPLORER 40, 158, 568, 104, "." NAME "p3_b"
FILEDIALOG "*.bas", "Apri", "*.bas" NAME "p3_c"
CONTROL SET "p3_c", "left", 40
CONTROL SET "p3_c", "top", 300
PANEL 644, 126, 612, 150, "PRINT / MEDIA" NAME "p3_d"
PRINTERDIALOG "Stampa", s, n, n2 NAME "p3_e"
CONTROL SET "p3_e", "left", 700
CONTROL SET "p3_e", "top", 300
IMAGEBUTTON 660, 180, 260, 40, "Open file" NAME "p3_f"
TEXTBOX 980, 180, 260, 34 INTO s2 NAME "p3_g"






' ══ PAGINA 4 · CLIPPER (a-l) ════════════════════════════════════════════
PANEL 24, 126, 700, 360, "TBROWSE - @SELECT (Clipper)" NAME "p4_a"
TBROWSE 40, 168, 668, 292, [["Name", 22], ["Qty", 8], ["Status", 12]], [["alpha", 12, "ok"], ["beta", 7, "ok"], ["gamma", 3, "warn"], ["delta", 21, "ok"], ["epsilon", 9, "crit"], ["zeta", 15, "ok"], ["eta", 4, "ok"]] INTO n NAME "p4_b"

PANEL 744, 126, 512, 230, "CLIPPER - @SAY / @GET / MENU" NAME "p4_c"
LABEL 764, 166, 470, "Cliente: Mario Rossi   Saldo 1.240,00" NAME "p4_d"
TEXTBOX 764, 204, 470, 34 INTO s NAME "p4_e"
MENU 764, 254, 470, "File|Edit|View|Help" NAME "p4_f"
CONTEXTMENU 764, 298, 300, "Copy|Paste|Delete" NAME "p4_g"

PANEL 744, 372, 512, 114, "DIALOGHI" NAME "p4_h"
ALERTBOX 764, 402, 220, 70, "Deploy completato" NAME "p4_i"
CONFIRMATIONBOX 1000, 402, 236, 70, "Procedere?" NAME "p4_j"

PANEL 24, 502, 1232, 130, "TABLE MODEL + DIALOGHI (pixel, nessuna cella)" NAME "p4_l"
LABEL 44, 540, 1180, "TBROWSE legge il datasource colonna per colonna e compone le celle in pixel" NAME "p4_k"

' ══ PAGINA 5 · CHARTS (a-l) ═════════════════════════════════════════════
PANEL 24, 126, 600, 360, "LINE CHART" NAME "p5_a"
CHART 40, 158, 568, 310, "1,18;2,22;3,19;4,28;5,26;6,34;7,30;8,38;9,35;10,44;11,40;12,48" NAME "p5_b"
PANEL 644, 126, 612, 360, "CANDLESTICK" NAME "p5_c"
CANDLESTICK 660, 158, 580, 200, "10,13,9,12;12,14,11,13;13,15,12,11;11,12,10,9;9,11,8,10;10,13,9,12" NAME "p5_d"
DASHBOARD 660, 392, 580, 78, "CPU|72|100|%|MEM|58|100|%" INTO n NAME "p5_e"
PANEL 24, 502, 600, 240, "LOGVIEW" NAME "p5_f"
LOGVIEW 40, 534, 568, 190, "boot ok|warmup done|cache primed|sync complete" NAME "p5_g"
PANEL 644, 502, 300, 240, "HEXVIEW" NAME "p5_h"
HEXVIEW 660, 534, 268, 190, "474D4261736963204F70733A206F6E6C696E65" NAME "p5_i"
PANEL 960, 502, 296, 240, "CODEAREA" NAME "p5_j"
CODEAREA 976, 534, 264, 190, "x = 1|y = 2|PRINT x + y" NAME "p5_k"
LABEL 24, 760, 600, "chart · candlestick · dashboard · logview · hexview · codearea" NAME "p5_l"

' ── commutazione pagina ──────────────────────────────────────────────────
SUB ShowPage(p AS INTEGER)
  IF p = 1 THEN CONTROL SET "p1_a", "visible", 1 ELSE CONTROL SET "p1_a", "visible", 0
  IF p = 1 THEN CONTROL SET "p1_b", "visible", 1 ELSE CONTROL SET "p1_b", "visible", 0
  IF p = 1 THEN CONTROL SET "p1_c", "visible", 1 ELSE CONTROL SET "p1_c", "visible", 0
  IF p = 1 THEN CONTROL SET "p1_d", "visible", 1 ELSE CONTROL SET "p1_d", "visible", 0
  IF p = 1 THEN CONTROL SET "p1_e", "visible", 1 ELSE CONTROL SET "p1_e", "visible", 0
  IF p = 1 THEN CONTROL SET "p1_f", "visible", 1 ELSE CONTROL SET "p1_f", "visible", 0
  IF p = 1 THEN CONTROL SET "p1_g", "visible", 1 ELSE CONTROL SET "p1_g", "visible", 0
  IF p = 1 THEN CONTROL SET "p1_h", "visible", 1 ELSE CONTROL SET "p1_h", "visible", 0
  IF p = 1 THEN CONTROL SET "p1_i", "visible", 1 ELSE CONTROL SET "p1_i", "visible", 0
  IF p = 1 THEN CONTROL SET "p1_j", "visible", 1 ELSE CONTROL SET "p1_j", "visible", 0
  IF p = 1 THEN CONTROL SET "p1_k", "visible", 1 ELSE CONTROL SET "p1_k", "visible", 0
  IF p = 1 THEN CONTROL SET "p1_l", "visible", 1 ELSE CONTROL SET "p1_l", "visible", 0
  IF p = 2 THEN CONTROL SET "p2_a", "visible", 1 ELSE CONTROL SET "p2_a", "visible", 0
  IF p = 2 THEN CONTROL SET "p2_b", "visible", 1 ELSE CONTROL SET "p2_b", "visible", 0
  IF p = 2 THEN CONTROL SET "p2_c", "visible", 1 ELSE CONTROL SET "p2_c", "visible", 0
  IF p = 2 THEN CONTROL SET "p2_d", "visible", 1 ELSE CONTROL SET "p2_d", "visible", 0
  IF p = 2 THEN CONTROL SET "p2_e", "visible", 1 ELSE CONTROL SET "p2_e", "visible", 0
  IF p = 2 THEN CONTROL SET "p2_f", "visible", 1 ELSE CONTROL SET "p2_f", "visible", 0
  IF p = 2 THEN CONTROL SET "p2_g", "visible", 1 ELSE CONTROL SET "p2_g", "visible", 0
  IF p = 2 THEN CONTROL SET "p2_h", "visible", 1 ELSE CONTROL SET "p2_h", "visible", 0
  IF p = 2 THEN CONTROL SET "p2_i", "visible", 1 ELSE CONTROL SET "p2_i", "visible", 0
  IF p = 2 THEN CONTROL SET "p2_j", "visible", 1 ELSE CONTROL SET "p2_j", "visible", 0
  IF p = 2 THEN CONTROL SET "p2_k", "visible", 1 ELSE CONTROL SET "p2_k", "visible", 0
  IF p = 2 THEN CONTROL SET "p2_l", "visible", 1 ELSE CONTROL SET "p2_l", "visible", 0
  IF p = 3 THEN CONTROL SET "p3_a", "visible", 1 ELSE CONTROL SET "p3_a", "visible", 0
  IF p = 3 THEN CONTROL SET "p3_b", "visible", 1 ELSE CONTROL SET "p3_b", "visible", 0
  IF p = 3 THEN CONTROL SET "p3_c", "visible", 1 ELSE CONTROL SET "p3_c", "visible", 0
  IF p = 3 THEN CONTROL SET "p3_d", "visible", 1 ELSE CONTROL SET "p3_d", "visible", 0
  IF p = 3 THEN CONTROL SET "p3_e", "visible", 1 ELSE CONTROL SET "p3_e", "visible", 0
  IF p = 3 THEN CONTROL SET "p3_f", "visible", 1 ELSE CONTROL SET "p3_f", "visible", 0
  IF p = 3 THEN CONTROL SET "p3_g", "visible", 1 ELSE CONTROL SET "p3_g", "visible", 0
  IF p = 3 THEN CONTROL SET "p3_h", "visible", 1 ELSE CONTROL SET "p3_h", "visible", 0
  IF p = 3 THEN CONTROL SET "p3_i", "visible", 1 ELSE CONTROL SET "p3_i", "visible", 0
  IF p = 3 THEN CONTROL SET "p3_j", "visible", 1 ELSE CONTROL SET "p3_j", "visible", 0
  IF p = 3 THEN CONTROL SET "p3_k", "visible", 1 ELSE CONTROL SET "p3_k", "visible", 0
  IF p = 3 THEN CONTROL SET "p3_l", "visible", 1 ELSE CONTROL SET "p3_l", "visible", 0
  IF p = 4 THEN CONTROL SET "p4_a", "visible", 1 ELSE CONTROL SET "p4_a", "visible", 0
  IF p = 4 THEN CONTROL SET "p4_b", "visible", 1 ELSE CONTROL SET "p4_b", "visible", 0
  IF p = 4 THEN CONTROL SET "p4_c", "visible", 1 ELSE CONTROL SET "p4_c", "visible", 0
  IF p = 4 THEN CONTROL SET "p4_d", "visible", 1 ELSE CONTROL SET "p4_d", "visible", 0
  IF p = 4 THEN CONTROL SET "p4_e", "visible", 1 ELSE CONTROL SET "p4_e", "visible", 0
  IF p = 4 THEN CONTROL SET "p4_f", "visible", 1 ELSE CONTROL SET "p4_f", "visible", 0
  IF p = 4 THEN CONTROL SET "p4_g", "visible", 1 ELSE CONTROL SET "p4_g", "visible", 0
  IF p = 4 THEN CONTROL SET "p4_h", "visible", 1 ELSE CONTROL SET "p4_h", "visible", 0
  IF p = 4 THEN CONTROL SET "p4_i", "visible", 1 ELSE CONTROL SET "p4_i", "visible", 0
  IF p = 4 THEN CONTROL SET "p4_j", "visible", 1 ELSE CONTROL SET "p4_j", "visible", 0
  IF p = 4 THEN CONTROL SET "p4_k", "visible", 1 ELSE CONTROL SET "p4_k", "visible", 0
  IF p = 4 THEN CONTROL SET "p4_l", "visible", 1 ELSE CONTROL SET "p4_l", "visible", 0
  IF p = 5 THEN CONTROL SET "p5_a", "visible", 1 ELSE CONTROL SET "p5_a", "visible", 0
  IF p = 5 THEN CONTROL SET "p5_b", "visible", 1 ELSE CONTROL SET "p5_b", "visible", 0
  IF p = 5 THEN CONTROL SET "p5_c", "visible", 1 ELSE CONTROL SET "p5_c", "visible", 0
  IF p = 5 THEN CONTROL SET "p5_d", "visible", 1 ELSE CONTROL SET "p5_d", "visible", 0
  IF p = 5 THEN CONTROL SET "p5_e", "visible", 1 ELSE CONTROL SET "p5_e", "visible", 0
  IF p = 5 THEN CONTROL SET "p5_f", "visible", 1 ELSE CONTROL SET "p5_f", "visible", 0
  IF p = 5 THEN CONTROL SET "p5_g", "visible", 1 ELSE CONTROL SET "p5_g", "visible", 0
  IF p = 5 THEN CONTROL SET "p5_h", "visible", 1 ELSE CONTROL SET "p5_h", "visible", 0
  IF p = 5 THEN CONTROL SET "p5_i", "visible", 1 ELSE CONTROL SET "p5_i", "visible", 0
  IF p = 5 THEN CONTROL SET "p5_j", "visible", 1 ELSE CONTROL SET "p5_j", "visible", 0
  IF p = 5 THEN CONTROL SET "p5_k", "visible", 1 ELSE CONTROL SET "p5_k", "visible", 0
  IF p = 5 THEN CONTROL SET "p5_l", "visible", 1 ELSE CONTROL SET "p5_l", "visible", 0
  CONTROL SET "tabs", "value", p - 1
END SUB

' ── 5 schede, ognuna salvata ─────────────────────────────────────────────
FOR i = 1 TO 5
  ShowPage(i)
  FOR n = 1 TO 24 : WINDOW PUMP 16 : NEXT
  IF i = 1 THEN SAVE SCREEN "work/tab1.png"
  IF i = 2 THEN SAVE SCREEN "work/tab2.png"
  IF i = 3 THEN SAVE SCREEN "work/tab3.png"
  IF i = 4 THEN SAVE SCREEN "work/tab4.png"
  IF i = 5 THEN SAVE SCREEN "work/tab5.png"
NEXT i

SCREEN 0
PRINT "TABS-OK"
