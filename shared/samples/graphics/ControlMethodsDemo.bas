' ControlMethodsDemo.bas — metodi a RUNTIME per nome (due token, come GMBasic):
'   CONTROL SET "nome", "prop", valore [, str$]
'   CONTROL GET "nome", "prop" INTO var
'   CONTROL GETS "nome", "prop" INTO var$
'   CONTROL MOVE "nome", x, y [, w, h]
'   CONTROL FOCUS "nome"
'   CONTROL ZORDER "nome", 1|0
IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 12
DIM stato = 1
DIM pos = 0
DIM cap = ""

SUB Main()
    WINDOW THEME "Monokai"
    CHECKBOX 40, 30, "Attiva" INTO stato NAME "chkAttiva"
    BUTTON 40, 80, 160, 40, "Premi" ON CLICK OnPremi NAME "btnPremi"
    GO SUB EventLoop()
END SUB

SUB OnPremi()
    CONTROL GET "btnPremi", "left" INTO pos
    CONTROL MOVE "btnPremi", pos + 20, 80
    CONTROL GETS "btnPremi", "caption" INTO cap
    PRINT cap; " @ "; pos
    CONTROL FOCUS "chkAttiva"
END SUB

SUB EventLoop()
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB
