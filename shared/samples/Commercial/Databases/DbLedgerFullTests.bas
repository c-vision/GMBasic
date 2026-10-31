' FILE: Databases/DbLedgerFullTests.bas
' =================================================================================================
'       GUIDA TEORICA: SINTASSI LEDGER (LEGACY)
' =================================================================================================
'
' Questo file di test è una suite di integrazione per verificare il corretto funzionamento
' del motore LEDGER (contabilità a partita doppia) di GM-BASIC, in sintassi legacy.
'
' 1. SINTASSI LEGACY (PREFISSATA)
'    - Stile: Ispirato ai comandi classici, usa parole chiave in ordine naturale.
'    - Requisito: Il namespace LEDGER deve essere importato (o il comando prefissato `LEDGER`).
'    - Caratteristiche: Usa la clausola `INTO` per restituire valori e la clausola `WITH`
'      per passare parametri complessi.
'    - Esempio: `LEDGER ADD hndl WITH date, "desc" {postings} INTO newId`
'
' 2. FUNZIONI
'    - Le funzioni che restituiscono un valore usano la sintassi qualificata con parentesi
'      (es. `h = LEDGER.OPEN("file.dat")`). I comandi LEDGER usano SOLO la sintassi legacy
'      a spazio; non esiste forma qualificata per i comandi (`LEDGER.ADD ...` non è valida).
'    - Esempio: `LEDGER.ADD hndl WITH date, "desc" {postings} INTO newId`
'
' 3. CONCETTI CHIAVE LEDGER
'    - Transazione: Un'operazione atomica composta da data, descrizione e una serie di "postings".
'    - Posting: Una singola riga contabile che associa un conto a un importo. La somma dei posting
'      di una transazione deve essere zero (partita doppia).
'    - Query: Stringa di filtro (es. `account:"Spese:*"`) per selezionare transazioni o conti.
'
' =================================================================================================

