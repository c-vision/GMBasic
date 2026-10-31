' FILE: Console/ConsoleSaveRestoreScreen.bas
' =================================================================================================
'       GUIDA TEORICA: SAVE SCREEN / RESTORE SCREEN (PORTABILE IN GMBASICAI)
' =================================================================================================
'
' SAVE SCREEN e RESTORE SCREEN sono comandi polimorfici: salvano/ripristinano il buffer della
' console testuale (GMBasicAI: solo testo). La variante forzata CSAVESCREEN/CRESTORESCREEN
' (solo console, anche con schermo grafico attivo) è in Graphics/GraphicsConsoleModes.bas
' perché non portabile in GMBasicAI.
'
' I comandi operano sul buffer a griglia: StandardConsole si apre di default in modalita'
' NATIVE (nessun buffer), quindi va attivata la modalita' BUFFERED prima di usarli.
'
' =================================================================================================

SCROLLMODE "BUFFERED"

SUB Main()
    CLS
    LOCATE 1, 1
    PRINT "Schermata originale - verra' salvata con SAVE SCREEN."
    LOCATE 3, 1
    PRINT "Riga 3 originale."

    SAVE SCREEN "principale"

    CLS
    LOCATE 1, 1
    PRINT "Schermata temporanea (es. un menu) - verra' scartata."
    LOCATE 3, 1
    PRINT "Questa riga sparira' dopo RESTORE SCREEN."

    RESTORE SCREEN "principale"

    LOCATE 18, 1
    PRINT "Schermata originale ripristinata con successo (SAVE/RESTORE SCREEN)."
    PRINT
    PRINT "Test completato."
END SUB
