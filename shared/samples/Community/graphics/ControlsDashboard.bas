' Dashboard UI — griglia 8px, pannelli con GAP uniforme, 3 skin (soft/neon/glass).
IMPORT CONTROLS
IMPORT GRAPHICS
SCREEN 12, 1216, 800, 32
WINDOW THEME "soft"
FONT "font.ttf", 16
DIM n AS INTEGER, s AS STRING
DIM voci(3) AS STRING
voci(0) = "Overview" : voci(1) = "Reports" : voci(2) = "Alerts" : voci(3) = "Settings"
CLS 1

' ── pannelli (gap 16px, margini 32) ──
PANEL 32, 32, 240, 736, "NAVIGATION"
PANEL 288, 32, 288, 480, "MONITOR"
PANEL 592, 32, 288, 480, "DATA"
PANEL 896, 32, 288, 480, "RECORDS"
PANEL 288, 528, 896, 240, "ACTIVITY"

' ── rail ──
BIGTEXT 48, 72, "GMBASIC"
BUTTON 48, 128, 208, 48, "Overview"
BUTTON 48, 192, 208, 48, "Reports" STYLE "primary"
BUTTON 48, 256, 208, 48, "Alerts"
BUTTON 48, 320, 208, 48, "Settings"
BUTTON 48, 384, 208, 48, "Exit"
STATUSBAR 48, 696, 208, "Ready"

' ── MONITOR ──
PROGRESS 304, 72, 256, 40
SLIDER   304, 128, 256, 0, 100, 62 INTO n
TOGGLE   304, 192, 20, "Auto refresh" INTO n
CHECKBOX 304, 240, 20, "Remember me" INTO n
RADIO    304, 288, 256, voci INTO n
CHART    304, 400, 256, 96, "1,12;2,18;3,9;4,22;5,30;6,25;7,34"

' ── DATA ──
TAB      608, 72, 256, "Day|Week|Month" INTO n
COMBOBOX 608, 128, 256, voci INTO s
LISTBOX  608, 176, 256, 152, voci INTO n
CODEAREA 608, 344, 256, 152, "x = 1|y = 2|PRINT x + y" INTO s

' ── RECORDS ──
GRID     912, 72, 256, 144, "Name|Qty", "alpha|12;beta|7;gamma|3" INTO n
TREEVIEW 912, 232, 256, 120, "root/a|root/b|root/c" INTO n
DATALIST 912, 368, 256, 128, voci INTO n

' ── ACTIVITY ──
MENU    304, 560, 864, "File|Edit|View|Help"
LOGVIEW 304, 608, 424, 144, "started|ready|waiting"
HEXVIEW 744, 608, 424, 144, "474D4261736963"

DIM f AS INTEGER
FOR f = 1 TO 40 : WINDOW PUMP 16 : NEXT
SAVE SCREEN "work/controls_dashboard.png"
SCREEN 0
PRINT "OKRES"
