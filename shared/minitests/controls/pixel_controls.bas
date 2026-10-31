' MiniTest: controlli pixel (lexeme) + proprietà + eventi + CONTROL + FORM_* — 4 backend.
' EXPECT: px-ok
IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 0
DIM c1 = 0
DIM c2 = 0
DIM c3 = 0
DIM c4 = 0
DIM c5 = 0
DIM s AS STRING
DIM h1 = -1

SUB Main()
    CHECKBOX 10, 10, "A" INTO c1 ID h1 NAME "k_check" VISIBLE 1 ENABLED 1 TABINDEX 1 TABSTOP 1 TAG "g" TOOLTIP "t"
    RADIO 10, 30, "a|b|c" INTO c2 NAME "k_radio"
    SLIDER 10, 50, 120, 0, 100, 5 INTO c3 NAME "k_track"
    COMBOBOX 10, 70, 120, "x|y|z" INTO c4 NAME "k_drop"
    PROGRESS 10, 90, 120 INTO c5 NAME "k_prog"
    TEXTBOX 10, 110, 200, 28 INTO s NAME "k_text"
    BUTTON 10, 150, 100, 30, "Ok" ON CLICK OnBtn NAME "k_btn" STYLE "rounded radius=8"
    CONTROL SET "k_prog", "value", 42
    CONTROL GET "k_prog", "value" INTO c5
    CONTROL GETS "k_text", "text" INTO s
    CONTROL MOVE "k_btn", 20, 20, 100, 30
    CONTROL FOCUS "k_btn"
    CONTROL ZORDER "k_btn", 1
    IF h1 >= 0 AND c5 = 42 THEN PRINT "px-ok" ELSE PRINT "px-no"
END SUB

SUB OnBtn()
END SUB