IMPORT JSON
IMPORT LEDGER
10 IMPORT LEDGER WITH ALIAS
20 IMPORT CONVERSIONS WITH ALIAS
30 IMPORT DATETIME WITH ALIAS
40 IMPORT IO WITH ALIAS
50 IMPORT JSON WITH ALIAS
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
60 
70 CLS
80 PRINT  "╔═════════════════════════════════════════════════════════════════════════════╗"
90 PRINT  "║ DbLedgerFullTests - LEDGER-CLi Test Suite (Sintassi Legacy)                 ║"
100 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
110 PRINT
120 
130 DIM cPath AS STRING
140 DIM hLedger AS INTEGER
150 DIM cAccount AS STRING
160 DIM cResult AS STRING
170 DIM dDate AS DATE
180 DIM tuttiConti$() AS STRING 
190 DIM isValid AS BIT
200 DIM nRet AS INTEGER 
210 
220 REM Percorso temporaneo per i file Ledger
230 cPath = g_work + "/ledger_test_data"
240 MKDIR cPath
250 CHDIR cPath
260 
270 PRINT "-------------------------------------------------------------------------------"
280 PRINT "1. Creazione e Apertura del Database Ledger"
290 PRINT "-------------------------------------------------------------------------------"
300 REM Sintassi Legacy OPEN (Comando)
' Apre un file ledger e assegna l'handle alla variabile specificata dopo AS.
310 LEDGER OPEN "contabilita_legacy.dat" AS hLedger
320 PRINT "Database Ledger Legacy aperto. Handle: " + STR(hLedger)
330 LEDGER CLOSE hLedger
340 
350 REM Sintassi legacy OPEN (Funzione)
' Apre il file e restituisce l'handle come valore di ritorno.
360 hLedger = LEDGER.OPEN("contabilita_qual.dat")
370 PRINT "Database Ledger legacy aperto. Handle: " + STR(hLedger)
380 REM Sintassi legacy Fusa OPEN (Comando)
' Apre il file e assegna l'handle alla variabile specificata dopo AS.
390 DIM hCmd AS INTEGER
400 LEDGER OPEN "contabilita_qual_cmd.dat" AS hCmd
410 PRINT "Database Ledger legacy (comando) aperto. Handle: " + STR(hCmd)
420 LEDGER CLOSE hCmd
430 KILL "contabilita_qual_cmd.dat"
430 
440 REM ==============================================================================
450 REM 2. Aggiunta di Transazioni (ADD)
460 REM ==============================================================================
470 PRINT
480 PRINT "-------------------------------------------------------------------------------"
490 PRINT "2. Aggiunta di Transazioni (ADD)"
500 PRINT "-------------------------------------------------------------------------------"
510 dDate = DATETIME.DATEVALUE("2023-01-01")
520 
530 REM --- ADD LEGACY (Comando con INTO) ---
' Usa WITH per passare data e descrizione, e un dizionario literal per i posting.
' INTO specifica la variabile che riceverà l'ID della nuova transazione.
540 LEDGER ADD hLedger WITH dDate, "Salario Gennaio Legacy" {
     "Attivita:Banca": 900.00,
     "Entrate:Salario": -1000.00,
     "Spese:Alimenti:Cena": 100.00
} INTO nRet
590 PRINT "Aggiunta transazione Legacy con ID: " + STR(nRet)
600 
610 REM --- ADD legacy (Comando con INTO) ---
' Usa WITH per passare data e descrizione, e un dizionario literal per i posting.
' INTO specifica la variabile che riceverà l'ID della nuova transazione.
620 LEDGER ADD hLedger WITH dDate, "Acquisto Libri legacy" {
     "Spese:Libri": 50.00,
     "Attivita:Contanti": -50.00
} INTO nRet
660 PRINT "Aggiunta transazione legacy (Funzione) con ID: " + STR(nRet)
670 
680 REM --- ADD legacy (Comando con INTO) ---
' WITH specifica data e descrizione, {} i posting, INTO la variabile di output (nRet).
690 LEDGER ADD hLedger WITH dDate, "Benzina legacy" { "Spese:Auto": 65.00, "Attivita:Banca": -65.00 } INTO nRet
700 PRINT "Aggiunta transazione legacy (Comando) con ID: " + STR(nRet)
710 
720 
730 REM ==============================================================================
740 REM 3. Modifica Transazioni (EDIT)
750 REM ==============================================================================
760 PRINT
770 PRINT "-------------------------------------------------------------------------------"
780 PRINT "3. Modifica Transazioni (EDIT)"
790 PRINT "-------------------------------------------------------------------------------"
800 
810 REM --- EDIT LEGACY (Comando con INTO) ---
' Modifica la transazione con ID 1, sostituendola con i nuovi dati.
820 LEDGER EDIT hLedger ID 1 WITH DATETIME.DATEADD("D", 1, dDate), "Salario Aggiornato Legacy" {
     "Attivita:Banca": 950.00,
     "Entrate:Salario": -1000.00,
     "Spese:Alimenti:Cena": 50.00
} INTO nRet
870 PRINT "Modificata transazione (Legacy). Nuovo ID: " + STR(nRet)
880 
890 REM --- EDIT legacy (Comando con INTO) ---
' Modifica la transazione ID nRet, con WITH per i nuovi dati e INTO per il nuovo ID.
900 nRet = 2
910 LEDGER EDIT hLedger ID nRet WITH DATETIME.DATEADD("D", 2, dDate), "Libri Aggiornati legacy" {
     "Spese:Libri": 60.00,
     "Attivita:Contanti": -60.00
} INTO nRet
950 PRINT "Modificata transazione (Funzione). Nuovo ID: " + STR(nRet)
960 
970 REM --- EDIT legacy (Comando con INTO) ---
' Modifica la transazione ID 3, con INTO per il nuovo ID.
980 LEDGER EDIT hLedger ID 3 WITH dDate, "Benzina Corretta" { "Spese:Auto": 65.00, "Attivita:Banca": -65.00 } INTO nRet
990 PRINT "Modificata transazione (Comando). Nuovo ID: " + STR(nRet)
1000 
1010 
1020 REM ==============================================================================
1030 REM 4. Query e Reporting
1040 REM ==============================================================================
1050 PRINT
1060 PRINT "-------------------------------------------------------------------------------"
1070 PRINT "4. Query e Reporting"
1080 PRINT "-------------------------------------------------------------------------------"
1090 
1100 REM --- REGISTER LEGACY (Comando con INTO) ---
' Genera un report delle transazioni che matchano la query "Spese:Libri".
1110 LEDGER REGISTER hLedger WHERE "account:""Spese:Libri""" INTO tuttiConti$
1120 PRINT "Registro Legacy (Spese:Libri): " + JSON.Stringify(tuttiConti$)
1130 
1140 REM --- REGISTER legacy (Comando con INTO) ---
' Genera il report nella variabile array tuttiConti$ usando la clausola WHERE.
1150 LEDGER REGISTER hLedger WHERE "account:""Spese:Auto""" INTO tuttiConti$
1160 PRINT "Registro legacy (Comando, Spese:Auto): " + JSON.Stringify(tuttiConti$)
1170 
1180 REM --- REGISTER legacy (Comando con INTO) ---
' Genera il report come array di stringhe JSON in tuttiConti$.
1190 LEDGER REGISTER hLedger WHERE "account:""Entrate:*""" INTO tuttiConti$
1200 PRINT "Registro legacy (Funzione, Entrate): " + JSON.Stringify(tuttiConti$)
1210 
1220 REM --- BALANCE LEGACY (Comando con INTO) ---
' Calcola i salti per i conti che matchano la query.
1230 LEDGER BALANCE hLedger WHERE "Attivita:Banca" INTO tuttiConti$
1240 PRINT "Bilancio Legacy (Attivita:Banca): " + JSON.Stringify(tuttiConti$)
1250 
1260 REM --- BALANCE legacy (Comando con INTO) ---
1270 LEDGER BALANCE hLedger WHERE "Attivita:Contanti" INTO tuttiConti$
1280 PRINT "Bilancio legacy (Comando, Attivita:Contanti): " + JSON.Stringify(tuttiConti$)
1290 
1300 REM --- BALANCE legacy (Comando con INTO) ---
1310 LEDGER BALANCE hLedger WHERE "Spese" INTO tuttiConti$
1320 PRINT "Bilancio legacy (Funzione, Spese): " + JSON.Stringify(tuttiConti$)
1330 
1340 REM --- GET LEGACY (Comando con INTO) ---
' Recupera i dettagli completi di una transazione specifica per ID.
1350 LEDGER GET hLedger ID 4 INTO tuttiConti$
1360 PRINT "Dettagli Transazione ID 4 (GET Legacy): " + JSON.Stringify(tuttiConti$)
1370 
1380 REM --- GET legacy (Comando con INTO) ---
1390 LEDGER GET hLedger ID 5 INTO tuttiConti$
1400 PRINT "Dettagli Transazione ID 5 (GET Funzione): " + JSON.Stringify(tuttiConti$)
1410 
1420 REM --- GET legacy (Comando con INTO) ---
1430 LEDGER GET hLedger ID 6 INTO tuttiConti$
1440 PRINT "Dettagli Transazione ID 6 (GET Comando): " + JSON.Stringify(tuttiConti$)
1450 
1460 REM --- ACCOUNTS LEGACY (Comando con INTO) ---
' Restituisce la lista di tutti i conti definiti nel ledger.
1470 LEDGER ACCOUNTS hLedger INTO tuttiConti$
1480 PRINT "Lista Account Legacy: " + JSON.Stringify(tuttiConti$)
1490 
1500 REM --- ACCOUNTS legacy (Comando con INTO) ---
1510 LEDGER ACCOUNTS hLedger INTO tuttiConti$
1520 PRINT "Lista Account legacy (Funzione): " + JSON.Stringify(tuttiConti$)
1530 
1540 REM --- ACCOUNTS legacy (Comando con INTO) ---
1550 LEDGER ACCOUNTS hLedger INTO tuttiConti$
1560 PRINT "Lista Account legacy (Comando): " + JSON.Stringify(tuttiConti$)
1570 
1580 
1590 REM ==============================================================================
1600 REM 5. Manutenzione
1610 REM ==============================================================================
1620 PRINT
1630 PRINT "-------------------------------------------------------------------------------"
1640 PRINT "5. Manutenzione"
1650 PRINT "-------------------------------------------------------------------------------"
1660 
1670 REM --- VALIDATE LEGACY (Comando con INTO) ---
' Verifica l'integrità del file ledger (checksum, coerenza partita doppia).
1680 LEDGER VALIDATE hLedger INTO isValid
1690 PRINT "Validazione Legacy: " + CONVERSIONS.CSTR(isValid)
1700 
1710 REM --- VALIDATE legacy (Comando con INTO) ---
1720 LEDGER VALIDATE hLedger INTO isValid
1730 PRINT "Validazione legacy (Comando): " + CONVERSIONS.CSTR(isValid)
1740 
1750 REM --- VALIDATE legacy (Comando con INTO) ---
1760 LEDGER VALIDATE hLedger INTO isValid
1770 PRINT "Validazione legacy (Funzione): " + CONVERSIONS.CSTR(isValid)
1780 
1790 REM --- DELETE LEGACY (Comando) ---
' Marca una transazione come cancellata.
1800 LEDGER DELETE hLedger ID 4
1810 PRINT "Transazione ID 4 cancellata (Legacy)."
1820 
1830 REM --- DELETE legacy (Comando) ---
1840 LEDGER DELETE hLedger ID 5
1850 PRINT "Transazione ID 5 cancellata (legacy)."
1860 
1870 REM --- PACK LEGACY (Comando) ---
' Rimuove fisicamente le transazioni cancellate e compatta il file.
1880 LEDGER PACK hLedger
1890 PRINT "Eseguito PACK (Legacy)."
1900 
1910 REM --- PACK legacy (Comando) ---
1920 LEDGER PACK hLedger
1930 PRINT "Eseguito PACK (legacy)."
1940 
1950 REM ==============================================================================
1960 REM 6. Pulizia Finale
1970 REM ==============================================================================
1980 LEDGER CLOSE hLedger
1990 PRINT "Database chiuso."
2000 
2010 CHDIR ".."
2020 KILL cPath + "/contabilita_qual.dat"
2030 KILL cPath + "/contabilita_qual.idx"
2035 KILL cPath + "/contabilita_qual_cmd.dat"
2037 KILL cPath + "/contabilita_qual_cmd.idx"
2040 KILL cPath + "/contabilita_legacy.dat"
2050 KILL cPath + "/contabilita_legacy.idx"
2060 RMDIR cPath
2070 PRINT "Pulizia completata."
2080 
2090 END
