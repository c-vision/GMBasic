' GuiControls.bas — modalita' grafica: controlli pixel col tema.
' I controlli pixel sono SELF-CONTAINED (mouse via gfx_input, valore in INTO var):
' NON toccano il Clipper READ/typeahead. La modalita' grafica rinvia i controlli
' keyword alla versione pixel (PIX<NAME>); GUI OFF torna ai controlli TUI/Clipper.
IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 12
DIM ok = 1
DIM n = 5
DIM tab = 0
DIM sel = 0
DIM col = 0

SUB Main()
    WINDOW THEME "Nord"
    TOGGLE 40, 30, "Abilitato" INTO ok
    SPINNER 40, 70, 180, 0, 100, 5 INTO n
    TAB 40, 120, "Generale|Avanzate|Info" INTO tab
    LISTBOX 40, 170, 300, 130, "Alpha|Beta|Gamma|Delta|Epsilon" INTO sel
    COLORPICKER 380, 30, "FF0000,00C000,3060FF,FFD400,FF00FF,00E5FF,FFFFFF,000000" INTO col
    STATUSBAR 40, 430, 560, "Pronto"
    GO SUB EventLoop()
END SUB

SUB EventLoop()
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB
