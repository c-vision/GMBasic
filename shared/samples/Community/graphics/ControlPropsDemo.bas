' ControlPropsDemo.bas — proprietà base VB dei controlli (Name/ToolTip/Tag/
' Visible/Enabled/TabIndex/TabStop) + SetFocus, fuse nella clausola (come STYLE).
'   CHECK ... INTO v NAME "chk" TOOLTIP "..." TAG "grp" VISIBLE 1 ENABLED 1 TABINDEX 1 TABSTOP 1 FOCUS
IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 12
DIM abilita = 1
DIM volume = 40
DIM nome = ""

SUB Main()
    WINDOW THEME "Monokai"
    CHECKBOX 40, 30, "Abilita notifiche" INTO abilita NAME "chkAbilita" TOOLTIP "Attiva o disattiva" TAG "grp" VISIBLE 1 ENABLED 1 TABINDEX 1 TABSTOP 1 FOCUS ON CHANGE OnAbilita
    SLIDER 40, 80, 300, 0, 100 INTO volume NAME "sldVol" TAG "grp" TABINDEX 2 TABSTOP 1 STYLE "rounded radius=8"
    TEXTBOX 40, 130, 300, 28 INTO nome NAME "txtNome" TOOLTIP "Scrivi qui" TABINDEX 3 TABSTOP 1 STYLE "rounded radius=6"
    GO SUB EventLoop()
END SUB

SUB OnAbilita()
    PRINT "Abilita = "; abilita
END SUB

SUB EventLoop()
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB
