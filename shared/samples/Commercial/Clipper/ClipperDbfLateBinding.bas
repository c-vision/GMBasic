' FILE: Clipper/ClipperDbfLateBinding.bas
' =================================================================================================
'       GUIDA TEORICA: LATE BINDING (Emulazione Clipper) e Accesso Dinamico ai Campi
' =================================================================================================
'
' Questo file di test è dedicato a una delle funzionalità più potenti e complesse del motore DBF
' di GM-BASIC: il "Late Binding" (o "Binding Dinamico"), che emula il comportamento tradizionale
' di Clipper/xBase.
'
' 1. COS'È IL LATE BINDING?
'    Normalmente, GM-BASIC incoraggia il "Binding Statico" tramite la clausola `AS Tipo` nel
'    comando `USE`. Questo lega una work area a un `TYPE` definito, permettendo al compilatore
'    di verificare i nomi dei campi a tempo di compilazione.
'
'    Il Late Binding è l'opposto: si attiva quando un file DBF viene aperto **SENZA** la clausola `AS`.
'    In questa modalità, i nomi dei campi (es. `ID`, `Nome`, `Saldo`) non sono noti al compilatore.
'    Quando il compilatore incontra un nome di variabile non dichiarato in un modulo che ha
'    `IMPORT DBF`, non genera un errore, ma lo trasforma in un nodo AST speciale. Sarà compito
'    dell'Executor, a runtime, cercare un campo con quel nome nel record attualmente attivo.
'
' 2. L'ARCHITETTURA DIETRO IL LATE BINDING
'    - `IMPORT DBF`: Questa direttiva agisce come un interruttore che attiva la "Modalità Clipper"
'      per il compilatore solo per il file corrente.
'    - `USE ...` (senza `AS`): Apre il database in modalità di binding dinamico.
'    - `DBF GET`: Questo comando è FONDAMENTALE. Popola un buffer interno a `BasicRuntime`
'      con i valori del record corrente.
'    - Accesso ai Campi: Quando si scrive `PRINT Nome` o `REPLACE Saldo WITH ...`, l'Executor,
'      vedendo l'AST speciale generato dal compilatore, non cerca una variabile `Nome` o `Saldo`,
'      ma cerca una chiave con quel nome nel buffer popolato da `DBF GET`.

' 3. IL COMANDO `GET` IN MODALITÀ LATE BINDING
'    - `DBF GET` o `DBF GET`: Popola il buffer interno. Necessario prima di accedere ai campi
'      come se fossero variabili.
'    - `DBF GET miaVariabile`: Se `miaVariabile` non è stata dichiarata, questo comando ha un
'      comportamento speciale: legge la struttura del file DBF a runtime, crea un `TYPE`
'      dinamico in memoria, crea l'istanza `miaVariabile` con quel tipo e la popola.
'
' 4. LAMBDA E CONTESTO
'    Anche all'interno di una lambda (es. in `SET FILTER`), il late binding funziona.
'    `FN(rec) => Citta = "Roma"`: Il parametro `rec` è un placeholder per il compilatore.
'    A runtime, `Citta` viene risolto come un campo del record attivo, non come un membro di `rec`.
'
' Questo test verifica rigorosamente che tutte le operazioni DBF (REPLACE, LIST, FOR, filtri,
' join, etc.) funzionino correttamente in questa modalità dinamica, dimostrando la piena
' compatibilità con lo stile di programmazione di Clipper.
'
' =================================================================================================

' Importa i namespace necessari per i comandi qualificati e le funzioni di conversione.
IMPORT DBF WITH ALIAS
IMPORT CONVERSIONS WITH ALIAS
IMPORT IO WITH ALIAS
IMPORT DATETIME WITH ALIAS
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

10 CLS
20 PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
30 PRINT "║ ClipperDbfLateBinding - DBF Full Test Suite (LATE BINDING)                  ║"
40 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
50 PRINT
60 // ==============================================================================
70 // Setup Iniziale: Import e Variabili
80 // ==============================================================================

170 DIM cPath AS STRING
180 DIM nArea AS INTEGER

210 DIM sFilter AS STRING
220 DIM sKeyExpr AS STRING
230 DIM nRet AS INTEGER

