' AllControls.bas — TUTTI i controlli pixel in una finestra grande, font piccolo.
IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 12, 1280, 860, 32
DIM m = 0
DIM tab = 0
DIM ok = 1
DIM tg = 0
DIM rad = 1
DIM val = 40
DIM spn = 5
DIM cb = 0
DIM lb = 2
DIM dd = 0
DIM col = 0
DIM prg = 65
DIM sel = 0
DIM tree = 0
DIM dt AS STRING
DIM ff AS STRING

SUB Main()
    WINDOW THEME "Turbo"
    MENU 20, 10, "File|Modifica|Visualizza|Aiuto" INTO m
    TAB 20, 46, "Generale|Dettagli|Avanzate" INTO tab
    CHECKBOX 20, 96, "Abilita notifiche" INTO ok
    TOGGLE 300, 96, "Modalita' scura" INTO tg
    RADIO 20, 130, "Basso|Medio|Alto" INTO rad
    SLIDER 20, 268, 260, 0, 100, 5 INTO val
    SPINNER 20, 300, 150, 0, 999, 1 INTO spn
    COMBOBOX 20, 342, 260, "Alfa|Beta|Gamma" INTO cb
    LISTBOX 20, 382, 260, 130, "Uno|Due|Tre|Quattro|Cinque" INTO lb
    COMBOBOX 20, 522, 260, "Rosso|Verde|Blu" INTO dd
    COLORPICKER 300, 180 INTO col
    DATEPICKER 300, 300 INTO dt
    PROGRESS 20, 700, 300 INTO prg
    STATUSBAR 20, 820, 1240, "Pronto"
    GRID 560, 90, 680, 260, "Nome|Eta|Citta", "Anna|30|Roma;Bruno|25|Milano;Carla|41|Napoli;Dino|19|Torino" INTO sel
    TREEVIEW 560, 380, 320, 220, "Progetto/src|Progetto/src/main.c|Progetto/include|Progetto/include/a.h" INTO tree
    FILEDIALOG 900, 380, 340, 220, "." INTO ff
    GO SUB EventLoop()
END SUB

SUB EventLoop()
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB
