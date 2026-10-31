' FILE: Clipper/ClipperDbfFullTests.bas
' =================================================================================================
'       GUIDA TEORICA: Suite di Test Completa per il Motore DBF in Modalità Clipper
' =================================================================================================
'
' Questo file rappresenta una suite di test di integrazione esaustiva per il motore DBF di GM-BASIC.
' Verifica la corretta implementazione dei comandi DBF in sintassi legacy (sintassi spazio).
'
' 1. ARCHITETTURA DEL TEST
'    - Setup: Il test inizia creando un ambiente pulito, definendo un `TYPE` che mappa la
'      struttura del file DBF e preparando una directory temporanea per i file.
'    - Test per Categoria: Ogni funzionalità del motore DBF (comandi SET, apertura, popolamento,
'      navigazione, modifica, indicizzazione, query, transazioni, etc.) è testata in una
'      sezione numerata e dedicata.
'    - Pulizia: Il test termina chiudendo tutti i file e rimuovendo la directory temporanea
'      e tutti i file generati, garantendo di non lasciare artefatti sul sistema.
'
' 2. CONCETTI CHIAVE DIMOSTRATI
'    - Modalità Clipper (`IMPORT DBF`): L'inclusione di `IMPORT DBF` attiva la modalità di
'      compatibilità che permette la risoluzione dei campi a runtime (late binding),
'      essenziale per la sintassi `REPLACE Campo WITH ...` o `FOR Campo = ...`.
'    - Legame Statico (`AS Tipo`): L'uso di `AS Cliente` nel comando `USE` lega la work area
'      a una struttura `TYPE` a tempo di compilazione. Questo abilita il type-checking,
'      l'autocompletamento (IntelliSense) e previene errori sui nomi dei campi.
'    - Sintassi Comandi: `DBF COMANDO opzioni...` (solo legacy a spazio; nessuna forma qualificata).
'    - Lambda Expressions (`FN(...)`): Le clausole di filtro (`FOR`/`WHILE`) supportano
'      funzioni anonime per una maggiore sicurezza dei tipi rispetto alle espressioni
'      passate come stringa.
'    - Funzioni di Stato: Tutte le funzioni che restituiscono informazioni sullo stato del
'      database (es. `DBF.EOF()`, `DBF.RECNO()`) usano la sintassi qualificata con parentesi.
'
' Questo test è un documento vivente delle capacità del motore DBF e un punto di
' riferimento cruciale per la validazione di future modifiche o correzioni di bug.
'
' =================================================================================================

IMPORT DBF WITH ALIAS
IMPORT CONVERSIONS WITH ALIAS
IMPORT IO WITH ALIAS
IMPORT DATETIME WITH ALIAS
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

10 CLS
20 PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
30 PRINT "║ ClipperDbfFullTests - DBF Full Test Suite                                   ║"
40 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
50 PRINT
60 // ==============================================================================
70 // Setup Iniziale: Import, Variabili Globali e Tipo Utente
80 // ==============================================================================

' Definisce la struttura dati che mapperà i campi del file DBF.
' Questo permette il controllo dei tipi a tempo di compilazione.
90 TYPE Cliente
100    ID AS INTEGER        ' Corrisponde a DbfFieldType.Integer
110    Nome AS STRING * 20  ' Corrisponde a DbfFieldType.Character
120    Citta AS STRING * 15 ' Corrisponde a DbfFieldType.Character
130    Saldo AS CURRENCY    ' Corrisponde a DbfFieldType.Currency
140    Attivo AS BIT        ' Corrisponde a DbfFieldType.Logical
150    UltimoOrdine AS DATE ' Corrisponde a DbfFieldType.Date
160 END TYPE

' Dichiarazione delle variabili globali usate nel test.
170 DIM cPath AS STRING
180 DIM nArea AS INTEGER
190 DIM clienteRec AS Cliente
200 DIM totaliRec AS Cliente
210 DIM sFilter AS STRING
220 DIM sKeyExpr AS STRING
230 DIM nRet AS INTEGER