240 // Percorso temporaneo per i file DBF
250 cPath = g_work + "/dbf_test_data"
260 MKDIR cPath
270 DBF SET DEFAULT TO cPath // Imposta la directory di default per i comandi DBF
' Pulizia dei file generati: rende il sample DETERMINISTICO tra gambe e run. Senza,
' una gamba precedente lascia clienti.dbf/indici e `DBF USE ... AS Tipo` (create-if-missing)
' APRE il file stantio invece di crearlo → record/indici ereditati (divergenze vmc/c99n).
KILL cPath + "/clienti.dbf"
KILL cPath + "/clienti.gms"
KILL cPath + "/_structure_source.dbf"
KILL cPath + "/_structure_source.gms"
KILL cPath + "/idx_nome_temp.ntx"
KILL cPath + "/idx_nome.ntx"
KILL cPath + "/idx_citta_nome.ntx"
KILL cPath + "/idx_citta.ntx"
KILL cPath + "/cli_id.ntx"
KILL cPath + "/citta_saldo.ntx"
KILL cPath + "/tot_citta.dbf"
KILL cPath + "/tot_citta.gms"
KILL cPath + "/totali.dbf"
KILL cPath + "/clienti_ordinati.dbf"
KILL cPath + "/clienti_ordinati.gms"
KILL cPath + "/clienti_roma.dbf"
KILL cPath + "/clienti_roma.gms"
KILL cPath + "/movimenti.dbf"
KILL cPath + "/movimenti.gms"
KILL cPath + "/idx_id_mov.ntx"
KILL cPath + "/mov_id.ntx"
KILL cPath + "/clienti_movimenti.dbf"
KILL cPath + "/clienti_movimenti.gms"
KILL cPath + "/joined.dbf"
KILL cPath + "/roma.dbf"
KILL cPath + "/sorted.dbf"


280 PRINT "-------------------------------------------------------------------------------"
290 PRINT "1. Creazione e Apertura del Database 'CLIENTI'"
300 PRINT "-------------------------------------------------------------------------------"

' Workaround per creare un file DBF con una struttura definita senza avere un TYPE persistente.
' Si crea un file temporaneo basato su un TYPE locale, se ne copia la struttura, e poi si elimina il temporaneo.
310 // Sintassi Legacy: Crea la struttura del file DBF da un UDT fittizio per avere un sorgente
    TYPE __TempStructure
        ID AS INTEGER
        Nome AS STRING * 20
        Citta AS STRING * 15
        Saldo AS CURRENCY
        Attivo AS BIT
        UltimoOrdine AS DATE
    END TYPE
    : DBF USE "_structure_source.dbf" EXCLUSIVE ALIAS TempStruct AS __TempStructure
    : DBF COPY STRUCTURE TO "clienti.dbf"
    : DBF CLOSE DATABASES
    : KILL "_structure_source.dbf"
    : KILL "_structure_source.gms"

' COMANDO CHIAVE: Apre il database SENZA la clausola "AS Tipo".
' Questo attiva la modalità di LATE BINDING per la work area con alias "Clienti".
340 // Sintassi Legacy: Apre il database appena creato (che ora ha la struttura corretta), senza AS.
345 DBF USE "clienti.dbf" EXCLUSIVE ALIAS Clienti

350 PRINT "Database 'clienti.dbf' creato e aperto con alias 'Clienti'."
360 PRINT "Work Area corrente: " + STR(DBF.SELECTED())

370 // ==============================================================================
380 // Popolamento Dati: APPEND BLANK, REPLACE
390 // ==============================================================================
400 PRINT
410 PRINT "-------------------------------------------------------------------------------"
420 PRINT "2. Popolamento Dati Iniziale"
430 PRINT "-------------------------------------------------------------------------------"

' Aggiunge un record vuoto e lo popola. In modalità late binding, i nomi dei campi
' (ID, Nome, Citta, etc.) non sono variabili, ma vengono risolti a runtime.
440 // Record 1
450 DBF APPEND BLANK
460 DBF REPLACE ID WITH 1, Nome WITH "Mario Rossi", Citta WITH "Roma", Saldo WITH 150.75, Attivo WITH TRUE, UltimoOrdine WITH DATETIME.TODAY()
470 PRINT "Aggiunto: Mario Rossi"

480 // Record 2
490 DBF APPEND BLANK
500 DBF REPLACE ID WITH 2, Nome WITH "Giulia Verdi", Citta WITH "Milano", Saldo WITH 2300.50, Attivo WITH TRUE, UltimoOrdine WITH DATETIME.DATEADD("D", -10, DATETIME.TODAY())
510 PRINT "Aggiunto: Giulia Verdi"

520 // Record 3
530 DBF APPEND BLANK
540 DBF REPLACE ID WITH 3, Nome WITH "Andrea Bianchi", Citta WITH "Torino", Saldo WITH -120.00, Attivo WITH FALSE, UltimoOrdine WITH DATETIME.DATEADD("M", -1, DATETIME.TODAY())
550 PRINT "Aggiunto: Andrea Bianchi"

560 // Record 4 (Senza variabile UDT intermedia)
570 DBF APPEND BLANK
580 DBF REPLACE ID WITH 4, Nome WITH "Laura Neri", Citta WITH "Roma", Saldo WITH 500.00, Attivo WITH TRUE, UltimoOrdine WITH DATETIME.DATEADD("Y", -2, DATETIME.TODAY())
640 PRINT "Aggiunto: Laura Neri"

650 // Record 5
660 DBF APPEND BLANK
670 DBF REPLACE ID WITH 5, Nome WITH "Marco Gialli", Citta WITH "Firenze", Saldo WITH 99.99, Attivo WITH TRUE, UltimoOrdine WITH DATETIME.DATEADD("D", -5, DATETIME.TODAY())
680 PRINT "Aggiunto: Marco Gialli"

