' FILE: Computer/ComputerType.bas
' =================================================================================================
' COMPUTER.TYPE(text$) — simula digitazione tastiera (Accessibilità richiesta su macOS)
' =================================================================================================
IMPORT COMPUTER

SUB Main()
    CLS
    PRINT "=== COMPUTER.TYPE ==="
    DIM result = COMPUTER.TYPE("hello world")
    PRINT "Result: "; result
    PRINT "=== OK ==="
END SUB