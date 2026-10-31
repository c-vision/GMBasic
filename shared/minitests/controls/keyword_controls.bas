' MiniTest: controlli pixel a keyword (SCREEN n!=0) + CONTROL — 4 backend.
' EXPECT: kw-ok
IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 12
DIM n = 0
DIM h = -1
DIM sd AS STRING

SUB Main()
    TOGGLE 10, 10, "T" INTO n ID h NAME "k_tog"
    SPINNER 10, 40, "dots" INTO n NAME "k_spin"
    TAB 10, 80, 120, 20, "t1|t2" INTO n NAME "k_tab"
    LISTBOX 10, 110, 120, 60, "x|y" INTO n NAME "k_list"
    COLORPICKER 10, 180, 8 INTO n NAME "k_col"
    GRID 10, 200, 200, 80, "H1|H2", "a|b;c|d" INTO n NAME "k_grid"
    TREEVIEW 10, 290, 200, 80, "r|r/1" INTO n NAME "k_tree"
    STATUSBAR "st"
    MENU 10, 400, "F|E" INTO n NAME "k_menu"
    DATEPICKER 10, 420 INTO sd NAME "k_date"
    IF h >= 0 THEN CPRINT "kw-ok" ELSE CPRINT "kw-no"
END SUB