690 // Record 6
700 DBF APPEND BLANK
710 DBF REPLACE ID WITH 6, Nome WITH "Record Cancellato", Citta WITH "Milano", Saldo WITH 1000.00, Attivo WITH FALSE, UltimoOrdine WITH DATETIME.DATEADD("M", -3, DATETIME.TODAY())
720 PRINT "Aggiunto: Record Cancellato (per test DELETE)"

730 // Record 7
740 DBF APPEND BLANK
750 DBF REPLACE ID WITH 7, Nome WITH "Cliente Update", Citta WITH "Roma", Saldo WITH 100.00, Attivo WITH TRUE, UltimoOrdine WITH DATETIME.TODAY()
760 PRINT "Aggiunto: Cliente Update (per test UPDATE ON)"

770 PRINT "Totale record nel database: " + STR(DBF.LASTREC())

780 // ==============================================================================
790 // Navigazione e Lettura Dati: GO, SKIP, SEEK, GET
800 // ==============================================================================
810 PRINT
820 PRINT "-------------------------------------------------------------------------------"
830 PRINT "3. Navigazione e Lettura Dati"
840 PRINT "-------------------------------------------------------------------------------"

850 DBF GO TOP
860 PRINT "Posizionato a TOP. Record corrente: "
' DBF GET popola il buffer interno, rendendo i campi accessibili come variabili.
870 DBF GET // riempie il buffer corrente
' DBF GET var con una variabile non dichiarata la crea dinamicamente a runtime.
880 DBF GET clienteRec // crea la variabile inesistente
885 PRINT "  ID: " + STR(clienteRec.ID) + ", Nome: " + clienteRec.Nome

890 DBF SKIP 3
900 PRINT "Saltato di 3 posizioni. Record corrente: "
910 DBF GET clienteRec ' Popola la variabile 'clienteRec' con i dati del record corrente.
920 PRINT "  ID: " + STR(clienteRec.ID) + ", Nome: " + clienteRec.Nome

930 DBF GO BOTTOM
940 PRINT "Posizionato a BOTTOM. Record corrente: "
950 DBF GET clienteRec
960 PRINT "  ID: " + STR(clienteRec.ID) + ", Nome: " + clienteRec.Nome
' Creiamo un indice per testare SEEK. I nomi dei campi sono risolti a runtime.
965 DBF INDEX ON Nome TO "idx_nome_temp.ntx"
967 DBF SET ORDER TO "idx_nome_temp.ntx"

970 DBF SET EXACT ON
980 PRINT "SET EXACT ON. Cercando 'Mario Rossi'..."
990 DBF SEEK "Mario Rossi"
1000 IF DBF.FOUND() THEN
1010    DBF GET clienteRec
1020    PRINT "  Trovato: ID " + STR(clienteRec.ID) + ", Nome: " + clienteRec.Nome
1030 ELSE
1040    PRINT "  ""Mario Rossi"" non trovato (con SET EXACT ON)."
1050 END IF

1060 DBF SET EXACT OFF
1070 PRINT "SET EXACT OFF. Cercando 'Mar'..."
1080 DBF SEEK "Mar"
1090 IF DBF.FOUND() THEN
1100    DBF GET clienteRec
1110    PRINT "  Trovato: ID " + STR(clienteRec.ID) + ", Nome: " + clienteRec.Nome
1120 ELSE
1130    PRINT "  ""Mar"" non trovato (con SET EXACT OFF)."
1140 END IF
1145 DBF SET ORDER TO 0 // Torna a ordine naturale

1150 // ==============================================================================
1160 // Filtraggio Dati: SET DELETED, SET FILTER
1170 // ==============================================================================
1180 PRINT
1190 PRINT "-------------------------------------------------------------------------------"
1200 PRINT "4. Filtraggio e Gestione Record Cancellati"
1210 PRINT "-------------------------------------------------------------------------------"

1220 // Test DELETE e SET DELETED
1230 DBF SEEK "Record Cancellato"
1240 IF DBF.FOUND() THEN
1250    DBF DELETE
1260    PRINT "Record 'Record Cancellato' marcato come eliminato."
1270 ELSE
1280    PRINT "Record 'Record Cancellato' non trovato per eliminazione."
1290 END IF

1300 DBF SET DELETED ON
1310 PRINT "SET DELETED ON. Elenco clienti (solo attivi e non cancellati):"
1320 DBF GO TOP
1330 DO WHILE NOT DBF.EOF()
' In late binding, i campi (ID, Nome) sono accessibili direttamente, letti dal buffer interno.
1340    // Non possiamo usare GET senza UDT, quindi stampiamo i campi direttamente
1350    PRINT "  ID: " + STR(ID) + ", Nome: " + Nome
1360    DBF SKIP
1370 LOOP