' Prepara una directory pulita per i file di test.
240 // Percorso temporaneo per i file DBF
250 cPath = g_work + "/dbf_test_data"
260 MKDIR cPath
270 DBF SET DEFAULT TO cPath ' Imposta la directory di default per tutti i comandi DBF successivi.

280 PRINT "-------------------------------------------------------------------------------"
290 PRINT "1. Creazione e Apertura del Database 'CLIENTI'"
300 PRINT "-------------------------------------------------------------------------------"
' Crea un file DBF vuoto ("clienti.dbf") basato sulla struttura del tipo 'Cliente'.
310 // Sintassi Legacy: Crea la struttura del file DBF
320 DBF COPY STRUCTURE TO "clienti.dbf"

' Apre il database in modalità esclusiva, lo lega al tipo 'Cliente' e gli assegna l'alias 'Clienti'.
330 // Sintassi Legacy: Apre il database principale in un'area di lavoro
340 DBF USE "clienti.dbf" EXCLUSIVE AS Cliente ALIAS Clienti

350 PRINT "Database 'clienti.dbf' creato e aperto con alias 'Clienti'."
360 PRINT "Work Area corrente: " + STR(DBF.SELECTED())

370 // ==============================================================================
380 // Popolamento Dati: APPEND BLANK, REPLACE, APPEND RECORD
390 // ==============================================================================
400 PRINT
410 PRINT "-------------------------------------------------------------------------------"
420 PRINT "2. Popolamento Dati Iniziale"
430 PRINT "-------------------------------------------------------------------------------"

' Aggiunge un record vuoto e lo popola campo per campo.
440 // Record 1 (legacy)
450 DBF APPEND BLANK ' Aggiunge un record vuoto.
460 DBF REPLACE ID WITH 1, Nome WITH "Mario Rossi", Citta WITH "Roma", Saldo WITH 150.75, Attivo WITH TRUE, UltimoOrdine WITH DATETIME.TODAY()
470 PRINT "Aggiunto: Mario Rossi"

' Record 2 (Sintassi Legacy)
480 // Record 2
490 DBF APPEND BLANK ' Sintassi Legacy
500 DBF REPLACE ID WITH 2, Nome WITH "Giulia Verdi", Citta WITH "Milano", Saldo WITH 2300.50, Attivo WITH TRUE, UltimoOrdine WITH DATETIME.DATEADD("D", -10, DATETIME.TODAY())
510 PRINT "Aggiunto: Giulia Verdi"

' Record 3
520 // Record 3
530 DBF APPEND BLANK
540 DBF REPLACE ID WITH 3, Nome WITH "Andrea Bianchi", Citta WITH "Torino", Saldo WITH -120.00, Attivo WITH FALSE, UltimoOrdine WITH DATETIME.DATEADD("M", -1, DATETIME.TODAY())
550 PRINT "Aggiunto: Andrea Bianchi"

' Record 4 (Aggiunta da variabile UDT)
560 // Record 4
570 clienteRec.ID = 4
580 clienteRec.Nome = "Laura Neri"
590 clienteRec.Citta = "Roma"
600 clienteRec.Saldo = 500.00
610 clienteRec.Attivo = TRUE
620 clienteRec.UltimoOrdine = DATETIME.DATEADD("Y", -2, DATETIME.TODAY())
630 DBF APPEND RECORD clienteRec
640 PRINT "Aggiunto: Laura Neri"

' Record 5
650 // Record 5
660 DBF APPEND BLANK
670 DBF REPLACE ID WITH 5, Nome WITH "Marco Gialli", Citta WITH "Firenze", Saldo WITH 99.99, Attivo WITH TRUE, UltimoOrdine WITH DATETIME.DATEADD("D", -5, DATETIME.TODAY())
680 PRINT "Aggiunto: Marco Gialli"

' Record 6 (Record da cancellare)
690 // Record 6 (Record da cancellare)
700 DBF APPEND BLANK
710 DBF REPLACE ID WITH 6, Nome WITH "Record Cancellato", Citta WITH "Milano", Saldo WITH 1000.00, Attivo WITH FALSE, UltimoOrdine WITH DATETIME.DATEADD("M", -3, DATETIME.TODAY())
720 PRINT "Aggiunto: Record Cancellato (per test DELETE)"

