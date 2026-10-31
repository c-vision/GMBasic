' FILE: Clipper/WindowTurboVision.bas
' Dimostrazione completa delle finestre TUI con stile Turbo Vision,
' ombre 3D reali, cornici differenziate (attiva/inattiva), scrollbar e cambio tema.

IMPORT WINDOWS

IMPORT CONTROLS
SUB Main()
    PRINT "=== WindowTurboVision Demo ==="

    ' 1. Verifica tema di default (Turbo)
    t$ = WINDOWS.THEME()
    PRINT "Tema iniziale: "; t$

    ' 2. Definizione finestra principale con bordo doppio e ombra Turbo Vision
    WINDOW DEFINE Editor FROM 2, 4 TO 16, 46 TITLE "Turbo Editor" DOUBLE SHADOW
    WINDOW Editor PRINT "Benvenuti nell'ambiente Turbo Vision!"
    WINDOW Editor PRINT "-------------------------------------"
    WINDOW Editor PRINT "Le finestre hanno bordi doppi [■] e [▲]."
    WINDOW Editor PRINT "L'ombra a destra e sotto dona profondita 3D."
    WINDOW Editor PRINT "Premiamo F6 o TAB per cambiare finestra."
    WINDOW Editor PRINT "Linea di contenuto aggiuntiva 1"
    WINDOW Editor PRINT "Linea di contenuto aggiuntiva 2"
    WINDOW Editor PRINT "Linea di contenuto aggiuntiva 3"
    WINDOW Editor PRINT "Linea di contenuto aggiuntiva 4"
    WINDOW Editor PRINT "Fine del documento."

    ' 3. Finestra secondaria sovrapposta (mostra l'effetto dell'ombra sul desktop/finestra)
    WINDOW DEFINE Info FROM 6, 32 TO 18, 74 TITLE "Stato & Temi" DOUBLE SHADOW
    WINDOW Info PRINT "Finestra Sovrapposta"
    WINDOW Info PRINT "===================="
    WINDOW Info PRINT "Supporta temi multipli:"
    WINDOW Info PRINT " - Turbo (Default Borland)"
    WINDOW Info PRINT " - QBasic (MS-DOS)"
    WINDOW Info PRINT " - Nord, Dracula, Monokai"
    WINDOW Info PRINT " - SolarizedDark, RetroGreen, Amber"

    ' 4. Seleziona e verifica
    WINDOW SELECT Editor
    PRINT "Finestra attiva: Editor"
    WINDOW SELECT Info
    PRINT "Finestra attiva: Info"

    ' 5. Test cambio tema
    WINDOW THEME "Nord"
    t_nord$ = WINDOWS.THEME()
    PRINT "Nuovo tema: "; t_nord$

    WINDOW THEME "Dracula"
    t_drac$ = WINDOWS.THEME()
    PRINT "Nuovo tema: "; t_drac$

    ' Ripristino tema predefinito Turbo Vision
    WINDOW THEME "Turbo"
    PRINT "Tema finale: "; WINDOWS.THEME()

    ' Ritorno a schermo base
    WINDOW SELECT 0
    STATUSBAR "F1 Aiuto  F2 Salva  F3 Carica  F10 Menu"
    PRINT "=== Demo Turbo Vision completata con successo ==="
END SUB