1380 DBF SET DELETED OFF
1390 PRINT "SET DELETED OFF. Elenco clienti (tutti, inclusi i cancellati):"
1400 DBF GO TOP
1410 DO WHILE NOT DBF.EOF()
1420    PRINT "  ID: " + STR(ID) + ", Nome: " + Nome + " (Cancellato: " + CONVERSIONS.CSTR(DBF.DELETED()) + ")"
1440    DBF SKIP
1450 LOOP

' Anche nella lambda, "ID" è un campo risolto a runtime, non un membro del parametro "rec".
1460 DBF RECALL FOR FN(rec) => ID = 6
1470 PRINT "Richiamato record con ID = 6."

1480 // Test SET FILTER
' La condizione del filtro usa i nomi dei campi che saranno risolti a runtime.
1490 DBF SET FILTER TO FN(rec) => Citta = "Roma" AND Saldo > 0
1500 PRINT "SET FILTER con Lambda: Clienti di Roma con Saldo > 0."
1510 PRINT "Clienti filtrati:"
1520 DBF GO TOP
1530 DO WHILE NOT DBF.EOF()
1540    PRINT "  ID: " + STR(ID) + ", Nome: " + Nome + ", Citta: " + Citta + ", Saldo: " + CONVERSIONS.CSTR(Saldo)
1560    DBF SKIP
1570 LOOP
1580 DBF SET FILTER TO ""

1600 // ==============================================================================
1610 // Indicizzazione: INDEX ON, SET ORDER
1620 // ==============================================================================
1630 PRINT
1640 PRINT "-------------------------------------------------------------------------------"
1650 PRINT "5. Indicizzazione e Ordine dei Record"
1660 PRINT "-------------------------------------------------------------------------------"

' I campi "Nome" e "Citta" vengono risolti a runtime per creare gli indici.
1670 DBF INDEX ON Nome TO "idx_nome.ntx"
1680 PRINT "Creato indice 'idx_nome.ntx' sul campo Nome."
1690 DBF INDEX ON Citta + Nome TO "idx_citta_nome.ntx"
1700 PRINT "Creato indice 'idx_citta_nome.ntx' sul campo Citta + Nome."

1710 DBF SET ORDER TO "idx_nome"
1720 PRINT "Ordine impostato su 'idx_nome'."
1730 PRINT "Elenco clienti per Nome:"
1740 DBF GO TOP
1750 DO WHILE NOT DBF.EOF()
1770    PRINT "  Nome: " + Nome + ", Citta: " + Citta
1780    DBF SKIP
1790 LOOP

1800 DBF SET ORDER TO "idx_citta_nome"
1810 PRINT "Ordine impostato su 'idx_citta_nome'."
1820 PRINT "Elenco clienti per Citta e Nome:"
1830 DBF GO TOP
1840 DO WHILE NOT DBF.EOF()
1860    PRINT "  Citta: " + Citta + ", Nome: " + Nome
1880    DBF SKIP
1890 LOOP
1892 DBF SET ORDER TO 0

1900 // ==============================================================================
1910 // Query e Report: COPY TO, TOTAL ON, SORT TO
1920 // ==============================================================================
1930 PRINT
1940 PRINT "-------------------------------------------------------------------------------"
1950 PRINT "6. Query e Report (COPY, TOTAL, SORT)"
1960 PRINT "-------------------------------------------------------------------------------"

1970 // COPY TO con filtro
1980 DBF COPY TO "clienti_roma.dbf" FOR FN(rec) => Citta = "Roma"
1990 PRINT "Copiati clienti di Roma in 'clienti_roma.dbf'."

2000 // TOTAL ON
2010 DBF SET ORDER TO "idx_citta_nome"
2020 DBF TOTAL ON Citta TO "tot_citta.dbf" FIELDS Saldo FOR FN(rec) => Attivo = TRUE
2030 DBF USE "tot_citta.dbf" ALIAS "TotaliCitta" ' Senza AS per testare il late binding anche qui.
2040 DBF SELECT TotaliCitta
2050 PRINT "Totali per città (solo Attivo = TRUE):"
2060 DBF GO TOP
2070 DO WHILE NOT DBF.EOF()
2090    PRINT "  Citta: " + Citta + ", Saldo Totale: " + CONVERSIONS.CSTR(Saldo)
2110    DBF SKIP
2120 LOOP
2122 DBF CLOSE
2130 DBF SELECT Clienti

2140 // SORT TO
2150 DBF SORT TO "clienti_ordinati.dbf" ON Citta, Saldo /D FOR FN(rec) => Attivo = TRUE
2160 DBF USE "clienti_ordinati.dbf" ALIAS "Ordinati"
2170 DBF SELECT Ordinati
2180 PRINT "Clienti ordinati per Città, poi Saldo Decrescente:"
2190 DBF GO TOP
2200 DO WHILE NOT DBF.EOF()
2220    PRINT "  Citta: " + Citta + ", Saldo: " + CONVERSIONS.CSTR(Saldo) + ", Nome: " + Nome
2230    DBF SKIP
2240 LOOP
2250 DBF CLOSE
2260 DBF SELECT Clienti