' Record 7 (Record da aggiornare da altro file)
730 // Record 7 (Record da aggiornare da altro file)
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

' Spostamento del puntatore del record.
850 DBF GO TOP
860 PRINT "Posizionato a TOP. Record corrente: "
870 DBF GET clienteRec
880 PRINT "  ID: " + STR(clienteRec.ID) + ", Nome: " + clienteRec.Nome

890 DBF SKIP 3 ' Salta 3 record
900 PRINT "Saltato di 3 posizioni. Record corrente: "
910 DBF GET clienteRec
920 PRINT "  ID: " + STR(clienteRec.ID) + ", Nome: " + clienteRec.Nome

930 DBF GO BOTTOM ' Sintassi Legacy
940 PRINT "Posizionato a BOTTOM. Record corrente: "
950 DBF GET clienteRec
960 PRINT "  ID: " + STR(clienteRec.ID) + ", Nome: " + clienteRec.Nome

' Test della ricerca con SET EXACT.
970 DBF SET EXACT ON ' SET EXACT ON per SEEK precisi
980 PRINT "SET EXACT ON. Cercando 'Mario Rossi'..."
990 DBF SEEK "Mario Rossi" ' Richiede indice sul nome (che non abbiamo), quindi fallirà. Corretto per testare FOUND()
1000 IF DBF.FOUND() THEN
1010    DBF GET clienteRec
1020    PRINT "  Trovato: ID " + STR(clienteRec.ID) + ", Nome: " + clienteRec.Nome
1030 ELSE
1040    PRINT "  ""Mario Rossi"" non trovato (con SET EXACT ON)."
1050 END IF

1060 DBF SET EXACT OFF ' SET EXACT OFF per SEEK parziali
1070 PRINT "SET EXACT OFF. Cercando 'Mar'..."
1080 DBF SEEK "Mar"
1090 IF DBF.FOUND() THEN
1100    DBF GET clienteRec
1110    PRINT "  Trovato: ID " + STR(clienteRec.ID) + ", Nome: " + clienteRec.Nome
1120 ELSE
1130    PRINT "  ""Mar"" non trovato (con SET EXACT OFF)."
1140 END IF

1150 // ==============================================================================
1160 // Filtraggio Dati: SET DELETED, SET FILTER
1170 // ==============================================================================
1180 PRINT
1190 PRINT "-------------------------------------------------------------------------------"
1200 PRINT "4. Filtraggio e Gestione Record Cancellati"
1210 PRINT "-------------------------------------------------------------------------------"

' Test della marcatura per cancellazione e del suo annullamento.
1220 // Test DELETE e SET DELETED
1230 DBF SEEK "Record Cancellato"
1240 IF DBF.FOUND() THEN
1250    DBF DELETE ' Marca il record corrente per la cancellazione.
1260    PRINT "Record 'Record Cancellato' marcato come eliminato."
1270 ELSE
1280    PRINT "Record 'Record Cancellato' non trovato per eliminazione."
1290 END IF

' SET DELETED ON nasconde i record marcati dalle operazioni di navigazione e query.
1300 DBF SET DELETED ON
1310 PRINT "SET DELETED ON. Elenco clienti (solo attivi e non cancellati):"
1320 DBF GO TOP
1330 DO WHILE NOT DBF.EOF()
1340    DBF GET clienteRec
1350    PRINT "  ID: " + STR(clienteRec.ID) + ", Nome: " + clienteRec.Nome
1360    DBF SKIP
1370 LOOP

' SET DELETED OFF rende nuovamente visibili i record marcati.
1380 DBF SET DELETED OFF
1390 PRINT "SET DELETED OFF. Elenco clienti (tutti, inclusi i cancellati):"
1400 DBF GO TOP
1410 DO WHILE NOT DBF.EOF()
1420    DBF GET clienteRec
1430    PRINT "  ID: " + STR(clienteRec.ID) + ", Nome: " + clienteRec.Nome + " (Cancellato: " + CONVERSIONS.CSTR(DBF.DELETED()) + ")"
1440    DBF SKIP
1450 LOOP

