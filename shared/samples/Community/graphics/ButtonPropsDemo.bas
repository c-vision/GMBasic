' ButtonPropsDemo.bas — BUTTON è un Control: stesse proprietà base dei widget
' (Name/ToolTip/Tag/Visible/Enabled/TabIndex/TabStop/Focus) + STYLE + ON CLICK.
IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 12

SUB Main()
    WINDOW THEME "Monokai"
    BUTTON 40, 40, 160, 40, "Saluta" ON CLICK OnSaluta NAME "btnSaluta" TOOLTIP "Cliccami" TAG "toolbar" VISIBLE 1 ENABLED 1 TABINDEX 1 TABSTOP 1 FOCUS STYLE "rounded radius=10 emboss"
    BUTTON 40, 100, 160, 40, "Nascosto" ON CLICK OnNascosto NAME "btnHidden" VISIBLE 0
    GO SUB EventLoop()
END SUB

SUB OnSaluta()
    PRINT "Ciao!"
END SUB

SUB OnNascosto()
    PRINT "non dovresti vedermi"
END SUB

SUB EventLoop()
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB
