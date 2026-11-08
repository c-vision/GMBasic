' Dashboard UI — layout PIXEL (griglia 8px: margini 24, gap 16, padding pannello 16).
' Nessuna metrica da cella: altezze widget 32/40, righe a passo 52, pannelli espliciti.
IMPORT CONTROLS
IMPORT GRAPHICS
SCREEN 12, 1216, 800, 32
WINDOW THEME "soft"
FONT "font.ttf", 16
DIM n AS INTEGER, s AS STRING
DIM voci(3) AS STRING
voci(0) = "Overview" : voci(1) = "Reports" : voci(2) = "Alerts" : voci(3) = "Settings"
CLS 1

' ── pannelli: margine 24, gap 16, riga/colonna da 296 ──
PANEL 24, 24, 232, 752, "NAVIGATION"
PANEL 272, 24, 296, 480, "MONITOR"
PANEL 584, 24, 296, 480, "DATA"
PANEL 896, 24, 296, 480, "RECORDS"
PANEL 272, 520, 920, 256, "ACTIVITY"

' ── rail: padding 16, bottoni 40px a passo 52 ──
BIGTEXT 40, 56, "GMBASIC"
BUTTON 40, 120, 200, 40, "Overview"
BUTTON 40, 172, 200, 40, "Reports" STYLE "primary"
BUTTON 40, 224, 200, 40, "Alerts"
BUTTON 40, 276, 200, 40, "Settings"
BUTTON 40, 328, 200, 40, "Exit"
STATUSBAR 40, 720, 200, "Ready"

' ── MONITOR: 32px di riga, gap 12 ──
PROGRESS 288, 72, 256, 12
SLIDER   288, 100, 256, 0, 100, 62 INTO n
TOGGLE   288, 140, 256, "Auto refresh" INTO n
CHECKBOX 288, 184, 256, "Remember me" INTO n
RADIO    288, 224, 256, voci INTO n
CHART    288, 372, 256, 108, "1,12;2,18;3,9;4,22;5,30;6,25;7,34"

' ── DATA ──
TAB      600, 72, 264, "Day|Week|Month" INTO n
COMBOBOX 600, 116, 264, voci INTO s
LISTBOX  600, 160, 264, 152, voci INTO n
CODEAREA 600, 324, 264, 156, "x = 1|y = 2|PRINT x + y" INTO s

' ── RECORDS ──
GRID     912, 72, 264, 144, "Name|Qty", "alpha|12;beta|7;gamma|3" INTO n
TREEVIEW 912, 228, 264, 120, "root/a|root/b|root/c" INTO n
DATALIST 912, 360, 264, 120, voci INTO n

' ── ACTIVITY ──
MENU    288, 552, 888, "File|Edit|View|Help"
LOGVIEW 288, 592, 440, 160, "started|ready|waiting"
HEXVIEW 744, 592, 440, 160, "474D4261736963"

DIM f AS INTEGER
FOR f = 1 TO 40 : WINDOW PUMP 16 : NEXT
SAVE SCREEN "work/controls_dashboard.png"
SCREEN 0
PRINT "OKRES"