' Annulla la marcatura per cancellazione usando una lambda.
1460 DBF RECALL FOR FN(rec) => rec.ID = 6 ' con Lambda
1470 PRINT "Richiamato record con ID = 6."

' Test di SET FILTER per visualizzare solo un sottoinsieme di record.
1480 // Test SET FILTER
1490 DBF SET FILTER TO FN(rec) => rec.Citta = "Roma" AND rec.Saldo > 0 ' con Lambda
1500 PRINT "SET FILTER con Lambda: Clienti di Roma con Saldo > 0."
1510 PRINT "Clienti filtrati:"
1520 DBF GO TOP
1530 DO WHILE NOT DBF.EOF()
1540    DBF GET clienteRec
1550    PRINT "  ID: " + STR(clienteRec.ID) + ", Nome: " + clienteRec.Nome + ", Citta: " + clienteRec.Citta + ", Saldo: " + CONVERSIONS.CSTR(clienteRec.Saldo)
1560    DBF SKIP
1570 LOOP
1580 DBF SET FILTER TO "" ' Rimuove il filtro

1600 // ==============================================================================
1610 // Indicizzazione: INDEX ON, SET ORDER
1620 // ==============================================================================
1630 PRINT
1640 PRINT "-------------------------------------------------------------------------------"
1650 PRINT "5. Indicizzazione e Ordine dei Record"
1660 PRINT "-------------------------------------------------------------------------------"

' Crea i file di indice (.ntx).
1670 DBF INDEX ON Nome TO "idx_nome.ntx" ' Crea indice per Nome
1680 PRINT "Creato indice 'idx_nome.ntx' sul campo Nome."
1690 DBF INDEX ON Citta + Nome TO "idx_citta_nome.ntx" ' Crea indice composito
1700 PRINT "Creato indice 'idx_citta_nome.ntx' sul campo Citta + Nome."

' Attiva l'ordine basato sull'indice del nome.
1710 DBF SET ORDER TO "idx_nome" ' Imposta ordine per Nome
1720 PRINT "Ordine impostato su 'idx_nome'."
1730 PRINT "Elenco clienti per Nome:"
1740 DBF GO TOP
1750 DO WHILE NOT DBF.EOF()
1760    DBF GET clienteRec
1770    PRINT "  Nome: " + clienteRec.Nome + ", Citta: " + clienteRec.Citta
1780    DBF SKIP
1790 LOOP

' Attiva l'ordine basato sull'indice composito.
1800 DBF SET ORDER TO "idx_citta_nome"
1810 PRINT "Ordine impostato su 'idx_citta_nome'."
1820 PRINT "Elenco clienti per Citta e Nome:"
1830 DBF GO TOP
1840 DO WHILE NOT DBF.EOF()
1850    DBF GET clienteRec
1860    PRINT "  Citta: " + clienteRec.Citta + ", Nome: " + clienteRec.Nome
1870    DBF SKIP
1880 LOOP
1890 DBF SET ORDER TO 0 ' Torna all'ordine naturale (fisico).

1900 // ==============================================================================
1910 // Query e Report: COPY TO, TOTAL ON, SORT TO
1920 // ==============================================================================
1930 PRINT
1940 PRINT "-------------------------------------------------------------------------------"
1950 PRINT "6. Query e Report (COPY, TOTAL, SORT)"
1960 PRINT "-------------------------------------------------------------------------------"

' Copia un sottoinsieme di record in un nuovo file DBF.
1970 // COPY TO con filtro
1980 DBF COPY TO "clienti_roma.dbf" FOR FN(rec) => rec.Citta = "Roma" ' con Lambda
1990 PRINT "Copiati clienti di Roma in 'clienti_roma.dbf'."

