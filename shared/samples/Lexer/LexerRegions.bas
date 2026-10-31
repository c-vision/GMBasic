' FILE: Lexer/LexerRegions.bas
' ======================================================================================================================
' GUIDA TEORICA: REGIONI DI CODICE (#REGION / #END REGION)
' ======================================================================================================================
' Le regioni permettono di organizzare il codice in sezioni logiche collassabili.
' Sono puramente estetiche e non influenzano la compilazione o l'esecuzione.
'
' 1. #REGION "nome"
'    Inizia una nuova regione con l'etichetta specificata. Il nome è una stringa
'    descrittiva tra virgolette.
'
' 2. #END REGION
'    Termina la regione corrente.
'
' Le regioni possono essere annidate.
' ======================================================================================================================

#REGION "Inizializzazione"
    CLS
    DIM app_name$ = "GM-Basic Region Demo"
    DIM version$ = "1.0.0"
    DIM author$ = "GM-Basic Team"

    PRINT "╔══════════════════════════════════════════════════════════════════════╗"
    PRINT "║ LexerRegions - Organizzazione del codice con #REGION               ║"
    PRINT "╚══════════════════════════════════════════════════════════════════════╝"
    PRINT
#END REGION

#REGION "Variabili Globali"
    DIM counter AS INTEGER
    DIM total AS DOUBLE
    DIM names$(10)
    DIM is_initialized AS BIT
#END REGION

#REGION "Funzioni di Utility"

    PUBLIC SUB InitApp()
        counter = 0
        total = 0.0
        is_initialized = 1
    END SUB

    PUBLIC SUB Increment(Optional delta AS INTEGER = 1)
        counter = counter + delta
        total = total + delta
    END SUB

#END REGION

#REGION "Test Principali"

    GOSUB TestBasicRegion
    GOSUB TestNestedRegions

    END

#END REGION

#REGION "Test Base"

    1000 TestBasicRegion:
    1010 PRINT "--- 1. Test Regione Base ---"
    1020 CALL InitApp
    1030 CALL Increment(5)
    1040 CALL Increment(10)
    1050 PRINT "  Counter:"; counter; " (Atteso: 15)"
    1060 PRINT "  Total:"; total; " (Atteso: 15.0)"
    1070 PRINT
    1080 RETURN

#END REGION

#REGION "Test Regioni Annidate"

    #REGION "Sotto-Test A"
        2000 TestNestedRegions:
        2010 PRINT "--- 2. Test Regioni Annidate ---"
    #END REGION

    #REGION "Sotto-Test B"
        2020 DIM results$(2)

        #REGION "Dati di Prova"
            2030 results$(0) = "OK"
            2040 results$(1) = "PASS"
            2050 results$(2) = "DONE"
        #END REGION

        2060 FOR i = 0 TO UBOUND(results$)
        2070     PRINT "  Test #"; i; ": "; results$(i)
        2080 NEXT
    #END REGION

    2090 PRINT
    2100 RETURN

#END REGION

#REGION "Pulizia e Chiusura"
    PRINT "Test completato."
#END REGION
