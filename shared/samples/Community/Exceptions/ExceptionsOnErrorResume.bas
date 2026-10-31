' FILE: Exceptions/ExceptionsOnErrorResume.bas
' =================================================================================================
'       GUIDA TEORICA: GESTIONE ERRORI CLASSICA (ON ERROR, RESUME)
' =================================================================================================
'
' ON ERROR GOTO / RESUME: gestione errori stile BASIC classico.
'
' MODIFICATO: wrapped in SUB Main per compatibilità GPM.
' =================================================================================================

SUB Main()
    CLS
    PRINT "╔══════════════════════════════════════════════════════════════╗"
    PRINT "║ ExceptionsOnErrorResume - ON ERROR/RESUME                    ║"
    PRINT "╚══════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- SCENARIO 1: ON ERROR GOTO con RESUME ---
    PRINT "--- 1. ON ERROR GOTO + RESUME CON LABEL ---"
    ON ERROR GOTO gestore_1
    LET A = 10
    LET B = 0
    PRINT "Provo a dividere per zero..."
    LET C = A / B
    PRINT "Risultato:"; C
    GOTO sezione_2

gestore_1:
    PRINT ">> Errore intercettato (GOTO a label)"
    PRINT "Codice ERR ="; ERR.Number; ", riga ERL ="; ERR.LineNumber
    RESUME NEXT

    ' --- SCENARIO 2: ON ERROR GOTO con RESUME NEXT ---
sezione_2:
    PRINT
    PRINT "--- 2. ON ERROR GOTO + RESUME NEXT ---"
    ON ERROR GOTO gestore_2
    DIM X(2)
    FOR I = 0 TO 3
        PRINT "Accesso a X("; I; ")"
        X(I) = I * 2
    NEXT I
    PRINT "Fine ciclo FOR (dopo RESUME NEXT)"
    GOTO sezione_3

gestore_2:
    PRINT ">> ERRORE: "; ERR.Description
    PRINT "   Numero:"; ERR.Number; " Linea:"; ERR.LineNumber
    RESUME NEXT

    ' --- SCENARIO 3: ON ERROR GOTO 0 (disabilita) ---
sezione_3:
    PRINT
    PRINT "--- 3. ON ERROR GOTO 0 (disabilita) ---"
    ON ERROR GOTO 0
    PRINT "Provo un accesso lecito a X(0) ="; X(0)

    ' --- SCENARIO 4: RESET con ON ERROR GOTO 0 ---
    PRINT
    PRINT "--- 4. RESET con ON ERROR GOTO 0 ---"
    ON ERROR GOTO gestore_3
    LET D = 5 / 0

gestore_3:
    PRINT ">> Errore catturato:"; ERR.Description
    ON ERROR GOTO 0
    PRINT "Errore successivo NON sara' catturato."

    PRINT "== FINE DEL PROGRAMMA =="
END SUB