' Aggrega i dati, sommando i campi numerici per ogni valore univoco di un campo chiave.
2000 // TOTAL ON
2010 DBF SET ORDER TO "idx_citta_nome" ' Per TOTAL ON è necessario un ordine
2020 DBF TOTAL ON Citta TO "tot_citta.dbf" FIELDS Saldo FOR FN(rec) => rec.Attivo = TRUE ' con Lambda
2030 DBF USE "tot_citta.dbf" ALIAS "TotaliCitta"
2040 DBF SELECT TotaliCitta
2050 PRINT "Totali per città (solo Attivo = TRUE):"
2060 DBF GO TOP
2070 DO WHILE NOT DBF.EOF()
2080    DBF GET totaliRec
2090    PRINT "  Citta: " + totaliRec.Citta + ", Saldo Totale: " + CONVERSIONS.CSTR(totaliRec.Saldo)
2100    DBF SKIP
2110 LOOP
2120 DBF CLOSE ' Chiude database TotaliCitta
2130 DBF SELECT Clienti

' Crea una copia ordinata del file.
2140 // SORT TO
2150 DBF SORT TO "clienti_ordinati.dbf" ON Citta, Saldo /D FOR FN(rec) => rec.Attivo = TRUE ' con Lambda
2160 DBF USE "clienti_ordinati.dbf" ALIAS "Ordinati"
2170 DBF SELECT Ordinati
2180 PRINT "Clienti ordinati per Città, poi Saldo Decrescente:"
2190 DBF GO TOP
2200 DO WHILE NOT DBF.EOF()
2210    DBF GET clienteRec
2220    PRINT "  Citta: " + clienteRec.Citta + ", Saldo: " + CONVERSIONS.CSTR(clienteRec.Saldo) + ", Nome: " + clienteRec.Nome
2230    DBF SKIP
2240 LOOP
2250 DBF CLOSE ' Chiude database Ordinati
2260 DBF SELECT Clienti

2270 // ==============================================================================
2280 // Aggiornamento e Join da Altro Database: UPDATE ON, JOIN WITH
2290 // ==============================================================================
2300 PRINT
2310 PRINT "-------------------------------------------------------------------------------"
2320 PRINT "7. Aggiornamento e Join da Altri Database"
2330 PRINT "-------------------------------------------------------------------------------"

' Crea un file 'movimenti.dbf' per il test di UPDATE e JOIN.
2340 // Crea un file 'movimenti.dbf' per il test di UPDATE e JOIN
2350 DBF COPY STRUCTURE TO "movimenti.dbf"
2360 DBF USE "movimenti.dbf" UNSAFE ALIAS "Movimenti" AS Cliente ' EXCLUSIVE = TRUE
2370 DBF SELECT Movimenti
2380 DBF APPEND BLANK
2390 DBF REPLACE ID WITH 7, Nome WITH "Update Source", Citta WITH "Marsiglia", Saldo WITH 50.00, Attivo WITH TRUE, UltimoOrdine WITH DATETIME.TODAY()
2400 DBF APPEND BLANK
2410 DBF REPLACE ID WITH 1, Nome WITH "Mario Updated", Citta WITH "Roma", Saldo WITH 20.00, Attivo WITH TRUE, UltimoOrdine WITH DATETIME.TODAY()
2420 DBF INDEX ON ID TO "idx_id_mov.ntx" ' Necessario indice per UPDATE ON/JOIN
2430 DBF SET ORDER TO "idx_id_mov"
2440 DBF SELECT Clienti ' Torna al database principale

' Aggiorna i record della tabella corrente usando i dati della tabella 'Movimenti'.
2450 DBF UPDATE ON ID FROM Movimenti REPLACE Nome WITH Movimenti.Nome, Saldo WITH Saldo + Movimenti.Saldo
2460 PRINT "Aggiornato il record ID=7 e ID=1 dal database 'Movimenti'."
2470 DBF SEEK "Mario Updated" ' Cerca Mario Updated
2480 IF DBF.FOUND() THEN
2490     DBF GET clienteRec
2500     PRINT "  Record ID=1 (Mario Rossi) ora è: Nome: " + clienteRec.Nome + ", Saldo: " + CONVERSIONS.CSTR(clienteRec.Saldo)
2510 END IF
2520 DBF SEEK "Cliente Update" ' Cerca Cliente Update
2530 IF DBF.FOUND() THEN
2540     DBF GET clienteRec
2550     PRINT "  Record ID=7 (Cliente Update) ora è: Nome: " + clienteRec.Nome + ", Saldo: " + CONVERSIONS.CSTR(clienteRec.Saldo)
2560 END IF

