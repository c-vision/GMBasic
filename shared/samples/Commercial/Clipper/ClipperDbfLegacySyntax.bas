' FILE: Clipper/ClipperDbfBothSytaxes.bas
' =================================================================================================
'       GUIDA TEORICA: SINTASSI DBF LEGACY (sintassi spazio)
' =================================================================================================
'
' Questo file di test è una suite di integrazione completa che dimostra e verifica
' il supporto di GM-BASIC per la manipolazione dei file DBF tramite la sintassi
' Legacy Prefissata (sintassi spazio: `DBF USE ...`, `DBF SET DEFAULT TO ...`).
'
' NOTA: I comandi DBF usano SOLO la sintassi legacy a spazio. Non esiste sintassi
' qualificata per i comandi DBF (`DBF.USE ...` non è valida). Le FUNZIONI DBF che
' restituiscono un valore usano invece la sintassi qualificata con parentesi
' (es. `DBF.EOF()`, `DBF.RECNO()`).
'
' 1. SINTASSI LEGACY PREFISSATA (es. `DBF USE ...`)
'    - Scopo: Mantenere una certa compatibilità con il codice Clipper/xBase.
'    - Requisito: A differenza del Clipper originale, in GM-BASIC i comandi DBF devono essere
'      preceduti dalla keyword `DBF` per risolvere le ambiguità con altri comandi del linguaggio
'      (es. `SELECT`, `CLOSE`, `DELETE`).
'    - Attivazione: Richiede l'import del namespace (`IMPORT DBF` o `IMPORT DBF WITH ALIAS`).
'    - Caratteristiche: Utilizza una sintassi a parole chiave e clausole in ordine flessibile
'      (es. `... FOR condizione`, `... ALIAS nome`).
'
' 2. GESTIONE DEI TIPI E LATE BINDING
'    - `AS Tipo`: La clausola `AS Cliente` nel comando `USE` lega staticamente la work area
'      a un `TYPE` definito dall'utente. Questo abilita il controllo dei tipi a tempo di
'      compilazione, l'autocompletamento (IntelliSense) e previene errori di battitura sui nomi dei campi.
'    - Late Binding (Senza `AS`): Se la clausola `AS` viene omessa, GM-BASIC entra in una modalità
'      di "late binding" per quella work area, emulando il comportamento di Clipper. I nomi
'      dei campi vengono risolti a runtime, offrendo flessibilità ma a scapito della sicurezza dei tipi.
'
' 4. FUNZIONI DI STATO vs. COMANDI
'    - Comandi: Modificano lo stato (es. `DBF GO TOP`, `DBF DELETE`).
'    - Funzioni: Restituiscono un valore (es. `DBF.FOUND()`, `DBF.RECNO()`). Tutte le funzioni
'      di stato in GM-BASIC richiedono la sintassi qualificata.
'
' Questo test esegue una serie di operazioni (SET, USE, APPEND, REPLACE, query, etc.)
' prima con la sintassi legacy e poi con quella moderna, per verificare che il risultato
' finale sia identico e che entrambe le API siano pienamente funzionanti.
'
' =================================================================================================

' Importa i namespace necessari. "WITH ALIAS" permette di usare i nomi dei namespace direttamente.
IMPORT DBF WITH ALIAS
IMPORT CONVERSIONS WITH ALIAS
IMPORT IO WITH ALIAS
IMPORT DATETIME WITH ALIAS
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

