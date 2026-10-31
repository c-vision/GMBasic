' FILE: Graphics/ConsoleModeSelect.bas
' =================================================================================================
' TEXTMODE — selezione della modalita' di console a runtime + costanti %SDL/%C64/%ANSI/%SYSTEM
' =================================================================================================
' Dimostra:
'   - le costanti globali di modalita' console: %SDL=0, %C64=1, %ANSI=2, %SYSTEM=3;
'   - il comando TEXTMODE <modo> che seleziona la console e la ridisegna (applica
'     consoleConfiguration[modo]: font, palette, griglia, e in C64 il remap PETSCII);
'   - SCREEN 0 che chiude la finestra grafica (se aperta) tornando alla console.
'
' In ambiente headless (GMBASIC_HEADLESS=1) TEXTMODE e' un no-op su tutti i backend e il
' programma resta deterministico (stesso output su IL/PCodeVM/VM-C/C99Native).
' =================================================================================================

SUB Main()
    PRINT "=== ConsoleModeSelect ==="

    ' Costanti globali (valori canonici: 0/1/2/3).
    PRINT "%SDL    = "; %SDL
    PRINT "%C64    = "; %C64
    PRINT "%ANSI   = "; %ANSI
    PRINT "%SYSTEM = "; %SYSTEM

    ' Elenco delle modalita' e ciclo di selezione con TEXTMODE.
    DIM modes(3) AS INTEGER
    modes(0) = %SDL
    modes(1) = %C64
    modes(2) = %ANSI
    modes(3) = %SYSTEM

    DIM i AS INTEGER
    FOR i = 0 TO 3
        PRINT "TEXTMODE "; modes(i)
        TEXTMODE modes(i)          ' argomento da variabile (non solo letterale)
        SELECT CASE modes(i)       ' usa le costanti in SELECT CASE
            CASE %SDL
                PRINT "  -> SDL    : console grafica TUI (default)"
            CASE %C64
                PRINT "  -> C64    : palette VICE, griglia 40x25, font Sixtyfour (OFL)"
            CASE %ANSI
                PRINT "  -> ANSI   : terminale standard (escape ANSI)"
            CASE %SYSTEM
                PRINT "  -> SYSTEM : console di sistema"
            CASE ELSE
                PRINT "  -> ???    : modalita' sconosciuta"
        END SELECT
    NEXT i

    ' Ripristino la console grafica di default e chiudo l'eventuale finestra grafica.
    TEXTMODE %SDL
    SCREEN 0
    PRINT "Console ripristinata (SDL) e finestra grafica chiusa."
    PRINT "OK"
END SUB
