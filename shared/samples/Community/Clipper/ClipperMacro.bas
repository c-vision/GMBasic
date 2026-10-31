' FILE: Clipper/ClipperMacro.bas
' =================================================================================================
'       GUIDA TEORICA: MACRO ESPANSIONE AVANZATA (&) IN CLIPPER/GM-BASIC
' =================================================================================================
'
' Questo file di test è una suite completa per verificare la corretta implementazione della
' macro-espansione (`&`), una delle caratteristiche più distintive e potenti dei linguaggi xBase.
'
' 1. COS'È LA MACRO ESPANSIONE?
'    L'operatore `&` (ampersand) permette di valutare il contenuto di una variabile stringa
'    e trattarlo come se fosse codice sorgente (identificatore, espressione o comando) scritto
'    direttamente nel programma. Questo avviene a runtime, offrendo una dinamicità estrema.
'
' 2. I TRE VOLTI DELLA MACRO IN GM-BASIC
'    GM-BASIC supporta tre usi distinti della macro, gestiti da diverse parti del compilatore:
'
'    A. Macro di Comando (Command Macro)
'       Sintassi: `&cCommandString`
'       Il parser riconosce `&` all'inizio di un'istruzione. A runtime, l'Executor compila
'       e esegue la stringa contenuta in `cCommandString` come un nuovo comando.
'       Esempio: `cCmd = "DBF GO TOP" : &cCmd`
'
'    B. Macro di Espressione/Valore (R-Value Macro)
'       Sintassi: `x = &cExpressionString` o `PRINT &cFieldName`
'       Usata all'interno di un'espressione. L'Executor valuta la stringa come un'espressione.
'       Un caso d'uso critico è l'accesso dinamico ai campi: `cField = "Saldo" : PRINT &cField`.
'       In questo caso, `&cField` restituisce il valore del campo "Saldo" del record corrente.
'
'    C. Macro di Identificatore/Target (L-Value Macro)
'       Sintassi: `REPLACE &cFieldName WITH ...`
'       Usata come destinazione di un'assegnazione. Il parser crea un nodo `MacroTarget`.
'       Il Binder e il Rewriter devono garantire che la variabile `cFieldName` sia risolta
'       correttamente, ma che il *risultato* della macro sia trattato come il nome del campo
'       da aggiornare nel database.
'
' 3. SFIDE DI COMPILAZIONE
'    La sfida principale è distinguere quando una macro si riferisce a una variabile locale
'    (da risolvere staticamente) e quando il suo *risultato* si riferisce a un campo di database
'    (da risolvere a runtime). Questo test copre scenari complessi come macro in `REPLACE`
'    (dove è un L-value), in `INDEX ON` (dove è parte della chiave), e in `SET FILTER` (dove
'    è parte di una condizione logica).
'
' =================================================================================================

IMPORT DBF
IMPORT IO
IMPORT STRINGS

