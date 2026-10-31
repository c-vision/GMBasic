' FILE: Branches/BranchesSelectCaseAdvanced.bas
' =================================================================================================
'                                GUIDA TEORICA: SELECT CASE, EXIT e Pattern Matching
' =================================================================================================
'
' Questo test dimostra tre costrutti fondamentali per il controllo del flusso:
' `SELECT CASE` per la gestione di bivi multipli e `EXIT` per l'interruzione anticipata dei cicli.
'
' 1. IL COSTRUTTO `SELECT CASE`
'    `SELECT CASE` è una struttura di controllo per gestire decisioni complesse, alternativa
'    a una lunga catena di `IF...ELSEIF...ELSE`.
'
'    La sua struttura è la seguente:
'    - `SELECT CASE <espressione>`: L'interprete valuta l'espressione una sola volta all'inizio.
'    - `CASE <test>`: Un blocco di codice eseguito se il risultato corrisponde al test.
'
'    GM-BASIC supporta le forme tradizionali e il **Pattern Matching Avanzato**:
'      a) Valore singolo: `CASE 10`
'      b) Intervallo di valori: `CASE 5 TO 10`
'      c) Comparazione: `CASE IS > 20`
'      d) Pattern su Tipo con Acquisizione: `CASE IS STRING s`
'      e) Pattern su Valore con Acquisizione e Filtro: `CASE 15 AS v WHEN v > 10`
'      f) Pattern su Tipo con Filtro: `CASE IS DOUBLE d WHEN d > 0`
'
'    - `CASE ELSE`: Blocco opzionale eseguito se nessun'altra clausola `CASE` corrisponde.
'    - `END SELECT`: Chiude obbligatoriamente la struttura.
'
'    L'interprete controlla ogni `CASE` in ordine. Al primo `CASE` che risulta vero, il suo
'    blocco di codice viene eseguito e il controllo passa direttamente all'istruzione dopo `END SELECT`.
'
' 2. IL COMANDO `EXIT`
'    Il comando `EXIT` permette di interrompere prematuramente l'esecuzione di un ciclo,
'    trasferendo il controllo alla prima istruzione che segue la fine del ciclo stesso.
'
'    Le forme principali sono:
'    - `EXIT FOR`: Termina il ciclo `FOR...NEXT` più interno.
'    - `EXIT DO`: Termina il ciclo `DO...LOOP` più interno.
'    - `EXIT WHILE`: Termina il ciclo `WHILE...WEND` più interno.
'
'    Questo comando è tipicamente usato all'interno di un `IF` per gestire condizioni di
'    terminazione specifiche (es. un valore trovato, un errore, etc.).
'
' =================================================================================================

' --- Sottoprocedura per testare i tipi numerici e l'acquisizione di valore ---
SUB TestSelectNumeric(Valore AS DOUBLE)
    PRINT
    PRINT "--- Testando SELECT CASE con Valore ="; Valore; " (Tipo: DOUBLE) ---"
    
    SELECT CASE Valore
        ' 1. Clausola CASE per un singolo valore.
        CASE 1
            PRINT "  Trovato: Uno (Valore Singolo)"
        ' 2. Clausola CASE per una lista di valori separati da virgola.
        CASE 2 TO 4
            PRINT "  Trovato: Due, Tre o Quattro (Lista di Valori)"
        ' 3. Clausola CASE per un intervallo di valori.
        CASE 5 TO 10
            PRINT "  Trovato: Un valore tra 5 e 10 (Intervallo)"
        ' 4. Clausola CASE per una comparazione.
        CASE IS > 200
            PRINT "  Trovato: Un valore maggiore di 200 (Comparazione)"
        
        ' 5. Pattern su Valore con Assegnazione (Test con Double/Integer)
        CASE 15
            PRINT "  Trovato: 15"
            
        ' 6. Pattern su Tipo (double) con Filtro (WHEN)
        CASE IS > 100
            PRINT "  Trovato: DOUBLE maggiore di 100"

        ' Clausola di fallback se nessun altro CASE corrisponde.
        CASE ELSE
            PRINT "  Trovato: Altro Valore (Caso non gestito)"
    END SELECT
END SUB

' --- Sottoprocedura per testare il tipo Stringa ---
SUB TestSelectString(Valore AS STRING)
    PRINT
    PRINT "--- Testando SELECT CASE con Valore ="; Valore; " (Tipo: STRING) ---"
    
    SELECT CASE Valore
        ' Clausola di fallback se nessun altro CASE corrisponde.
        CASE ELSE
            PRINT "  Trovato: stringa (fallback)"
    END SELECT