' Crea una nuova tabella unendo i record di due tabelle.
2570 // JOIN WITH con Lambda
2580 DBF JOIN WITH Movimenti TO "clienti_movimenti.dbf" FOR FN(Clienti, Movimenti) => Clienti.ID = Movimenti.ID FIELDS Clienti.Nome, Movimenti.Saldo, Movimenti.Citta
2590 PRINT "Creato 'clienti_movimenti.dbf' unendo Clienti e Movimenti."
2600 DBF USE "clienti_movimenti.dbf" ALIAS "ClientiMov"
2610 DBF SELECT ClientiMov
2620 PRINT "Contenuto del file unito:"
2630 DBF GO TOP
2640 DO WHILE NOT DBF.EOF()
2650     DBF GET clienteRec ' Reusa clienteRec, i campi saranno mappati per nome
2660     PRINT "  Nome: " + clienteRec.Nome + ", Saldo Movimento: " + CONVERSIONS.CSTR(clienteRec.Saldo) + ", Citta Movimento: " + clienteRec.Citta
2670     DBF SKIP
2680 LOOP
2690 DBF CLOSE
2700 DBF SELECT Clienti

2710 // ==============================================================================
2720 // Transazioni: BEGIN TRANSACTION, COMMIT, ROLLBACK
2730 // ==============================================================================
2740 PRINT
2750 PRINT "-------------------------------------------------------------------------------"
2760 PRINT "8. Test Transazioni (BEGIN TRANSACTION, COMMIT, ROLLBACK)"
2770 PRINT "-------------------------------------------------------------------------------"

' Test di annullamento modifiche.
2780 DBF BEGIN TRANSACTION LEVEL REPEATABLE READ
2790    PRINT "Iniziata transazione. Saldo attuale di ID=1:"
2800    DBF SEEK "Mario Updated"
2810    DBF GET clienteRec
2820    PRINT "  Prima modifica - Saldo ID=1: " + CONVERSIONS.CSTR(clienteRec.Saldo)
2830    DBF REPLACE Saldo WITH Saldo + 100
2840    DBF GET clienteRec ' Rilegge il record modificato nella transazione
2850    PRINT "  Dopo modifica (non commessa) - Saldo ID=1: " + CONVERSIONS.CSTR(clienteRec.Saldo)
2860 DBF ROLLBACK
2870 PRINT "Rollback eseguito. Verifico saldo di ID=1:"
2880 DBF SEEK "Mario Updated"
2890 DBF GET clienteRec
2900 PRINT "  Dopo Rollback - Saldo ID=1: " + CONVERSIONS.CSTR(clienteRec.Saldo) ' Dovrebbe essere lo stato iniziale

' Test di salvataggio modifiche.
2910 DBF BEGIN TRANSACTION
2920    PRINT "Iniziata nuova transazione. Saldo attuale di ID=2:"
2930    DBF SEEK "Giulia Verdi"
2940    DBF GET clienteRec
2950    PRINT "  Prima modifica - Saldo ID=2: " + CONVERSIONS.CSTR(clienteRec.Saldo)
2960    DBF REPLACE Saldo WITH Saldo - 50
2970    DBF GET clienteRec ' Rilegge
2980    PRINT "  Dopo modifica (non commessa) - Saldo ID=2: " + CONVERSIONS.CSTR(clienteRec.Saldo)
2990 DBF COMMIT
3000 PRINT "Commit eseguito. Verifico saldo di ID=2:"
3010 DBF SEEK "Giulia Verdi"
3020 DBF GET clienteRec
3030 PRINT "  Dopo Commit - Saldo ID=2: " + CONVERSIONS.CSTR(clienteRec.Saldo) ' Dovrebbe mostrare la modifica

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
3140 DBF SEEK "Record Cancellato" ' Ricerco il record "Record Cancellato" che era stato richiamato
3150 IF DBF.FOUND() THEN
3160    DBF DELETE // Lo cancello di nuovo
3170    PRINT "Record ID=6 marcato come eliminato per test PACK."
3180 END IF

