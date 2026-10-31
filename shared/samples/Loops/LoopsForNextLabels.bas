' FILE: Loops/LoopsForNextLabels.bas
' =================================================================================================
'       TEST UNITARIO: CICLI ANNIDATI CON ETICHETTE E 'EXIT/CONTINUE <etichetta>'
' =================================================================================================
' Questo test verifica la capacità di uscire e continuare cicli annidati specifici usando etichette,
' una funzionalità cruciale per gestire flussi di controllo complessi senza GOTO.
'
' COPERTURA:
' - Cicli FOR...NEXT annidati (3 livelli).
' - Etichette associate ai cicli (es. OuterLoop: FOR ...).
' - Istruzione 'EXIT <etichetta>' per uscire da un ciclo non interno.
' - Istruzione 'CONTINUE <etichetta>' per saltare all'iterazione successiva di un ciclo non interno.
' - Verifica che i contatori dei cicli vengano interrotti o avanzati correttamente.
' =================================================================================================

10 CLS
20 PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
30 PRINT "║ LoopsForNextLabels - Test Uscita/Continuazione da Cicli Annidati         ║"
40 PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
50 PRINT

100 PRINT "--- Parte 1: Test 'EXIT <etichetta>' ---"
110 PRINT "Il ciclo interno si interromperà quando I=2, J=2, K=2"
120 PRINT "e userà 'EXIT OuterLoop' per terminare tutti i cicli."
130 PRINT

140 DIM IterazioniExit = 0

OuterLoop: FOR I = 1 TO 3
    MiddleLoop: FOR J = 1 TO 3
        InnerLoop: FOR K = 1 TO 3
            
            IterazioniExit = IterazioniExit + 1
            PRINT "  EXIT Test: I="; I; ", J="; J; ", K="; K
            
            IF I = 2 AND J = 2 AND K = 2 THEN
                PRINT "    -> CONDIZIONE RAGGIUNTA. Eseguo: EXIT OuterLoop"
                EXIT OuterLoop
            END IF
            
        NEXT K
        
        IF I = 2 AND J > 2 THEN
            PRINT "*** ERRORE: Il ciclo MiddleLoop (EXIT) ha continuato dopo l'uscita prevista!"
        END IF
        
    NEXT J
    
    IF I > 2 THEN
        PRINT "*** ERRORE: Il ciclo OuterLoop (EXIT) ha continuato dopo l'uscita prevista!"
    END IF
    
NEXT I

200 PRINT
210 PRINT "--- Cicli (EXIT Test) terminati ---"
220 PRINT "Valore finale di I (dopo EXIT): "; I; " (Atteso: 3, perché NEXT I viene eseguito prima della fine)"
230 PRINT "Valore finale di J (dopo EXIT): "; J; " (Atteso: 3, perché NEXT J viene eseguito prima della fine)"
240 PRINT "Valore finale di K (dopo EXIT): "; K; " (Atteso: 3, perché NEXT K viene eseguito prima della fine)"
250 PRINT "Numero totale di iterazioni (EXIT Test): "; IterazioniExit

260 PRINT
270 IF IterazioniExit < 27 THEN
280   PRINT "RISULTATO (EXIT): SUCCESSO! L'uscita anticipata ha funzionato."
290 ELSE
300   PRINT "RISULTATO (EXIT): FALLIMENTO! 'EXIT OuterLoop' non ha interrotto i cicli."
310 END IF

320 PRINT : PRINT "==============================================================================" : PRINT

330 PRINT "--- Parte 2: Test 'CONTINUE <etichetta>' ---"
340 PRINT "Quando J=2 e K=1, il codice eseguirà 'CONTINUE MiddleLoop',"
350 PRINT "saltando il resto del ciclo K e passando direttamente alla prossima iterazione di J."
360 PRINT

370 DIM IterazioniContinue = 0
380 DIM SkipCount = 0

OuterCont: FOR I = 1 TO 2
    MiddleCont: FOR J = 1 TO 3
        PRINT "  Continue Test: Entrato in MiddleCont J="; J
        InnerCont: FOR K = 1 TO 4
            IterazioniContinue = IterazioniContinue + 1
            
            IF J = 2 AND K = 1 THEN
                PRINT "    -> CONDIZIONE RAGGIUNTA. Eseguo: CONTINUE MiddleCont"
                CONTINUE MiddleCont
            END IF
            
            PRINT "    Continue Test: I="; I; ", J="; J; ", K="; K
            
            IF J = 2 AND K > 1 THEN
                ' Questo contatore si incrementerà solo se CONTINUE fallisce
                SkipCount = SkipCount + 1
            END IF
            
        NEXT K
    NEXT J
NEXT I

400 PRINT
410 PRINT "--- Cicli (CONTINUE Test) terminati ---"
420 PRINT "Iterazioni totali eseguite (CONTINUE Test): "; IterazioniContinue
430 PRINT "Contatore di iterazioni saltate non correttamente: "; SkipCount
440 PRINT
450 IF SkipCount = 0 AND IterazioniContinue < (2 * 3 * 4) THEN
460   PRINT "RISULTATO (CONTINUE): SUCCESSO! 'CONTINUE MiddleLoop' ha saltato correttamente."
470 ELSE
480   PRINT "RISULTATO (CONTINUE): FALLIMENTO! Il salto all'etichetta non ha funzionato."
490 END IF

500 PRINT
510 PRINT "Test completato."
520 END