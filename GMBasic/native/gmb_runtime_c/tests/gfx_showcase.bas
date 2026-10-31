' Vetrina controlli (coordinate PIXEL: su schermo grafico il backend e' pixel).
IMPORT CONTROLS
IMPORT GRAPHICS
SCREEN 12, 1040, 620, 32
WINDOW THEME "glass"
FONT "font.ttf", 16
CLS 1
DIM n AS INTEGER, s AS STRING
DIM voci(2) AS STRING
voci(0) = "Alfa" : voci(1) = "Beta" : voci(2) = "Gamma"
' colonna sinistra
BUTTON 30, 30, 240, 52, "HOME"
BUTTON 30, 100, 240, 52, "ACCEPT" STYLE "bg=#F6A800 fg=white"
TOGGLE 30, 175, 20, "Wi-Fi" INTO n
CHECKBOX 30, 220, 20, "Ricordami" INTO n
RADIO 30, 260, 240, voci INTO n
PROGRESS 30, 360, 240, 60
STATUSBAR 30, 440, 460, "status ok"
' colonna centrale
SLIDER 300, 45, 240, 0, 100, 35 INTO n
TAB 300, 100, 330, "Uno|Due|Tre" INTO n
COMBOBOX 300, 160, 240, voci INTO s
LISTBOX 300, 210, 240, 130, voci INTO n
CODEAREA 300, 360, 270, 160, "x = 1|y = 2|PRINT x + y" INTO s
' colonna destra
GRID 580, 30, 300, 150, "Nome|Qta", "a|1;b|2;c|3" INTO n
TREEVIEW 580, 195, 300, 120, "root/a|root/b|root/c" INTO n
DATALIST 580, 330, 300, 150, voci INTO n
BIGTEXT 900, 490, "BIG"
MENU 580, 560, 300, "File|Modifica|Aiuto"
LOGVIEW 900, 30, 120, 120, "r1|r2|r3"
HEXVIEW 900, 170, 120, 120, "4142434445464748"
DIM f AS INTEGER
FOR f = 1 TO 30 : WINDOW PUMP 16 : NEXT
SAVE SCREEN "/tmp/fin_glass.png"
SCREEN 0
PRINT "OKRES"