2270 // ==============================================================================
2280 // Aggiornamento e Join da Altro Database: UPDATE ON, JOIN WITH
2290 // ==============================================================================
2300 PRINT
2310 PRINT "-------------------------------------------------------------------------------"
2320 PRINT "7. Aggiornamento e Join da Altri Database"
2330 PRINT "-------------------------------------------------------------------------------"

2340 // Crea un file 'movimenti.dbf' per il test
2350 DBF COPY STRUCTURE TO "movimenti.dbf"
2360 DBF USE "movimenti.dbf" UNSAFE ALIAS "Movimenti" // Senza AS
2370 DBF SELECT Movimenti
2380 DBF APPEND BLANK
2390 DBF REPLACE ID WITH 7, Nome WITH "Update Source", Citta WITH "Marsiglia", Saldo WITH 50.00, Attivo WITH TRUE, UltimoOrdine WITH DATETIME.TODAY()
2400 DBF APPEND BLANK
2410 DBF REPLACE ID WITH 1, Nome WITH "Mario Updated", Citta WITH "Roma", Saldo WITH 20.00, Attivo WITH TRUE, UltimoOrdine WITH DATETIME.TODAY()
2420 DBF INDEX ON ID TO "idx_id_mov.ntx"
2430 DBF SET ORDER TO "idx_id_mov"
2440 DBF SELECT Clienti
' §7/§8 cercano per Nome: l ordine attivo ereditato da §6 è il COMPOSTO Citta+Nome
' (SET ORDER TO "idx_citta_nome"), quindi SEEK "<Nome>" non trovava mai il record e le
' sezioni stampavano il record CORRENTE stantio (divergenze §7/§8 su tutte le gambe).
2445 DBF SET ORDER TO "idx_nome"

' In UPDATE ON, i campi non qualificati (Nome, Saldo) si riferiscono alla work area corrente (Clienti).
' I campi qualificati (Movimenti.Nome) si riferiscono all'altra work area.
' Clipper: UPDATE ON <chiave> richiede il MASTER indicizzato sulla chiave (ID), non su Nome:
' con un ordine diverso l'aggiornamento diventava dipendente dal backend. Si azzera l'ordine
' per l'UPDATE e lo si ripristina subito dopo per le SEEK per Nome.
2450 DBF SET ORDER TO 0
2455 DBF UPDATE ON ID FROM Movimenti REPLACE Nome WITH Movimenti.Nome, Saldo WITH Saldo + Movimenti.Saldo
2457 DBF SET ORDER TO "idx_nome"
2460 PRINT "Aggiornato il record ID=7 e ID=1 dal database 'Movimenti'."
2470 DBF SEEK "Mario Updated"
2480 IF DBF.FOUND() THEN
2500     PRINT "  Record ID=1 (Mario Rossi) ora è: Nome: " + Nome + ", Saldo: " + CONVERSIONS.CSTR(Saldo)
2510 END IF
2520 DBF SEEK "Cliente Update"
2530 IF DBF.FOUND() THEN
2550     PRINT "  Record ID=7 (Cliente Update) ora è: Nome: " + Nome + ", Saldo: " + CONVERSIONS.CSTR(Saldo)
2560 END IF

' Nella lambda del JOIN, i parametri `cli` e `mov` sono convenzionali. I campi vengono risolti
' in base al nome dell'alias delle tabelle coinvolte.
2570 // JOIN WITH con Lambda
2580 DBF JOIN WITH Movimenti TO "clienti_movimenti.dbf" FOR FN(cli, mov) => Clienti.ID = Movimenti.ID FIELDS Clienti.Nome, Movimenti.Saldo, Movimenti.Citta
2590 PRINT "Creato 'clienti_movimenti.dbf' unendo Clienti e Movimenti."
2600 DBF USE "clienti_movimenti.dbf" ALIAS "ClientiMov"
2610 DBF SELECT ClientiMov
2620 PRINT "Contenuto del file unito:"
2630 DBF GO TOP
2640 DO WHILE NOT DBF.EOF()
2660     PRINT "  Nome: " + Nome + ", Saldo Movimento: " + Saldo + ", Citta Movimento: " + Citta
2670     DBF SKIP
2680 LOOP
2690 DBF CLOSE
2700 DBF SELECT Clienti

' ... (Il resto del file rimane uguale dalla riga 2710 in poi) ...
2710 // ==============================================================================
2720 // Transazioni: BEGIN TRANSACTION, COMMIT, ROLLBACK
2730 // ==============================================================================
2740 PRINT
2750 PRINT "-------------------------------------------------------------------------------"
2760 PRINT "8. Test Transazioni (BEGIN TRANSACTION, COMMIT, ROLLBACK)"
2770 PRINT "-------------------------------------------------------------------------------"