10 PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
20 PRINT "║ ClipperDbfBothSytaxes - DBF (Sintassi Legacy)                     ║"
30 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
40 PRINT
60 // ==============================================================================
70 // Setup Iniziale e Definizione Tipi
80 // ==============================================================================
90 TYPE Cliente
100    ID AS INTEGER
110    Nome AS STRING * 20
120    Citta AS STRING * 15
130    Saldo AS CURRENCY
140    Attivo AS BIT
150    UltimoOrdine AS DATE
160 END TYPE
170 
180 DIM cPath AS STRING, clienteRec AS Cliente, totaliRec AS Cliente
190 
200 CLS
' Prepara una directory pulita per i file di test.
210 cPath = g_work + "/dbf_test_suite_data"
220 IF IO.DirExists(cPath) THEN RMDIR cPath, TRUE
230 MKDIR cPath
240 
250 // ==============================================================================
260 // 1. Comandi SET: Impostano l'ambiente di lavoro per il motore DBF.
270 // ==============================================================================
280 PRINT "1. Test Comandi SET"
' Sintassi Legacy per impostare la directory di default e la modalità esclusiva.
290 DBF SET DEFAULT TO cPath
300 DBF SET EXCLUSIVE ON
' Imposta il percorso di ricerca e disattiva la modalità esclusiva.
310 DBF SET PATH TO cPath
320 DBF SET EXCLUSIVE OFF
' Test incrociato per le altre impostazioni.
330 DBF SET SCHEMA STRICT OFF
340 DBF SET SCHEMA STRICT ON
350 PRINT "  SET DEFAULT, PATH, EXCLUSIVE, SCHEMA eseguiti."
360 
370 // ==============================================================================
380 // 2. Creazione, Apertura e Selezione Database
390 // ==============================================================================
400 PRINT "2. Test Apertura e Selezione"
' Crea un file DBF vuoto usando la struttura del tipo 'Cliente' come modello.
410 DBF COPY STRUCTURE TO "clienti.dbf"
420 
' Apre il file DBF usando la sintassi legacy, assegnando un alias e legandolo staticamente al tipo 'Cliente'.
430 DBF USE "clienti.dbf" ALIAS Clienti1 AS Cliente
440 PRINT "  USE (Legacy): Aperto 'clienti.dbf' come 'Clienti1'."
450 DBF CLOSE DATABASES
460 
' Apre lo stesso file in una nuova work area (legacy).
470 DBF USE "clienti.dbf" ALIAS "Clienti2" AS Cliente
480 PRINT "  USE (Qualificato): Aperto 'clienti.dbf' come 'Clienti2'."
490 
' Seleziona la prima work area usando il suo alias (sintassi legacy).
500 DBF SELECT Clienti1
510 PRINT "  SELECT (Legacy by Alias): Selezionata area 'Clienti1'."
520 
' Seleziona la seconda work area usando il suo numero.
530 DBF SELECT 2
540 PRINT "  SELECT (Qualificato by Number): Selezionata area 2 ('Clienti2')."
550 
560 // ==============================================================================
570 // 3. Aggiunta Record: Dimostra i vari modi per popolare un file DBF.
580 // ==============================================================================
590 PRINT "3. Test Aggiunta Record"
600 DBF SELECT Clienti1 ' Lavoriamo sulla prima work area
610 DBF APPEND BLANK ' Aggiunge un record vuoto.
' Popola il record campo per campo con REPLACE (stile classico).
620 DBF REPLACE ID WITH 1, Nome WITH "Mario Rossi", Citta WITH "Roma", Saldo WITH 100
630 PRINT "  APPEND BLANK + REPLACE (Legacy): Aggiunto 'Mario Rossi'."
640 
' Equivalente legacy, con dictionary literal per REPLACE.
650 DBF APPEND BLANK
660 DBF REPLACE ID WITH 2, Nome WITH "Giulia Verdi", Citta WITH "Milano", Saldo WITH 200
670 PRINT "  APPENDBLANK + REPLACE (Qualificato): Aggiunto 'Giulia Verdi'."
680 
' Aggiunge un intero record da una variabile UDT pre-popolata.
690 clienteRec.ID = 3 : clienteRec.Nome = "Luca Neri" : clienteRec.Citta = "Roma" : clienteRec.Saldo = 300
700 DBF APPEND RECORD clienteRec
710 PRINT "  APPEND RECORD (Legacy): Aggiunto 'Luca Neri'."
720 
' Equivalente legacy.
730 clienteRec.ID = 4 : clienteRec.Nome = "Anna Gialli" : clienteRec.Citta = "Torino" : clienteRec.Saldo = 400
740 DBF APPEND RECORD clienteRec
750 PRINT "  APPEND RECORD (Qualificato): Aggiunto 'Anna Gialli'."
760 
' Aggiunge record da un altro file DBF.
770 DBF COPY STRUCTURE TO "nuovi.dbf"
780 DBF USE "nuovi.dbf" ALIAS "Nuovi" AS Cliente
790 DBF APPEND RECORD {ID: 5, Nome: "Paolo Blu", Citta: "Milano", Saldo: 500}
800 DBF APPEND FROM "nuovi.dbf" ' Aggiunge tutti i record dal file specificato.
810 PRINT "  APPEND FROM (Legacy): Aggiunto record da 'nuovi.dbf'."
820 
' Equivalente moderno con un filtro lambda per aggiungere solo record specifici.
830 DBF APPEND FROM "nuovi.dbf" FOR FN(rec) => rec.ID = 5
840 PRINT "  APPENDFROM (Qualificato): Aggiunto record da 'nuovi.dbf' con filtro."
850 DBF CLOSE : DBF SELECT Clienti1
860 
870 // ==============================================================================
880 // 4. Navigazione e Lettura
890 // ==============================================================================
900 PRINT "4. Test Navigazione e Lettura"
' Posizionamento assoluto nel file.
910 DBF GO TOP
920 PRINT "  GO TOP (Legacy): OK"
930 DBF GO BOTTOM
940 PRINT "  GO (Qualificato): OK"
950 
' Spostamento relativo del puntatore del record.
960 DBF SKIP -1
970 PRINT "  SKIP (Legacy): OK"
980 DBF SKIP -1
990 PRINT "  SKIP (Qualificato): OK"
1000 
' Ricerca tramite un indice.
1010 DBF INDEX ON ID TO "idx_id"
1020 DBF SET ORDER TO 1
1030 DBF SEEK 1
1040 PRINT "  SEEK (Legacy): OK, Found=" + CONVERSIONS.CSTR(DBF.FOUND())
1050 DBF SEEK 2
1060 PRINT "  SEEK (Qualificato): OK, Found=" + CONVERSIONS.CSTR(DBF.FOUND())
1070 
' Lettura del record corrente in una variabile UDT.
1080 DBF GET clienteRec
1090 PRINT "  GET (Legacy): Letto record ID " + STR(clienteRec.ID)
1100 DBF GET clienteRec
1110 PRINT "  GET (Qualificato): Letto record ID " + STR(clienteRec.ID)
1120 
1130 // ==============================================================================
1140 // 5. Modifica e Cancellazione
1150 // ==============================================================================
1160 PRINT "5. Test Modifica e Cancellazione"
1170 DBF GO TOP
' Marcatura per cancellazione del record corrente.
1180 DBF DELETE
1190 PRINT "  DELETE (Legacy, current): Record 1 marcato."
' Annullamento della marcatura per cancellazione.
1200 DBF RECALL
1210 PRINT "  RECALL (Qualificato, current): Record 1 ripristinato."
1220 
' Marcatura condizionale usando una lambda.
1230 DBF DELETE FOR FN(rec) => rec.ID=1
1240 PRINT "  DELETE (Qualificato, FOR): Record 1 marcato di nuovo."
' Annullamento marcatura condizionale con sintassi legacy.
1250 DBF RECALL FOR ID = 1
1260 PRINT "  RECALL (Legacy, FOR): Record 1 ripristinato di nuovo."
1270 
' Rimozione fisica dei record marcati.
1280 DBF DELETE FOR ID = 1
1290 DBF PACK
1300 PRINT "  PACK (Legacy): Eseguito. Record totali: " + STR(DBF.LASTREC())
1310 DBF DELETE FOR FN(rec) => rec.ID=2
1320 DBF PACK
1330 PRINT "  PACK (Qualificato): Eseguito. Record totali: " + STR(DBF.LASTREC())
1340 
' Rimozione di tutti i record dal file.
1350 DBF COPY TO "clienti_backup_temp.dbf" ' Salva una copia per i test successivi
1360 DBF ZAP
1370 PRINT "  ZAP (Legacy): Eseguito. Record totali: " + STR(DBF.LASTREC())
1380 DBF APPEND FROM "clienti_backup_temp.dbf" ' Ripristina per ZAP qualificato
1390 DBF ZAP
1400 PRINT "  ZAP (Qualificato): Eseguito. Record totali: " + STR(DBF.LASTREC())
1410 
1420 DBF APPEND FROM "clienti_backup_temp.dbf" ' Ripristina i dati per i test finali
1430 
1440 // ==============================================================================
1450 // 6. Query e Report
1460 // ==============================================================================
1470 PRINT "6. Test Query e Report"
' Ordinamento fisico di un file in un nuovo file.
1480 DBF SORT ON Citta TO "sorted_legacy.dbf"
1490 PRINT "  SORT TO (Legacy): Creato 'sorted_legacy.dbf'."
1500 DBF SORT TO "sorted_qual.dbf" ON Saldo /D FOR FN(rec) => rec.Attivo = TRUE
1510 PRINT "  SORTTO (Qualificato): Creato 'sorted_qual.dbf'."
1520 
' Aggregazione di dati in un nuovo file sommando i campi specificati.
1530 DBF TOTAL ON Citta TO "total_legacy.dbf" FIELDS Saldo
1540 PRINT "  TOTAL ON (Legacy): Creato 'total_legacy.dbf'."
1550 DBF TOTAL ON Citta TO "total_qual.dbf" FIELDS Saldo FOR FN(rec) => rec.Attivo = TRUE
1560 PRINT "  TOTAL (Qualificato): Creato 'total_qual.dbf'."
1570 
' Setup per i comandi relazionali: crea una seconda tabella 'Movimenti'.
1580 DBF USE "movimenti.dbf" UNSAFE ALIAS "Movimenti" AS Cliente
1590 DBF APPEND RECORD {ID:1, Saldo: 50, Nome: "Nuovo Nome"}
1600 DBF INDEX ON ID TO "idx_mov_id.ntx"
1610 DBF SELECT Clienti1
1620 
' Aggiorna i record della tabella corrente usando i dati di un'altra tabella.
1630 DBF UPDATE ON ID FROM Movimenti REPLACE Saldo WITH Saldo + Movimenti.Saldo
1640 PRINT "  UPDATE ON (Legacy): Eseguito."
1650 DBF UPDATE ON ID FROM Movimenti REPLACE Nome WITH Movimenti.Nome
1660 PRINT "  UPDATEON (Qualificato): Eseguito."
1670 
' Crea una nuova tabella unendo i record di due tabelle.
1680 DBF JOIN WITH Movimenti TO "join_legacy.dbf" FOR ID = Movimenti.ID
1690 PRINT "  JOIN WITH (Legacy): Eseguito."
1700 DBF JOIN WITH Movimenti TO "join_qual.dbf" FOR FN(cli, mov) => cli.ID = mov.ID
1710 PRINT "  JOINWITH (Qualificato): Eseguito."
1720 
1730 // ==============================================================================
1740 // 7. Funzioni di Stato: verificano lo stato corrente del motore DBF
1750 // ==============================================================================
1760 PRINT "7. Test Funzioni di Stato"
' Nota: Le funzioni di stato richiedono sempre la sintassi qualificata.
1770 PRINT "  RECNO: " + STR(DBF.RECNO())
1780 PRINT "  EOF: " + CONVERSIONS.CSTR(DBF.EOF())
1790 PRINT "  BOF: " + CONVERSIONS.CSTR(DBF.BOF())
1800 PRINT "  USED: " + CONVERSIONS.CSTR(DBF.USED())
1810 PRINT "  ALIAS: " + DBF.ALIAS()
1820 PRINT "  LUPDATE: " + CONVERSIONS.CSTR(DBF.LUPDATE())
1830 PRINT "  RECSIZE: " + STR(DBF.RECSIZE())
1840 PRINT "  FIELDNAME(1): " + DBF.FIELDNAME(1)
1850 PRINT "  INDEXORD: " + STR(DBF.INDEXORD())
1860 PRINT "  INDEXKEY(0): " + DBF.INDEXKEY(0)
1870 PRINT "  DBRELATION: " + DBF.DBRELATION()
1880 PRINT "  RLOCK/FLOCK/NETERR: " + CONVERSIONS.CSTR(DBF.RLOCK()) + "," + CONVERSIONS.CSTR(DBF.FLOCK()) + "," + CONVERSIONS.CSTR(DBF.NETERR())
1990 
2000 // ==============================================================================
2010 // 8. Pulizia Finale
2020 // ==============================================================================
2030 PRINT "8. Pulizia Finale"
2040 DBF CLOSE DATABASES
2050 RMDIR cPath, TRUE
2060 PRINT "  File e directory di test rimossi."
2070 
2080 PRINT "Test DBF Completato!"
2090 END
