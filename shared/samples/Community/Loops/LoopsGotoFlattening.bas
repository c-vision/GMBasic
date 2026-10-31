' FILE: Loops/LoopsGotoFlattening.bas
' ======================================================================================================================
' GUIDA TEORICA: GOTO VERSO CICLI APPIATTITI (FLATTENED)
' ======================================================================================================================
' Questo test verifica un'interazione critica tra due funzionalità del compilatore: i salti incondizionati (`GOTO`)
' e l'ottimizzazione dei cicli (`Loop Flattening`).
'
' 1. IL PROBLEMA
'    - L'ottimizzatore "Loop Flattening" trasforma un ciclo `FOR...NEXT` in una macchina a stati lineare.
'    - Ad esempio, `100 FOR I = 1 TO 3 ... 120 NEXT I` non esisterà più come un blocco unico nell'AST finale.
'    - Verrà sostituito da una serie di istruzioni di basso livello: `LET I=1`, `LABEL_CHECK`, `IF I>3 JUMP LABEL_EXIT`,
'      corpo del ciclo, `LET I=I+1`, `JUMP LABEL_CHECK`, `LABEL_EXIT`.
'    - Un comando `GOTO 100` deve continuare a funzionare. Se l'ottimizzatore eliminasse la riga numerata 100,
'      il compilatore lancerebbe un errore "Undefined line number".
'
' 2. LA SOLUZIONE ARCHITETTURALE
'    - Il `LoopFlatteningOptimizer` è "consapevole" delle etichette numeriche.
'    - Quando appiattisce un ciclo che inizia su una riga numerata (es. linea 100), si assicura che l'inizio
'      della macchina a stati generata sia associato a quella riga.
'    - Questo garantisce che la mappa delle etichette (`LabelMap`), usata dal Linker per risolvere i `GOTO`,
'      contenga ancora una voce valida per la linea `100`, che punterà alla prima istruzione del ciclo appiattito.
'
' 3. COSA VERIFICA QUESTO TEST
'    - GOTO Esterno: Verifica che un `GOTO` da fuori un ciclo possa entrare correttamente in un ciclo `FOR` (singolo e annidato).
'    - GOTO Interno (Riavvio): Verifica che un `GOTO` dall'interno di un ciclo verso la sua stessa linea di inizio
'      resetti correttamente l'esecuzione del ciclo dall'inizio.
' ======================================================================================================================

10 CLS
20 PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
30 PRINT "║ LoopsGotoFlattening.bas - Test GOTO verso cicli ottimizzati (Flattening)    ║"
40 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
50 PRINT

' --- Test 1: GOTO verso un ciclo FOR singolo ---
' Il comportamento atteso è che il programma salti alla riga 100
' e il ciclo venga eseguito 3 volte.
60 PRINT "--- Test 1: GOTO verso un ciclo FOR singolo ---"
70 GOTO 100

80 PRINT "!!! FAIL !!! - Questa riga (dopo il GOTO 100) non dovrebbe mai essere eseguita."
90 GOTO 999 ' Salta alla fine in caso di fallimento del flusso

100 FOR I = 1 TO 3
110     PRINT "  Ciclo 1, I = "; I
120 NEXT I
130 PRINT "  -> Ciclo 1 completato correttamente."
140 PRINT

150 GOTO 200 ' Passa al prossimo test

' --- Test 2: GOTO verso un ciclo FOR annidato (soggetto a flattening) ---
' Questo è il test critico. Se l'ottimizzatore appiattisce il ciclo
' alle righe 300-350, la label 300 deve essere preservata e puntare
' all'inizio della nuova macchina a stati generata.
200 PRINT "--- Test 2: GOTO verso un ciclo FOR annidato (soggetto a flattening) ---"
210 GOTO 300

220 PRINT "!!! FAIL !!! - Questa riga (dopo il GOTO 300) non dovrebbe mai essere eseguita."
230 GOTO 999

300 FOR J = 1 TO 2
310     PRINT "  Ciclo 2 Esterno, J = "; J
320     FOR K = 1 TO 2
330         PRINT "    Ciclo 2 Interno, K = "; K
340     NEXT K
350 NEXT J
360 PRINT "  -> Ciclo 2 (annidato) completato correttamente."
370 PRINT

380 GOTO 400 ' Passa al prossimo test

' --- Test 3: GOTO per riavviare un ciclo da dentro se stesso ---
' Questo test verifica che la label numerica sia risolvibile anche
' dall'interno del ciclo stesso.
400 PRINT "--- Test 3: GOTO per riavviare un ciclo ---"
410 DIM restart_flag AS BIT = TRUE
500 FOR R = 1 TO 3
510     PRINT "  Ciclo 3, R = "; R
520     IF R = 2 AND restart_flag THEN
530         restart_flag = FALSE
540         PRINT "    ...Riavvio il ciclo con GOTO 500..."
550         GOTO 500
560     END IF
570 NEXT R
580 PRINT "  -> Ciclo 3 completato correttamente dopo il riavvio."
590 PRINT

999 PRINT "--- Test completato ---"
1000 END