' FILE: Loops/LoopsForNextStressed.bas
' ======================================================================================================================
' GUIDA TEORICA: OTTIMIZZAZIONE "LOOP FLATTENING"
' ======================================================================================================================
' Questo test è progettato specificamente per verificare l'efficacia del "Loop Flattening Optimizer" del compilatore.
'
' 1. IL PROBLEMA DEI CICLI ANNIDATI
'    - Nell'interpretazione classica (AST Walking), ogni ciclo annidato spesso comporta una nuova chiamata ricorsiva
'      al metodo `ExecuteBlock`. In cicli profondamente annidati o con molte iterazioni, questo causa un overhead
'      significativo (stack depth, context switching).
'
' 2. LA SOLUZIONE: APPIATTIMENTO (FLATTENING)
'    - Il Linker (Fase 3) trasforma la struttura gerarchica dei cicli (`ForStmt` che contiene `Body`) in una
'      sequenza lineare di istruzioni a basso livello (Macchina a Stati).
'    - Struttura Tipica Appiattita:
'      1. Inizializzazione (LET I = Start)
'      2. Etichetta Inizio (LABEL_START)
'      3. Controllo Condizione (IF I > End GOTO LABEL_EXIT)
'      4. Corpo del ciclo
'      5. Etichetta Continua (LABEL_CONTINUE)
'      6. Incremento (LET I = I + Step)
'      7. Salto Incondizionato (GOTO LABEL_START)
'      8. Etichetta Uscita (LABEL_EXIT)
'
' 3. VANTAGGI
'    - Performance: Elimina l'overhead delle chiamate ricorsive dell'interprete.
'    - Controllo: Semplifica drasticamente l'implementazione di `EXIT FOR` e `CONTINUE` in cicli annidati,
'      che diventano semplici salti (GOTO) a etichette generate.
' ======================================================================================================================

1 CLS
2 PRINT "╔════════════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ LoopsForNextOptimized - FOR NEXT - Optimized (Cicli appiattiti e ottimizzati)  ║"
4 PRINT "╚════════════════════════════════════════════════════════════════════════════════╝"
5 PRINT

' --- TEST 1: Cicli Annidati Standard ---
' Questo blocco verificherà la corretta generazione delle etichette e dei salti per cicli multipli.
10 FOR I = 1 TO 3
20     PRINT "I = "; I
30     FOR J = 1 TO 2
40         PRINT "  J = "; J
50     NEXT J
60 NEXT I
70 PRINT "Cicli annidati completati."

' --- TEST 2: Ciclo con STEP Negativo (Decremento) ---
' Verifica che la logica di incremento appiattita gestisca correttamente i passi negativi.
PRINT
PRINT "--- TEST 2: STEP Negativo ---"
FOR K = 3 TO 1 STEP -1
    PRINT "  K (Down) = "; K
NEXT K

' --- TEST 3: Modifica della Variabile di Ciclo (Stateful Logic) ---
' L'ottimizzatore deve usare la variabile reale per il controllo, permettendo modifiche nel corpo.
' Se K salta da 1 a 3 manualmente, il ciclo deve terminare prima.
PRINT
PRINT "--- TEST 3: Modifica Variabile Loop ---"
FOR L = 1 TO 5
    PRINT "  L prima = "; L
    IF L = 2 THEN 
        L = 4 ' Salta le iterazioni 3 e 4
        PRINT "    -> Salto manuale a 4"
    END IF
NEXT L

' --- TEST 4: Scope del Contatore FOR (block-scoped) ---
' In GM-BASIC il contatore di un FOR è una variabile LOCALE AL BLOCCO (shadowing):
' se esiste una variabile DIM omonima nello scope esterno, il FOR la "maschera"
' durante il ciclo e, al termine, la variabile esterna conserva il suo valore.
' NIENTE semantica "End + Step" del BASIC classico dopo il ciclo.
PRINT
PRINT "--- TEST 4: Scope del Contatore FOR ---"
DIM M = 42
FOR M = 1 TO 3
    ' Il corpo vede la M del blocco (1, 2, 3), non la DIM esterna.
NEXT M
PRINT "  Valore di M dopo il ciclo (atteso 42, la DIM esterna): "; M
IF M = 42 THEN PRINT "  SUCCESSO" ELSE PRINT "  FALLITO"

80 END