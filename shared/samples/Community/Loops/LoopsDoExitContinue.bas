' FILE: Loops/LoopsDoExitContinue.bas
' ======================================================================================================================
' GUIDA TEORICA: CONTROLLO AVANZATO DEI CICLI (EXIT E CONTINUE)
' ======================================================================================================================
' GM-BASIC fornisce istruzioni per alterare il flusso di esecuzione naturale dei cicli.
'
' 1. EXIT <LoopType>
'    - Sintassi: EXIT DO, EXIT FOR, EXIT WHILE
'    - Comportamento: Termina immediatamente l'esecuzione del ciclo più interno del tipo specificato.
'      Il controllo passa all'istruzione successiva al termine del blocco del ciclo (es. dopo LOOP, NEXT, WEND).
'    - Utilizzo: Utile per uscire anticipatamente da un ciclo quando una condizione di ricerca è soddisfatta o
'      si verifica un errore.
'
' 2. CONTINUE
'    - Sintassi: CONTINUE (o CONTINUE FOR, CONTINUE DO, CONTINUE WHILE per chiarezza)
'    - Comportamento: Interrompe l'iterazione corrente e salta direttamente alla fase successiva del ciclo.
'      - Nei cicli FOR: Incrementa la variabile contatore e valuta la condizione di termine.
'      - Nei cicli DO/WHILE (Pre-Test): Salta alla valutazione della condizione in testa.
'      - Nei cicli DO (Post-Test): Salta alla valutazione della condizione in coda.
'    - Utilizzo: Utile per saltare l'elaborazione di elementi specifici senza annidare grandi blocchi IF.
' ======================================================================================================================

' --- TEST COMPLETO EXIT DO E CONTINUE ---

SUB TestExitDo()
    PRINT
    PRINT "--- Test di EXIT DO ---"
    DIM I = 1
    ' Ciclo infinito (DO...LOOP senza condizioni)
    DO
        PRINT "  Iterazione DO:"; I
        ' Condizione di uscita anticipata
        IF I = 3 THEN
            PRINT "    Uscita forzata con EXIT DO."
            EXIT DO ' Esce dal ciclo immediatamente
        END IF
        I = I + 1
    LOOP
    ' L'esecuzione riprende qui
    PRINT "  Ciclo DO terminato."
END SUB

SUB TestContinue()
    PRINT
    PRINT "--- Test di CONTINUE FOR ---"
    DIM I
    FOR I = 1 TO 5
        ' Salta l'iterazione specifica
        IF I = 3 THEN
            PRINT "  Salto l'iterazione 3 con CONTINUE."
            CONTINUE ' Salta a NEXT I (incremento e check)
        END IF
        PRINT "  Iterazione FOR:"; I
    NEXT I
    PRINT "  Ciclo FOR terminato."
    PRINT
 
    PRINT "--- Test di CONTINUE DO ---"
    DIM J = 0
    ' Ciclo Post-Test
    DO
        J = J + 1
        ' Salta iterazioni multiple
        IF J = 2 OR J = 4 THEN
            PRINT "  Salto l'iterazione"; J; "con CONTINUE."
            CONTINUE ' Salta al controllo WHILE J < 5
        END IF
        PRINT "  Iterazione DO:"; J
    LOOP WHILE J < 5
    PRINT "  Ciclo DO terminato."
END SUB

' ======================================================================================================================
' VERIFICA DI COMPLETEZZA: TEST INTEGRATIVI
' ======================================================================================================================

SUB TestExitFor()
    PRINT
    PRINT "--- Test di EXIT FOR (Integrazione) ---"
    DIM K
    FOR K = 1 TO 10
        PRINT "  K ="; K
        IF K = 4 THEN
            PRINT "    Uscita forzata a K=4"
            EXIT FOR
        END IF
    NEXT K
    PRINT "  Ciclo FOR terminato (Atteso K=4 o 5 a seconda dell'implementazione interna)."
END SUB

SUB TestNestedLoopsExit()
    PRINT
    PRINT "--- Test EXIT in Cicli Annidati (Integrazione) ---"
    DIM Outer, Inner
    
    FOR Outer = 1 TO 3
        PRINT "  Outer Loop:"; Outer
        
        FOR Inner = 1 TO 5
            IF Inner = 2 THEN
                PRINT "    -> EXIT FOR interno (Inner=2)"
                EXIT FOR ' Deve uscire solo dal ciclo Inner
            END IF
            PRINT "    Inner Loop:"; Inner
        NEXT Inner
        
        PRINT "  Rientro in Outer Loop"
    NEXT Outer
    PRINT "  Test Annidamento terminato."
END SUB

10 CLS
20 PRINT "╔════════════════════════════════════════════════════════════════════════╗"
30 PRINT "║ LoopsDoExitContinue - EXIT and CONTINUE                                ║"
40 PRINT "╚════════════════════════════════════════════════════════════════════════╝"
50 PRINT
60 TestExitDo()
70 TestContinue()
' Esecuzione test integrativi
80 TestExitFor()
90 TestNestedLoopsExit()
100 PRINT
110 PRINT "Test terminato."
120 END