2780 DBF BEGIN TRANSACTION LEVEL REPEATABLE READ
2790    PRINT "Iniziata transazione. Saldo attuale di ID=1:"
2800    DBF SEEK "Mario Updated"
2820    PRINT "  Prima modifica - Saldo ID=1: " + CONVERSIONS.CSTR(Saldo)
2830    DBF REPLACE Saldo WITH Saldo + 100
2850    PRINT "  Dopo modifica (non commessa) - Saldo ID=1: " + CONVERSIONS.CSTR(Saldo)
2860 DBF ROLLBACK
2870 PRINT "Rollback eseguito. Verifico saldo di ID=1:"
2880 DBF SEEK "Mario Updated"
2900 PRINT "  Dopo Rollback - Saldo ID=1: " + CONVERSIONS.CSTR(Saldo)

2910 DBF BEGIN TRANSACTION
2920    PRINT "Iniziata nuova transazione. Saldo attuale di ID=2:"
2930    DBF SEEK "Giulia Verdi"
2950    PRINT "  Prima modifica - Saldo ID=2: " + CONVERSIONS.CSTR(Saldo)
2960    DBF REPLACE Saldo WITH Saldo - 50
2980    PRINT "  Dopo modifica (non commessa) - Saldo ID=2: " + CONVERSIONS.CSTR(Saldo)
2990 DBF COMMIT
3000 PRINT "Commit eseguito. Verifico saldo di ID=2:"
3010 DBF SEEK "Giulia Verdi"
3030 PRINT "  Dopo Commit - Saldo ID=2: " + CONVERSIONS.CSTR(Saldo)

3040 // ==============================================================================
3050 // Integrità e Manutenzione: DBF VALIDATE, PACK, REINDEX, ZAP
3060 // ==============================================================================
3070 PRINT
3080 PRINT "-------------------------------------------------------------------------------"
3090 PRINT "9. Integrità e Manutenzione (VALIDATE, PACK, REINDEX, ZAP)"
3100 PRINT "-------------------------------------------------------------------------------"

3110 DBF VALIDATE Clienti
3120 PRINT "Eseguita validazione su 'Clienti'."

3130 // Per testare PACK, cancelliamo di nuovo il record con ID=6
3140 DBF SEEK "Record Cancellato"
3150 IF DBF.FOUND() THEN
3160    DBF DELETE
3170    PRINT "Record ID=6 marcato come eliminato per test PACK."
3180 END IF

3190 DBF PACK
3200 PRINT "Eseguito PACK su 'clienti.dbf'. I record cancellati sono stati rimossi fisicamente."
3210 PRINT "Totale record dopo PACK: " + STR(DBF.LASTREC())
3230 DBF SELECT Clienti

3240 DBF REINDEX
3250 PRINT "Eseguito REINDEX su 'clienti.dbf'. Indici ricostruiti."

3260 DBF ZAP
3270 PRINT "Eseguito ZAP su 'clienti.dbf'. Tutti i record rimossi permanentemente."
3280 PRINT "Totale record dopo ZAP: " + STR(DBF.LASTREC())

3290 // ==============================================================================
3300 // Esportazione Dati
3310 // ==============================================================================
3320 PRINT
3330 PRINT "-------------------------------------------------------------------------------"
3340 PRINT "10. Esportazione Dati"
3350 PRINT "-------------------------------------------------------------------------------"

3360 // Aggiungiamo alcuni record per l'esportazione
3370 DBF APPEND BLANK
3380 DBF REPLACE ID WITH 100, Nome WITH "Export Test", Citta WITH "Roma", Saldo WITH 1000.00, Attivo WITH TRUE, UltimoOrdine WITH DATETIME.TODAY()
3390 DBF APPEND BLANK
3400 DBF REPLACE ID WITH 101, Nome WITH "Export Test 2", Citta WITH "Milano", Saldo WITH 2000.00, Attivo WITH TRUE, UltimoOrdine WITH DATETIME.TODAY()

3410 DBF EXPORT TO "clienti_export.csv" AS CSV
3420 PRINT "Esportato 'clienti.dbf' in 'clienti_export.csv'."

3430 DBF EXPORT TO "clienti_export.json" AS JSON
3440 PRINT "Esportato 'clienti.dbf' in 'clienti_export.json'."

