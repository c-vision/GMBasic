' PixelControlsGallery.bas — catalogo dei controlli GRAFICI con renderer PIXEL nativo
' (nessun blit di celle TUI). Tema glass, coordinate in pixel.
IMPORT CONTROLS
IMPORT GRAPHICS

SCREEN 12, 1280, 1200, 32
WINDOW THEME "glass"
COLOR 15, 0
CLS 0

DIM n AS INTEGER, n2 AS INTEGER
DIM s AS STRING, s2 AS STRING

PANEL 0, 0, 1280, 60, ""
BIGTEXT 24, 10, "CONTROL GALLERY" STYLE "font=30"
LABEL 24, 44, 700, "tutti i controlli pixel nativi · tema glass"
STATUSBAR 24, 1156, 1232, "38 controlli · renderer pixel nativo"

' ══ COLONNA 1 ═══════════════════════════════════════════════════════════
LABEL 24, 78, 280, "BUTTON / primary"
BUTTON 24, 98, 280, 38, "Deploy" STYLE "primary"
LABEL 24, 146, 280, "BUTTON"
BUTTON 24, 166, 280, 38, "Rollback"
LABEL 24, 214, 280, "CHECKBOX"
CHECKBOX 24, 234, 280, "Enable autoscaling" INTO n
LABEL 24, 274, 280, "TOGGLE"
TOGGLE 24, 294, 280, "Live updates" INTO n2
LABEL 24, 414, 280, "RADIO"
RADIO 24, 434, 280, "Fast|Balanced|Safe" INTO n
LABEL 24, 520, 280, "SLIDER"
SLIDER 24, 540, 280, 0, 100, 64 INTO n
LABEL 24, 580, 280, "PROGRESS"
PROGRESS 24, 600, 280, 72
LABEL 24, 640, 280, "NUMERICUPDOWN"
NUMERICUPDOWN 24, 660, 280, 1, 32, 1 INTO n

' ══ COLONNA 2 ═══════════════════════════════════════════════════════════
LABEL 340, 78, 280, "STARRATING"
STARRATING 340, 98, 280, 5, 4 INTO n
LABEL 340, 138, 280, "TEXTBOX"
TEXTBOX 340, 158, 280, 34 INTO s
LABEL 340, 204, 280, "COMBOBOX"
COMBOBOX 340, 224, 280, "deploy|restart|scale|rollback" INTO s
LABEL 340, 270, 280, "SPINNER"
SPINNER 340, 290, 280, "idle|busy|done" INTO n
LABEL 340, 336, 280, "TAB"
TAB 340, 356, 280, "Overview|Metrics|Logs" INTO n
LABEL 340, 402, 280, "MENU (menubar)"
MENU 340, 422, 280, "File|Edit|View|Help"
LABEL 340, 470, 280, "FUNCTIONKEYBAR"
FUNCTIONKEYBAR 1130, 340, 280, "F1|F2|F3|F4"
LABEL 340, 524, 280, "COLORPICKER"
COLORPICKER 340, 544, 280, 48 INTO n

' ══ COLONNA 3 ═══════════════════════════════════════════════════════════
LABEL 660, 78, 280, "GRID"
GRID 660, 98, 280, 168, "Node|Load|Status", "alpha|62%|ok;beta|48%|ok;gamma|71%|warn;delta|33%|ok;epsilon|89%|crit" INTO n
LABEL 660, 282, 280, "LISTBOX"
LISTBOX 660, 302, 280, 108, "Overview|Metrics|Logs|Alerts" INTO n
LABEL 660, 426, 280, "TREEVIEW"
TREEVIEW 660, 446, 280, 130, "root/api|root/api/v1|root/db|root/db/primary" INTO n

' ══ COLONNA 4 ═══════════════════════════════════════════════════════════
LABEL 980, 78, 280, "LOGVIEW"
LOGVIEW 980, 98, 280, 96, "boot ok|warmup done|cache primed"
LABEL 980, 210, 280, "HEXVIEW"
HEXVIEW 980, 230, 280, 80, "474D4261736963"
LABEL 980, 326, 280, "CODEAREA"
CODEAREA 980, 346, 280, 100, "x = 1|y = 2|PRINT x + y"
LABEL 980, 462, 280, "PROPERTYGRID"
PROPERTYGRID 980, 482, 280, 60, "policy=rolling|window=5m"
LABEL 980, 558, 280, "DATALIST"
DATALIST 980, 578, 280, 60, "restart|drain|scale" INTO n

' ══ RIGA CHARTS ═════════════════════════════════════════════════════════
LABEL 24, 730, 380, "CHART"
CHART 24, 750, 380, 190, "1,18;2,22;3,19;4,28;5,26;6,34;7,30;8,38;9,35;10,44"

LABEL 424, 730, 380, "CANDLESTICK"
CANDLESTICK 424, 750, 380, 190, "10,13,9,12;12,14,11,13;13,15,12,11;11,12,10,9;9,11,8,10;10,13,9,12"

LABEL 824, 730, 420, "RICHTEXT"
RICHTEXT 824, 750, 420, 90, "Deploy completato su 3 nodi|Nessun errore|Rollback disponibile"

LABEL 824, 860, 100, "QR"
QR 824, 880, 96, 96, "GMBASIC"

LABEL 940, 860, 200, "IMAGEBUTTON"
IMAGEBUTTON 940, 880, 160, 40, "Open"
LABEL 940, 930, 200, "HALFBLOCK"
HALFBLOCK 940, 950, 160, 30, "progress"

' ══ RIGA FINALE ═════════════════════════════════════════════════════════
LABEL 24, 960, 380, "DASHBOARD"
DASHBOARD 24, 980, 380, 110, "CPU|72|100|%|MEM|58|100|%|DISK|31|100|%" INTO n

LABEL 424, 960, 380, "SCROLLVIEW"
SCROLLVIEW 424, 980, 380, 110, "alpha|beta|gamma|delta|epsilon|zeta|eta|theta"

LABEL 824, 1000, 180, "SCROLLBAR"
SCROLLBAR 824, 1020, 180, 0, 100, 40 INTO n
LABEL 1030, 1000, 220, "SPLITTER"
SPLITTER 1030, 1024, 220, 20, 0

FOR n = 1 TO 60
  WINDOW PUMP 16
  LINE (0, 0)-(1279, 3), RGB(79, 195, 247), "BF"
NEXT

SAVE SCREEN "work/pixel_gallery.png"
SCREEN 0
PRINT "GALLERY-OK"
