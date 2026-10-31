' FILE: windows/WindowHeightWidth.bas
' =================================================================================================
' WINDOWS.WIDTH() / WINDOWS.HEIGHT() — dimensioni finestra attiva
' =================================================================================================
IMPORT WINDOWS

SUB Main()
    CLS
    PRINT "=== WINDOWS.WIDTH / HEIGHT ==="
    DIM w = WINDOWS.WIDTH()
    DIM h = WINDOWS.HEIGHT()
    PRINT "Width : "; w
    PRINT "Height: "; h
    PRINT "=== OK ==="
END SUB