3190 DBF PACK 
3200 PRINT "Eseguito PACK su 'clienti.dbf'. I record cancellati sono stati rimossi fisicamente."
3210 PRINT "Totale record dopo PACK: " + STR(DBF.LASTREC())
' NOTA: PACK può riposizionare l'area di lavoro o ricrearla, quindi potremmo dover ri-selezionare
3220 // NOTA: PACK può riposizionare l'area di lavoro o ricrearla, quindi potremmo dover ri-selezionare
3230 DBF SELECT Clienti

' Ricostruisce tutti gli indici associati.
3240 DBF REINDEX 
3250 PRINT "Eseguito REINDEX su 'clienti.dbf'. Indici ricostruiti."

' Rimuove tutti i record, lasciando la struttura.
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

' Aggiunge alcuni record per l'esportazione.
3360 // Aggiungiamo alcuni record per l'esportazione
3370 DBF APPEND BLANK
3380 DBF REPLACE ID WITH 100, Nome WITH "Export Test", Citta WITH "Roma", Saldo WITH 1000.00, Attivo WITH TRUE, UltimoOrdine WITH DATETIME.TODAY()
3390 DBF APPEND BLANK
3400 DBF REPLACE ID WITH 101, Nome WITH "Export Test 2", Citta WITH "Milano", Saldo WITH 2000.00, Attivo WITH TRUE, UltimoOrdine WITH DATETIME.TODAY()

' Esporta i dati in formato CSV e JSON.
3410 DBF EXPORT TO "clienti_export.csv" AS CSV // Sintassi Legacy
3420 PRINT "Esportato 'clienti.dbf' in 'clienti_export.csv'."

3430 DBF EXPORT TO "clienti_export.json" AS JSON
3440 PRINT "Esportato 'clienti.dbf' in 'clienti_export.json'."

3450 // ==============================================================================
3460 // Pulizia Finale
3470 // ==============================================================================
3480 PRINT
3490 PRINT "-------------------------------------------------------------------------------"
3500 PRINT "11. Pulizia Finale"
3510 PRINT "-------------------------------------------------------------------------------"

' Chiusura di tutti i database aperti.
3520 DBF CLOSE DATABASES // Sintassi Legacy
3530 PRINT "Chiusi tutti i database."

' Rimozione di tutti i file generati durante il test.
3540 ON ERROR RESUME NEXT
3550 KILL "clienti.dbf"
3560 KILL "idx_nome.ntx"
3570 KILL "idx_citta_nome.ntx"
3580 KILL "clienti_roma.dbf"
3590 KILL "tot_citta.dbf"
3600 KILL "clienti_ordinati.dbf"
3610 KILL "movimenti.dbf"
3620 KILL "idx_id_mov.ntx"
3630 KILL "clienti_movimenti.dbf"
3640 KILL "clienti_export.csv"
3650 KILL "clienti_export.json"
3660 KILL "clienti.gms"
3670 KILL "clienti.gmv"
3680 KILL "movimenti.gms"
3690 KILL "movimenti.gmv"
3700 KILL "tot_citta.gms"
3710 KILL "tot_citta.gmv"
3720 KILL "clienti_ordinati.gms"
3730 KILL "clienti_ordinati.gmv"
3740 KILL "clienti_roma.gms"
3750 KILL "clienti_roma.gmv"
3760 KILL "clienti_movimenti.gms"
3770 KILL "clienti_movimenti.gmv"
3780 RMDIR cPath // Rimuove la directory temporanea
3790 ON ERROR GOTO 0 ' Ripristina la gestione standard degli errori.
3800 PRINT "File di test e directory eliminati."

3810 PRINT
3820 PRINT "Test DBF Completato!"
3830 END