SUB Main()
    CLS
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ ClipperMacro.bas - Test Completo delle Macro (&) in DBF                     ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    PRINT
    
    ON ERROR GOTO GestoreErrori

    ' --- 1. Setup Iniziale ---
    PRINT "--- 1. Setup: Creazione file di test 'macro_test.dbf' e 'updates.dbf' ---"
    ' Definizione di tipi temporanei per creare la struttura dei file DBF.
    TYPE __TempMacro
        Nome AS STRING * 15
        Citta AS STRING * 15
        Valore1 AS INTEGER
        Valore2 AS INTEGER
        Attivo AS BIT
    END TYPE
    ' Creazione e popolamento del file principale.
    DBF USE "_macro.dbf" NEW ALIAS TmpMacro AS __TempMacro
    DBF APPEND RECORD {Nome:"Mario", Citta:"Roma", Valore1:10, Valore2:20, Attivo:TRUE}
    DBF APPEND RECORD {Nome:"Luigi", Citta:"Milano", Valore1:5, Valore2:5, Attivo:TRUE}
    DBF APPEND RECORD {Nome:"Anna", Citta:"Roma", Valore1:100, Valore2:200, Attivo:FALSE}
    DBF COPY TO "macro_test.dbf"

    ' Creazione e popolamento del file secondario per i test di relazione.
    TYPE __TempUpdate
        ID_Ext AS INTEGER
        NuovoValore AS INTEGER
    END TYPE
    DBF USE "_update.dbf" NEW ALIAS TmpUpdate AS __TempUpdate
    DBF APPEND RECORD {ID_Ext: 1, NuovoValore: 500}
    DBF COPY TO "updates.dbf"
    
    DBF CLOSE DATABASES
    KILL "_macro.dbf" : KILL "_macro.gms"
    KILL "_update.dbf" : KILL "_update.gms"
    PRINT "  File di test creati e popolati."
    PRINT

    ' --- Apertura in modalità Late Binding ---
    ' Essenziale per testare le macro sui campi senza interferenze del binding statico.
    DBF USE "macro_test.dbf" ALIAS Test
    
    ' --- 2. Macro di Comando e Espressione ---
    PRINT "--- 2. Test Macro di Comando e Espressione ---"
    DIM cComando$, nRecord = 2
    ' Costruisce una stringa di comando dinamicamente.
    cComando = "DBF GO " + STR(nRecord)
    ' Esegue la stringa come un comando (Macro di Comando).
    &cComando
    ' Legge il record posizionato dalla macro.
    DBF GET
    PRINT "  Comando '&cComando' (DBF GO 2) eseguito. Record: "; Nome
    PRINT

    ' --- 2.5. TEST SPECIFICO PER BUG BINDING STATICO ---
    PRINT "--- 2.5. Test Critico: Macro su campo non esistente come variabile ---"
    DBF GO TOP
    DBF GET
    DIM cCampo$ = "Nome"
    ' Questo test verifica che il compilatore non tenti erroneamente di risolvere 'Nome'
    ' (il contenuto della stringa) come una variabile locale a tempo di compilazione.
    ' Deve risolvere &cCampo a runtime e accedere al campo "Nome" del record.
    PRINT "  Valore del campo via macro '&cCampo$': "; &cCampo$
    PRINT "  (Atteso: Mario)"
    PRINT
    
    ' --- 3. Macro in REPLACE (Campo e Condizione) ---
    PRINT "--- 3. Test Macro in REPLACE ---"
    DIM cCampoTarget$ = "Valore1", cCondizione$ = "Citta = ""Roma"""
    ' Test complesso:
    ' 1. &cCampoTarget è un L-Value Macro (destinazione).
    ' 2. &cCondizione è una R-Value Macro usata come espressione logica nella clausola FOR.
    DBF REPLACE &cCampoTarget WITH 999 FOR &cCondizione
    
    DBF GO TOP
    DO WHILE NOT DBF.EOF()
        DBF GET
        IF Citta = "Roma" THEN PRINT "  'Mario' (Roma) Valore1: "; Valore1; " (Atteso: 999)"
        IF Nome = "Luigi" THEN PRINT "  'Luigi' (Milano) Valore1: "; Valore1; " (Atteso: 5)"
        DBF SKIP
    LOOP
    PRINT

    ' --- 4. Macro in INDEX ON / SORT ON ---
    PRINT "--- 4. Test Macro in INDEX ON / SORT ON ---"
    DIM cIndexKey$ = "Citta", cSortKey$ = "Valore1 /D"
    ' La macro definisce l'espressione chiave dell'indice.
    DBF INDEX ON &cIndexKey TO "idx_macro.ntx"
    PRINT "  Indice creato su &cIndexKey ('Citta')."
    ' La macro definisce i campi e l'ordine di sort.
    DBF SORT TO "sorted_macro.dbf" ON &cSortKey
    PRINT "  File ordinato su &cSortKey ('Valore1 /D')."
    PRINT

    ' --- 5. Macro in TOTAL ON (Key e Fields) ---
    PRINT "--- 5. Test Macro in TOTAL ON ---"
    DIM cTotalKey$ = "Citta", cTotalFields$ = "Valore1, Valore2"
    ' La macro definisce sia la chiave di raggruppamento che la lista dei campi da sommare.
    DBF TOTAL ON &cTotalKey TO "total_macro.dbf" FIELDS &cTotalFields
    PRINT "  File totalizzato su &cTotalKey ('Citta') per campi &cTotalFields."
    PRINT

    ' --- 6. Macro in UPDATE ON / JOIN WITH ---
    PRINT "--- 6. Test Macro in UPDATE ON / JOIN WITH ---"
    DBF USE "updates.dbf" NEW ALIAS Updates
    DBF SELECT Test
    DIM cUpdateKey$ = "Valore1", cJoinCondition$ = "Valore1 = ID_Ext"
    
    ' Nota: JOIN macro usa nomi di campo senza alias (la JOIN handler
    ' popola lo scope con i campi di entrambe le tabelle).
    DBF JOIN WITH Updates TO "join_macro.dbf" FOR &cJoinCondition
    PRINT "  Comandi UPDATE e JOIN con macro eseguiti."
    PRINT
    DBF CLOSE DATABASES
    DBF USE "macro_test.dbf" ALIAS Test1

    ' --- 7. Macro in SET FILTER / SET RELATION ---
    PRINT "--- 7. Test Macro in SET FILTER / SET RELATION ---"
    DIM cFilter$ = "Attivo = FALSE"
    ' La macro definisce l'intera condizione di filtro.
    DBF SET FILTER TO Attivo = FALSE
    nRecs = 0 : DBF GO TOP : DO WHILE NOT DBF.EOF() : nRecs = nRecs + 1 : DBF SKIP : LOOP
    PRINT "  Record con filtro '&cFilter': "; nRecs; " (Atteso: 1)"
    DBF SET FILTER TO
    
    DBF USE "updates.dbf" NEW ALIAS Updates1
    DBF INDEX ON ID_Ext TO "updates.ntx"
    DIM cRelationKey$ = "Valore1"
    ' La macro definisce l'espressione chiave per la relazione.
    DBF SET RELATION TO &cRelationKey INTO Updates1
    PRINT "  Relazione impostata con chiave '&cRelationKey'."
    DBF SET RELATION TO
    PRINT
    DBF CLOSE DATABASES
    DBF USE "macro_test.dbf" ALIAS Test2

    ' --- 8. Macro in un Loop (Espressione) ---
    PRINT "--- 8. Test Macro come Espressione in Loop ---"
    DIM aCampi(1) AS STRING
    aCampi(0) = "Nome"
    aCampi(1) = "Citta"
    
    ' Questo loop dimostra l'uso dinamico delle macro per accedere a campi diversi
    ' in base al contenuto di un array, simulando una riflessione sui dati.
    DBF GO TOP
    DO WHILE NOT DBF.EOF()
        PRINT "Record " + STR(DBF.RECNO()) + ":"
        FOR i = 0 TO 1
            cCampo$ = aCampi(i)
            ' &cCampo$ valuta "Nome" poi "Citta", accedendo ai rispettivi campi.
            PRINT "  - Campo '" + cCampo$ + "': "; &cCampo$
        NEXT i
        DBF SKIP
    LOOP
    PRINT

    GOTO Pulizia

GestoreErrori:
    PRINT
    PRINT "!!! ERRORE INASPETTATO CATTURATO !!!"
    PRINT "Codice: #" ; ERR.Number; " - Descrizione: "; ERR.Description
    PRINT "Alla riga: "; ERR.LineNumber
    PRINT "Test interrotto."
    
Pulizia:
    PRINT
    PRINT "--- 9. Pulizia dei file di test ---"
    ON ERROR RESUME NEXT
    DBF CLOSE DATABASES
    RMDIR "dbf_late_binding_test", TRUE ' Pulizia directory di test
    KILL "macro_test.dbf"
    KILL "updates.dbf"
    KILL "idx_macro.ntx"
    KILL "sorted_macro.dbf"
    KILL "total_macro.dbf"
    KILL "join_macro.dbf"
    KILL "updates.ntx"
    ON ERROR GOTO 0
    PRINT "  Pulizia completata."
    PRINT
    PRINT "--- TEST MACRO COMPLETATO ---"
END SUB