3540 REM ==============================================================================
3550 REM 11. Test Comandi con Late Binding (senza clausola AS TIPO)
3560 REM ==============================================================================
3570 PRINT
3580 PRINT "-------------------------------------------------------------------------------"
3590 PRINT "11. Test Comandi con Late Binding (Senza AS TIPO)"
3600 PRINT "-------------------------------------------------------------------------------"
3610 
3620 REM --- Test REPLACE FOR ---
3630 PRINT "Test REPLACE FOR..."
3640 DBF USE "clienti.dbf" ALIAS ClientiLate
3650 DBF SELECT ClientiLate
3660 DBF REPLACE Saldo WITH Saldo * 1.1 FOR Citta = "Roma"
3670 DBF GO TOP : DBF GET
3680 PRINT "  Saldo 'Rossi' (ID 1) dopo aumento del 10%: " + CONVERSIONS.CSTR(Saldo) + " (Atteso 165.825)"
3690 DBF CLOSE DATABASES
3700 
3710 REM --- Test DELETE & RECALL FOR ---
3720 PRINT "Test DELETE/RECALL FOR..."
3730 DBF USE "clienti.dbf" ALIAS ClientiDelete
3740 DBF DELETE FOR Saldo > 200
3750 DBF SET DELETED ON
3760 nRecs = 0 : DBF GO TOP : DO WHILE NOT DBF.EOF() : nRecs = nRecs + 1 : DBF SKIP : LOOP
3770 PRINT "  Record non cancellati (Saldo <= 200): "; nRecs
3780 DBF RECALL FOR Citta = "Milano"
3790 nRecs = 0 : DBF GO TOP : DO WHILE NOT DBF.EOF() : nRecs = nRecs + 1 : DBF SKIP : LOOP
3800 PRINT "  Record non cancellati dopo RECALL: "; nRecs
3810 DBF SET DELETED OFF : DBF RECALL ALL
3820 DBF CLOSE DATABASES
3830 
3840 REM --- Test COPY TO & APPEND FROM FOR ---
3850 PRINT "Test COPY TO / APPEND FROM FOR..."
3860 DBF USE "clienti.dbf" ALIAS ClientiCopy
3870 DBF COPY TO "roma.dbf" FOR Citta = "Roma"
3880 DBF USE "roma.dbf" NEW ALIAS RomaDB
3890 nRecs = 0 : DBF GO TOP : DO WHILE NOT DBF.EOF() : nRecs = nRecs + 1 : DBF SKIP : LOOP
3900 PRINT "  Record nel file 'roma.dbf': "; nRecs
3910 DBF CLOSE DATABASES : DBF USE "clienti.dbf" ALIAS ClientiAppend
3920 DBF APPEND FROM "roma.dbf" FOR Saldo > 150
3930 nRecs = 0 : DBF GO TOP : DO WHILE NOT DBF.EOF() : nRecs = nRecs + 1 : DBF SKIP : LOOP
3940 PRINT "  Record totali dopo APPEND FROM: "; nRecs
3950 DBF CLOSE DATABASES
3960 
3970 REM --- Test SORT TO & INDEX ON ---
3980 PRINT "Test SORT TO / INDEX ON..."
3990 DBF USE "clienti.dbf" ALIAS ClientiSort
4000 DBF SORT TO "sorted.dbf" ON Saldo /D FOR Attivo = TRUE
4010 DBF USE "sorted.dbf" NEW ALIAS Sorted
4020 DBF GO TOP : DBF GET
4030 PRINT "  Primo record per Saldo (decrescente): Nome='"; Nome; "'"
4040 DBF CLOSE DATABASES : DBF USE "clienti.dbf" ALIAS ClientiIndex
4050 DBF INDEX ON Citta + STR(Saldo) TO "citta_saldo.ntx"
4060 DBF SET ORDER TO 1
4070 DBF GO TOP : DBF GET
4080 PRINT "  Primo record per Citta+Saldo: Citta='"; Citta; "'"
4090 DBF CLOSE DATABASES
4100 
4110 REM --- Test TOTAL ON ---
4120 PRINT "Test TOTAL ON..."
4130 DBF USE "clienti.dbf" ALIAS ClientiTotal
4140 DBF INDEX ON Citta TO "idx_citta.ntx" : DBF SET ORDER TO 1
4150 DBF TOTAL ON Citta TO "totali.dbf" FIELDS Saldo FOR Saldo > 100
4160 DBF USE "totali.dbf" NEW ALIAS Totali
4170 DBF GO TOP : DBF GET
4180 PRINT "  Totale per (primo gruppo): " + CONVERSIONS.CSTR(Saldo)
4190 DBF CLOSE DATABASES : DBF USE "clienti.dbf" ALIAS ClientiUpdate
4200 
4210 REM --- Test UPDATE ON & JOIN WITH ---
4220 PRINT "Test UPDATE ON / JOIN WITH..."
4230 DBF USE "movimenti.dbf" NEW ALIAS MovimentiUpdate
4240 DBF SELECT MovimentiUpdate
4250 DBF INDEX ON ID_Cliente TO "mov_id.ntx"
4260 DBF SELECT ClientiUpdate
4270 DBF INDEX ON ID TO "cli_id.ntx" : DBF SET ORDER TO 1
4280 DBF UPDATE ON ID FROM MovimentiUpdate REPLACE Saldo WITH Saldo + MovimentiUpdate.Importo
4290 DBF GO 1 : DBF GET
4300 PRINT "  Saldo di 'Rossi' (ID 1) dopo UPDATE: " + CONVERSIONS.CSTR(Saldo)
4310 DBF JOIN WITH MovimentiUpdate TO "joined.dbf" FOR ID = MovimentiUpdate.ID_Cliente FIELDS Nome, Saldo, Importo
4320 DBF USE "joined.dbf" NEW ALIAS JoinedUpdate
4330 DBF LIST
4340 DBF CLOSE DATABASES
4350 
4360 REM --- 8. Test SET FILTER & SET RELATION ---
4370 PRINT "--- 8. Test SET FILTER / SET RELATION ---"
4380 DBF USE "clienti.dbf" ALIAS ClientiFilter
4390 DBF SET FILTER TO Citta = "Torino"
4400 nRecs = 0 : DBF GO TOP : DO WHILE NOT DBF.EOF() : nRecs = nRecs + 1 : DBF SKIP : LOOP
4410 PRINT "  Record con filtro 'Torino': "; nRecs
4420 DBF SET FILTER TO
4430 
4440 DBF USE "movimenti.dbf" NEW ALIAS MovimentiRel
4450 DBF SELECT MovimentiRel : DBF INDEX ON ID_Cliente TO "mov_id.ntx" : DBF SET ORDER TO 1
4460 DBF SELECT ClientiFilter
4470 DBF SET RELATION TO ID INTO MovimentiRel
4480 DBF GO 1 ' Posiziona su Rossi
4490 DBF SELECT MovimentiRel
4500 PRINT "  Record in Movimenti dopo relazione su ID 1: Importo="; Importo
4510 DBF SET RELATION TO
4520 DBF CLOSE DATABASES
4530 
4531 REM ==============================================================================
4532 REM 8B. Test Comando LIST (Late Binding)
4533 REM ==============================================================================
4534 PRINT
4535 PRINT "-------------------------------------------------------------------------------"
4536 PRINT "8B. Test Comando LIST (Legacy e Qualificato)"
4537 PRINT "-------------------------------------------------------------------------------"
4538 DBF USE "clienti.dbf" ALIAS ClientiList
4539 
4540 PRINT "--- LIST Legacy: FIELDS e FOR ---"
4541 DBF LIST FIELDS Nome, Citta, Saldo FOR Saldo > 150 AND Attivo = TRUE
4542 
4543 PRINT : PRINT "--- LIST Legacy: TO FILE ---"
4544 DBF LIST TO FILE "list_output.txt" FOR Citta = "Roma"
4545 IF IO.FileExists("list_output.txt") THEN PRINT "  File 'list_output.txt' creato."
4546 
4547 PRINT : PRINT "--- LIST Qualificato: FIELDS e FOR con Lambda ---"
4548 DBF LIST FIELDS Nome, Saldo FOR FN(rec) => Saldo < 200 AND Citta <> "Roma"
4549 
4550 PRINT : PRINT "--- LIST Qualificato: Solo FOR con stringa, e TO FILE ---"
4551 DBF LIST FOR Attivo = FALSE TO "list_qual_output.txt"
4552 IF IO.FileExists("list_qual_output.txt") THEN PRINT "  File 'list_qual_output.txt' creato."
4553 
4554 DBF CLOSE DATABASES
4555 
4556 REM ==============================================================================
4557 REM 9. Pulizia Finale (RIGHE ORIGINALI RINUMERATE)
4558 REM ==============================================================================
4570 PRINT
4580 PRINT "-------------------------------------------------------------------------------"
4590 PRINT "9. Pulizia Finale"
4600 PRINT "-------------------------------------------------------------------------------"
4610 
4620 DBF CLOSE DATABASES
4630 PRINT "Chiusi tutti i database."
4640 
4650 ON ERROR RESUME NEXT
4660 KILL "clienti.dbf"
4670 KILL "idx_nome.ntx"
4680 KILL "idx_citta.ntx"
4690 KILL "idx_citta_nome.ntx"
4700 KILL "clienti_roma.dbf"
4710 KILL "tot_citta.dbf"
4720 KILL "clienti_ordinati.dbf"
4730 KILL "movimenti.dbf"
4740 KILL "idx_id_mov.ntx"
4750 KILL "clienti_movimenti.dbf"
4760 KILL "clienti_export.csv"
4770 KILL "clienti_export.json"
4780 KILL "clienti.gms"
4790 KILL "clienti.gmv"
4800 KILL "movimenti.gms"
4810 KILL "movimenti.gmv"
4820 KILL "tot_citta.gms"
4830 KILL "tot_citta.gmv"
4840 KILL "clienti_ordinati.gms"
4850 KILL "clienti_ordinati.gmv"
4860 KILL "clienti_roma.gms"
4870 KILL "clienti_roma.gmv"
4880 KILL "clienti_movimenti.gms"
4890 KILL "clienti_movimenti.gmv"
4900 KILL "sorted.dbf"
4910 KILL "citta_saldo.ntx"
4920 KILL "cli_id.ntx"
4930 KILL "joined.dbf"
4940 KILL "list_output.txt"
4950 KILL "list_qual_output.txt"
4960 RMDIR cPath
4970 ON ERROR GOTO 0
4980 PRINT "  Pulizia completata."
4990 
5000 PRINT
5010 PRINT "--- TEST DI LATE BINDING COMPLETATO ---"
5020 END
