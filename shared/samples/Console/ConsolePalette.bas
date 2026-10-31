' FILE: Console/ConsolePalette.bas
' =================================================================================================
' TEST UNITARIO: PALETTE CONSOLE (PORTABILE IN GMBASICAI)
' =================================================================================================
' Usa PALETTE (senza prefisso C*): in GMBasicAI la palette console è `PALETTE` con gli stessi
' subcomandi (USE/DEFINE/REPLACE/SAVE/LOAD). La variante forzata CPALETTE (solo console) è in
' Graphics/GraphicsConsoleModes.bas perché non portabile.
' =================================================================================================

IMPORT GRAPHICS

SUB Main()
    CLS
    PRINT "PALETTE test"
    PALETTE USE "Console"
    PALETTE 5, 255, 200, 0

    DIM arr(15)
    arr(0) = 0xFF0000: arr(1) = 0x00FF00: arr(2) = 0x0000FF
    arr(3) = 0xFFFF00: arr(4) = 0xFF00FF: arr(5) = 0x00FFFF
    arr(6) = 0x800000: arr(7) = 0x008000: arr(8) = 0x000080
    arr(9) = 0x808000: arr(10) = 0x800080: arr(11) = 0x008080
    arr(12) = 0xC0C0C0: arr(13) = 0x808080: arr(14) = 0xFF8800
    arr(15) = 0xFFFFFF

    PALETTE DEFINE "MyTest", arr()
    PALETTE USE "MyTest"
    PALETTE SAVE "cp_test.json"

    DIM arr2(15)
    arr2(0) = 0x000000: arr2(1) = 0x000000: arr2(2) = 0x000000
    arr2(3) = 0x000000: arr2(4) = 0x000000: arr2(5) = 0x000000
    arr2(6) = 0x000000: arr2(7) = 0x000000: arr2(8) = 0x000000
    arr2(9) = 0x000000: arr2(10) = 0x000000: arr2(11) = 0x000000
    arr2(12) = 0x000000: arr2(13) = 0x000000: arr2(14) = 0x000000
    arr2(15) = 0xFFFFFF

    PALETTE REPLACE "MyTest", arr2()
    PALETTE LOAD "cp_test.json"
    PALETTE USE "Nord"
    PRINT "DONE"
END SUB
