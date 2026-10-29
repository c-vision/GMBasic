IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 12, 1024, 768, 32
DIM m = 0
DIM tab = 0
DIM ok = 0
DIM tg = 0
DIM rad = 1
DIM val = 40
DIM spn = 5
DIM cb = 0
DIM lb = 2
DIM col = 0
DIM prg = 65
DIM sel = 0
DIM tree = 0
DIM dt AS STRING
DIM ff AS STRING
DIM nx = 0
DIM hh = -1
DIM st AS STRING

SUB Main()
    MENU 20, 10, "File|Modifica|Visualizza|Aiuto" INTO m
    TAB 20, 46, "Generale|Dettagli|Avanzate" INTO tab
    CHECKBOX 20, 96, "Abilita notifiche" INTO ok ID hh NAME "c_check"
    TOGGLE 300, 96, "Modalita' scura" INTO tg NAME "c_tog"
    RADIO 20, 130, "Basso|Medio|Alto" INTO rad NAME "c_rad"
    SLIDER 20, 268, 260, 0, 100, 5 INTO val NAME "c_sld"
    SPINNER 20, 300, 150, 0, 999, 1 INTO spn NAME "c_spn"
    COMBOBOX 20, 342, 260, "Alfa|Beta|Gamma" INTO cb NAME "c_cmb"
    LISTBOX 20, 382, 260, 130, "Uno|Due|Tre" INTO lb NAME "c_lst"
    COLORPICKER 300, 180 INTO col NAME "c_col"
    DATEPICKER 300, 300 INTO dt NAME "c_dat"
    PROGRESS 20, 700, 300 INTO prg NAME "c_prg"
    STATUSBAR 20, 740, 900, "Pronto" NAME "c_sb"
    GRID 560, 90, 420, 200, "Nome|Eta", "Anna|30;Bruno|25" INTO sel NAME "c_grd"
    TREEVIEW 560, 320, 320, 160, "Progetto/src|Progetto/src/main.c" INTO tree NAME "c_trv"
    FILEDIALOG 560, 500, 340, 160, "." INTO ff NAME "c_fdl"
    BUTTON 20, 600, 120, 32, "Ok" INTO nx NAME "c_btn"
    TEXTBOX 300, 420, 200, 28 INTO st NAME "c_txt"
    CANVAS 780, 640, 200, 100 NAME "c_cvs"
    IMAGEBUTTON 780, 300, 96, 32, "Salva" NAME "c_ibtn"
    QR 780, 120, 48, 48, "https://esempio.test" NAME "c_qr"
    RICHTEXT 780, 180, 200, 90, "Titolo|riga1|riga2" NAME "c_rt"
    HALFBLOCK 780, 380, 120, 40 NAME "c_hb"
    TILING 780, 440, 120, 40 NAME "c_tl"
    PANEL 780, 500, 120, 80 NAME "c_pnl"
    CONTROL GET "c_prg", "value" INTO prg
    CONTROL SET "c_prg", "value", 77
    CONTROL GET "c_prg", "value" INTO prg
    CONTROL SET "c_check", "visible", 1
    CONTROL GET "c_check", "visible" INTO ok
    CONTROL MOVE "c_btn", 20, 600, 120, 32
    CONTROL FOCUS "c_btn"
    IF hh >= 0 AND prg = 77 AND ok = 1 THEN CPRINT "gfx-ctls-ok" ELSE CPRINT "gfx-ctls-no"
END SUB