END SUB

' --- Sottoprocedura per testare tutte le varianti del costrutto SELECT CASE. ---
SUB TestSelect(Valore)
    PRINT
    PRINT "--- Testando SELECT CASE con Valore ="; Valore; "---"
    ' Inizia il blocco SELECT CASE sul parametro 'Valore'.
    SELECT CASE Valore
        ' Clausola CASE per un singolo valore.
        CASE 1
            PRINT "  Trovato: Uno (Valore Singolo)"
        ' Clausola CASE per una lista di valori separati da virgola.
        CASE 2 TO 4
            PRINT "  Trovato: Due, Tre o Quattro (Lista di Valori)"
        ' Clausola CASE per un intervallo di valori.
        CASE 5 TO 10
            PRINT "  Trovato: Un valore tra 5 e 10 (Intervallo)"
        ' Clausola CASE per una comparazione.
        CASE IS > 20
            PRINT "  Trovato: Un valore maggiore di 20 (Comparazione)"
        ' Clausola di fallback se nessun altro CASE corrisponde.
        CASE ELSE
            PRINT "  Trovato: Altro Valore (Caso non gestito)"
    ' Termina il blocco SELECT CASE.
    END SELECT
END SUB

' --- Sottoprocedura per testare l'uscita anticipata da un ciclo FOR. ---
SUB TestExitFor()
    PRINT
    PRINT "--- Test di EXIT FOR ---"
    FOR I = 1 TO 10
        PRINT "  Iterazione FOR:"; I
        IF I = 3 THEN
            PRINT "    Uscita forzata con EXIT FOR."
            EXIT FOR
        END IF
    NEXT I
    PRINT "  Ciclo FOR terminato."
END SUB

' --- Sottoprocedura per testare l'uscita anticipata da un ciclo DO...LOOP. ---
SUB TestExitDo()
    PRINT
    PRINT "--- Test di EXIT DO ---"
    I = 0
    DO
        I = I + 1
        PRINT "  Iterazione DO:"; I
        IF I = 3 THEN
            PRINT "    Uscita forzata con EXIT DO."
            EXIT DO
        END IF
    LOOP WHILE I < 10
    PRINT "  Ciclo DO terminato."
END SUB

' --- Sottoprocedura per testare l'uscita anticipata da un ciclo WHILE...WEND. ---
SUB TestExitWhile()
    PRINT
    PRINT "--- Test di EXIT WHILE ---"
    I = 1
    WHILE I <= 10
        PRINT "  Iterazione WHILE:"; I
        IF I = 3 THEN
            PRINT "    Uscita forzata con EXIT WHILE."
            EXIT WHILE
        END IF
        I = I + 1
    WEND
    PRINT "  Ciclo WHILE terminato."
END SUB


' ============================
' --- CORPO PRINCIPALE DEL TEST ---
' ============================
10 CLS
20 PRINT "╔═══════════════════════════════════════════════════════════════════════╗"
30 PRINT "║ BranchesSelectCase - SELECT CASE & EXIT Statements                    ║"
40 PRINT "╚═══════════════════════════════════════════════════════════════════════╝"

' Test delle varie clausole di SELECT CASE
50 TestSelect(3)   ' Test della lista di valori
55 TestSelectNumeric(3)      ' Test della lista di valori
60 TestSelect(7)   ' Test dell'intervallo
65 TestSelectNumeric(7)      ' Test dell'intervallo
70 TestSelect(25)  ' Test della comparazione
75 TestSelectNumeric(250)    ' Test della comparazione (>200)
80 TestSelect(15)  ' Test del CASE ELSE
85 TestSelectNumeric(15)     ' Test del Pattern Valore con Assegnazione (v=15)
90 TestSelectNumeric(150.5)  ' Test del Pattern con IS > 100
100 TestSelectNumeric(50.0)   ' Test del CASE ELSE (Double, ma <= 100)
110 TestSelectNumeric(42)     ' Test del CASE ELSE (Integer)

' Test del Pattern Matching (Stringa)
120 TestSelectString("Ciao Mondo")

' Test delle varie forme del comando EXIT
130 TestExitFor()
140 TestExitDo()
150 TestExitWhile()

160 PRINT
170 PRINT "Test terminato."
180 END