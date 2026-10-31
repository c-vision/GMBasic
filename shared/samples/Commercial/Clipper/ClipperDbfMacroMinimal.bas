' FILE: Clipper/ClipperMacroMinimal.bas
' =================================================================================================
'       GUIDA TEORICA: MACRO ESPANSIONE (&) IN L-VALUE (TARGET DI ASSEGNAZIONE)
' =================================================================================================
'
' Questo test verifica una funzionalità specifica e potente derivata dai dialetti xBase/Clipper:
' l'uso dell'operatore di macro espansione (`&`) per definire dinamicamente il campo di destinazione
' di un comando di modifica dati (in questo caso `REPLACE`).
'
' 1. IL CONCETTO DI MACRO (`&`)
'    L'operatore `&` istruisce l'interprete a prendere il contenuto di una variabile stringa
'    e trattarlo come se fosse codice sorgente (in questo caso, un identificatore) scritto direttamente
'    nel programma.
'
' 2. MACRO COME L-VALUE (Left-Value)
'    Normalmente, in un comando come `DBF REPLACE Campo WITH Valore`, "Campo" è un identificatore
'    statico. Usando la sintassi `DBF REPLACE &VariabileStringa WITH Valore`, il nome del campo
'    viene determinato a runtime leggendo il contenuto di `VariabileStringa`.
'
' 3. IMPLEMENTAZIONE IN GM-BASIC
'    - Parser (Fase 1): Riconosce `&` all'inizio di un target e crea un nodo `MacroTarget`.
'    - Binder (Fase 2): Registra la variabile contenuta nella macro (`cNomeCampo`) nello scope locale.
'    - Rewriter (Fase 3): Risolve il simbolo della variabile macro ma lascia intatto `MacroTarget`.
'    - Executor: A runtime, valuta l'espressione della macro, ottiene la stringa ("CampoA") e
'      la usa per cercare il campo corrispondente nella work area attiva per eseguire l'assegnazione.
'
' Questo test è cruciale per garantire che la catena di compilazione gestisca correttamente
' l'indirezione introdotta dalle macro quando usate come destinazione di scrittura.
'
' =================================================================================================

IMPORT DBF
IMPORT IO

SUB Main()
    CLS
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ ClipperMacroMinimal.bas - Test Macro Isolata in DBF REPLACE                 ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    PRINT
    
    ON ERROR GOTO GestoreErrori

    ' --- 1. Setup ---
    PRINT "--- 1. Setup: Creazione file 'macro_test_minimal.dbf' ---"
    ' Definizione di un tipo temporaneo per creare la struttura fisica del DBF.
    TYPE __TempMacro
        CampoA AS INTEGER
        CampoB AS INTEGER
    END TYPE
    ' Creazione e popolamento iniziale del file.
    DBF USE "_macro_min.dbf" NEW ALIAS Tmp AS __TempMacro
    DBF APPEND RECORD {CampoA: 10, CampoB: 20}
    DBF COPY TO "macro_test_minimal.dbf"
    DBF CLOSE DATABASES
    KILL "_macro_min.dbf" : KILL "_macro_min.gms"
    PRINT "  File di test creato e popolato."
    PRINT

    ' --- 2. Test ---
    PRINT "--- 2. Test Macro di Campo in REPLACE ---"
    ' Apertura in modalità Late Binding (senza AS TIPO) per massima flessibilità.
    DBF USE "macro_test_minimal.dbf" ALIAS TestMinimal
    
    ' Definizione della variabile che conterrà il nome del campo target.
    DIM cNomeCampo AS STRING
    cNomeCampo = "CampoA"
    
    PRINT "Eseguo: DBF REPLACE &cNomeCampo WITH 999"
    
    ' CORE DEL TEST: L'operatore & espande cNomeCampo.
    ' A runtime, questo deve equivalere a: DBF REPLACE CampoA WITH 999
    DBF REPLACE &cNomeCampo WITH 999
    
    ' --- 3. Verifica ---
    PRINT
    PRINT "--- 3. Verifica del Risultato ---"
    ' Legge il record corrente. In modalità Late Binding, questo popola le variabili
    ' dinamiche CampoA e CampoB.
    DBF GET
    PRINT "  Valore di CampoA: "; CampoA; " (Atteso: 999)"
    PRINT "  Valore di CampoB: "; CampoB; " (Atteso: 20)"
    
    ' Verifica logica dei valori.
    IF CampoA = 999 AND CampoB = 20 THEN
        PRINT "RISULTATO: SUCCESSO!"
    ELSE
        PRINT "RISULTATO: FALLIMENTO!"
    END IF

    GOTO Pulizia

GestoreErrori:
    PRINT
    PRINT "!!! ERRORE INASPETTATO CATTURATO !!!"
    PRINT "Codice: #" ; ERR.Number; " - Descrizione: "; ERR.Description
    PRINT "Alla riga: "; ERR.LineNumber
    PRINT "Test interrotto."
    
Pulizia:
    PRINT
    PRINT "--- 4. Pulizia ---"
    ON ERROR RESUME NEXT
    DBF CLOSE DATABASES
    KILL "macro_test_minimal.dbf"
    ON ERROR GOTO 0
    PRINT "  Pulizia completata."
    PRINT
    PRINT "--- TEST COMPLETATO ---"
END SUB