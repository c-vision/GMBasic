' FILE: Databases/DbSpeedFullTests.bas
' =================================================================================================
'       GUIDA TEORICA: SPEED DB TEST SUITE COMPLETA (SINTASSI LEGACY)
' =================================================================================================
'
' Questo test copre esaustivamente le funzionalità del motore di database SPEED.
' I comandi SPEED usano SOLO la sintassi legacy a spazio (`SPEED COMANDO argomenti...`).
' Non esiste forma qualificata per i comandi (`SPEED.USE ...` non è valida). Le funzioni
' SPEED che restituiscono valore usano la sintassi qualificata con parentesi
' (es. `SPEED.SELECTED()`, `SPEED.INFO()`).
'
' 1. IL MOTORE SPEED
'    SPEED è un motore database NoSQL embedded ad alte prestazioni nativo di GM-BASIC.
'    - Storage: Key-Value store persistente (basato su LMDB o simile).
'    - Struttura: Le tabelle sono collection di oggetti tipizzati (UDT).
'    - Indicizzazione: Supporta indici secondari veloci e query complesse.
'    - Relazioni: Supporta join e relazioni master-detail a livello di motore.
'
' 2. SINTASSI LEGACY (PREFISSATA)
'    - Sintassi: `SPEED COMANDO argomenti...`
'    - Stile: Imperativo, parole chiave separate da spazi.
'    - Esempio: `SPEED USE "Clienti" FROM MioDb OF Cliente ALIAS Cl`
'
' 3. QUERY DICHIARATIVE (SQL-LIKE)
'    SPEED supporta un potente linguaggio di query integrato per SELEZIONI e VISTE.
'    - `SPEED SELECT ... FROM ... JOIN ... WHERE ... GROUP BY ...`
'    - Questa sintassi è identica in entrambe le modalità (nella qualificata si passa come stringa).
'    - Permette di eseguire operazioni relazionali complesse (Join, Aggregazioni) direttamente
'      nel motore database, sfruttando gli indici per la massima velocità.
'
' 5. VISTE MATERIALIZZATE
'    - `CREATE VIEW`: Definisce una query persistente che viene aggiornata automaticamente
'      (o manualmente con `REFRESH`) quando i dati sottostanti cambiano.
'    - Le viste si comportano come tabelle di sola lettura.
'
' =================================================================================================

IMPORT SPEED WITH ALIAS
IMPORT IO WITH ALIAS
IMPORT CONVERSIONS WITH ALIAS
IMPORT DATETIME WITH ALIAS
IMPORT ARRAYS WITH ALIAS

SUB Main
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ DbSpeedFullTests - SPEED Test Suite (Sintassi Legacy)             ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    PRINT

    // ==============================================================================
    // Setup Iniziale: Tipi Utente e Variabili
    // ==============================================================================
    TYPE Cliente
        ID AS LONG
        Nome AS STRING * 50
        Citta AS STRING * 30
        Saldo AS CURRENCY
        Attivo AS BIT
    END TYPE

    TYPE Ordine
        ID_Cliente AS LONG
        ID_Ordine AS LONG
        Descrizione AS STRING * 50
        Importo AS CURRENCY
    END TYPE

    ' Tipo per raccogliere i risultati delle query di aggregazione
    TYPE RisultatoJoin
        NomeCliente AS STRING * 50
        TotaleImporto AS CURRENCY
    END TYPE

    DIM cPath AS STRING
    DIM clienteRec AS Cliente
    DIM ordineRec AS Ordine
    DIM ris() AS RisultatoJoin

    ' Preparazione ambiente di test pulito
    cPath = g_work + "/speed_test_data"
    IF IO.DirExists(cPath) THEN RMDIR g_work + "/speed_test_data", TRUE
    MKDIR cPath
    CHDIR cPath

    PRINT "-------------------------------------------------------------------------------"
    PRINT "1. Gestione Database e Tabelle"
    PRINT "-------------------------------------------------------------------------------"

    // SPEED OPEN (Legacy) - Apre/Crea il database fisico (directory o file container)
    SPEED OPEN "MioDatabase" ALIAS MioDb
    PRINT "  SPEED OPEN (Legacy): OK. Alias: MioDb"
    
    // SPEED OPEN (legacy) - Apre un secondo database
SPEED OPEN "AltroDatabase" ALIAS AltroDb
    PRINT "  SPEED OPEN (legacy): OK. Alias: AltroDb"

    // SPEED USE (Legacy) - Apre la tabella 'Clienti' nel database 'MioDb'
    ' OF Cliente: Associa staticamente la tabella al tipo UDT Cliente per la sicurezza dei tipi.
    ' INDEXED BY ID: Definisce 'ID' come chiave primaria (indice clustered).
    SPEED USE "Clienti" FROM MioDb OF Cliente ALIAS Clienti INDEXED BY ID
    PRINT "  SPEED USE (Legacy): OK."

    // SPEED USE (legacy) - Apre la tabella 'Ordini'
    ' Clausole: NomeTabella, FROM DbAlias, OF NomeTipoUDT, ALIAS AliasTabella, [INDEXED BY ...]
SPEED USE "Ordini" FROM MioDb OF Ordine ALIAS Ordini INDEXED BY ID_Ordine
    PRINT "  SPEED USE (legacy): OK."

    // ==============================================================================
    // 2. Manipolazione Dati (CRUD)
    // ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "2. Manipolazione Dati (CRUD)"
    PRINT "-------------------------------------------------------------------------------"

    // SPEED PUT (Legacy) - Inserisce o Aggiorna un record
    clienteRec.ID = 101
    clienteRec.Nome = "Mario Rossi"
    clienteRec.Citta = "Roma"
    clienteRec.Saldo = 150.75
    clienteRec.Attivo = TRUE
    SPEED PUT Clienti, clienteRec
    PRINT "  SPEED PUT (Legacy): OK."

    // SPEED PUT (legacy)
    clienteRec.ID = 102
    clienteRec.Nome = "Giulia Verdi"
    clienteRec.Citta = "Milano"
    clienteRec.Saldo = 2300.50
    clienteRec.Attivo = TRUE
SPEED PUT Clienti, clienteRec
    PRINT "  SPEED PUT (legacy): OK."

    // SPEED GET (Legacy) - Legge il record corrente nella variabile
    SPEED GO Clienti, TOP
    SPEED GET Clienti, clienteRec
    PRINT "  SPEED GET (Legacy): Letto ID " + CONVERSIONS.CSTR(clienteRec.ID)

    // SPEED GET (legacy)
SPEED GO Clienti, BOTTOM
SPEED GET Clienti, clienteRec
    PRINT "  SPEED GET (legacy): Letto ID " + CONVERSIONS.CSTR(clienteRec.ID)

    // SPEED DELETE (Legacy) - Cancella il record corrispondente alla chiave primaria in clienteRec
    clienteRec.ID = 102 // Preparo il record da cancellare
    SPEED DELETE Clienti, clienteRec
    PRINT "  SPEED DELETE (Legacy): OK."

    // SPEED DELETE (legacy)
    clienteRec.ID = 101 // Preparo il record da cancellare
SPEED DELETE Clienti, clienteRec
    PRINT "  SPEED DELETE (legacy): OK."

    // Ripopolo per i test successivi usando la sintassi con literal UDT
SPEED PUT Clienti, {ID: 101, Nome: "Mario Rossi", Citta: "Roma", Saldo: 150.75, Attivo: TRUE}
SPEED PUT Clienti, {ID: 102, Nome: "Giulia Verdi", Citta: "Milano", Saldo: 2300.50, Attivo: TRUE}
SPEED PUT Clienti, {ID: 103, Nome: "Andrea Bianchi", Citta: "Torino", Saldo: -120.00, Attivo: FALSE}

    // ==============================================================================
    // 3. Navigazione e Ricerca
    // ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "3. Navigazione e Ricerca"
    PRINT "-------------------------------------------------------------------------------"

    // SPEED GO (Legacy) - Spostamento assoluto (TOP/BOTTOM)
    SPEED GO Clienti, TOP
    PRINT "  SPEED GO (Legacy): OK."

    // SPEED GO (legacy)
SPEED GO Clienti, BOTTOM
    PRINT "  SPEED GO (legacy): OK."

    // SPEED SKIP (Legacy) - Spostamento relativo
    SPEED SKIP Clienti, -1
    PRINT "  SPEED SKIP (Legacy): OK."

    // SPEED SKIP (legacy)
SPEED SKIP Clienti, -1
    PRINT "  SPEED SKIP (legacy): OK."

    // SPEED SEEK (Legacy) - Ricerca per chiave primaria (o indice attivo)
    SPEED SEEK Clienti, 102
    PRINT "  SPEED SEEK (Legacy): OK."

    // SPEED SEEK (legacy)
SPEED SEEK Clienti, 103
    PRINT "  SPEED SEEK (legacy): OK."

    // ==============================================================================
    // 4. Indicizzazione e Filtri
    // ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "4. Indicizzazione e Filtri"
    PRINT "-------------------------------------------------------------------------------"

    // SPEED CREATE INDEX (Legacy) - Crea un indice secondario
    SPEED CREATE INDEX "CittaIdx" ON Clienti KEY (Citta)
    PRINT "  SPEED CREATE INDEX (Legacy): OK."

    // SPEED CREATE INDEX (legacy)
SPEED CREATE INDEX "NomeIdx" ON Clienti KEY (Nome)
    PRINT "  SPEED CREATE INDEX (legacy): OK."

    // Crea indice secondario su Ordini.ID_Cliente per il JOIN
SPEED CREATE INDEX "ID_Cliente" ON Ordini KEY (ID_Cliente)
    PRINT "  SPEED CREATE INDEX Ordini.ID_Cliente: OK."

    // SPEED SET ORDER (Legacy) - Cambia l'ordine di navigazione
    SPEED SET ORDER TO "CittaIdx" ON Clienti
    PRINT "  SPEED SET ORDER (Legacy): OK."

    // SPEED SET ORDER (legacy)
SPEED SET ORDER TO "NomeIdx" ON Clienti
    PRINT "  SPEED SET ORDER (legacy): OK."

    // SPEED SET FILTER (Legacy) - Filtra i record visibili
    SPEED SET FILTER TO Saldo > 0 ON Clienti
    PRINT "  SPEED SET FILTER (Legacy): OK."

    // SPEED SET FILTER (legacy) - Nota: la condizione è passata come stringa
SPEED SET FILTER TO "Attivo = FALSE" ON Clienti
    PRINT "  SPEED SET FILTER (legacy): OK."
SPEED SET FILTER TO "" ON Clienti // Rimuove filtro

    // SPEED SET RELATION (Legacy) - Imposta relazione master-detail
SPEED SET ORDER TO "" ON Clienti // Ordine naturale per relazione
SPEED SET ORDER TO "ID_Ordine" ON Ordini
    SPEED SET RELATION ON Clienti TO ID INTO Ordini
    PRINT "  SPEED SET RELATION (Legacy): OK."
    
    // SPEED SET RELATION (legacy)
SPEED SET RELATION ON Clienti TO "ID" INTO Ordini
    PRINT "  SPEED SET RELATION (legacy, impostazione): OK."
SPEED SET RELATION ON Clienti // Rimuove relazione
    PRINT "  SPEED SET RELATION (legacy, rimozione): OK."

    // ==============================================================================
    // 5. Query Dichiarativa e Viste
    // ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "5. Query Dichiarativa e Viste"
    PRINT "-------------------------------------------------------------------------------"

    // Popola Ordini per testare il JOIN
    ordineRec.ID_Cliente = 101: ordineRec.ID_Ordine = 1: ordineRec.Descrizione = "Penna": ordineRec.Importo = 10.50
SPEED PUT Ordini, ordineRec
    ordineRec.ID_Cliente = 101: ordineRec.ID_Ordine = 2: ordineRec.Descrizione = "Quaderno": ordineRec.Importo = 5.25
SPEED PUT Ordini, ordineRec
    ordineRec.ID_Cliente = 102: ordineRec.ID_Ordine = 3: ordineRec.Descrizione = "Gomma": ordineRec.Importo = 1.00
SPEED PUT Ordini, ordineRec

    // SPEED SELECT (Legacy) - Esegue una query SQL-like
    ' Seleziona campi, calcola somma, esegue JOIN e GROUP BY, e mette il risultato nell'array 'ris'.
    SPEED SELECT Clienti.Nome AS NomeCliente, SUM(Ordini.Importo) AS TotaleImporto INTO ris() OF RisultatoJoin FROM Clienti JOIN Ordini ON Clienti.ID = Ordini.ID_Cliente GROUP BY Clienti.Nome
    PRINT "  SPEED SELECT (Legacy): OK. Risultati: " + STR(UBOUND(ris) + 1)
    
    // SPEED SELECT (legacy) - Passa l'intera query come stringa
SPEED SELECT Clienti.Nome AS NomeCliente, SUM(Ordini.Importo) AS TotaleImporto INTO ris() OF RisultatoJoin FROM Clienti JOIN Ordini ON Clienti.ID = Ordini.ID_Cliente GROUP BY Clienti.Nome
    PRINT "  SPEED SELECT (legacy): OK. Risultati: " + STR(UBOUND(ris) + 1)

    // SPEED CREATE VIEW (Legacy) - Crea una vista persistente
    SPEED CREATE VIEW ClientiDiRoma OF Cliente AS SELECT * INTO ris() OF Cliente FROM Clienti WHERE Clienti.Citta = "Roma"
    PRINT "  SPEED CREATE VIEW (Legacy): OK."

    // SPEED CREATE VIEW (legacy)
SPEED CREATE VIEW ClientiAttivi OF Cliente AS SELECT * INTO ris() OF Cliente FROM Clienti WHERE Clienti.Attivo = TRUE
    PRINT "  SPEED CREATE VIEW (legacy): OK."

    // SPEED REFRESH VIEW (Legacy) - Aggiorna i dati della vista
    SPEED REFRESH VIEW ClientiDiRoma
    PRINT "  SPEED REFRESH VIEW (Legacy): OK."

    // SPEED REFRESH VIEW (legacy)
SPEED REFRESH VIEW ClientiAttivi
    PRINT "  SPEED.REFRESHVIEW (legacy): OK."

    // SPEED DROP VIEW (Legacy) - Elimina la definizione della vista
    SPEED DROP VIEW ClientiDiRoma
    PRINT "  SPEED DROP VIEW (Legacy): OK."

    // SPEED DROP VIEW (legacy)
SPEED DROP VIEW ClientiAttivi
    PRINT "  SPEED.DROPVIEW (legacy): OK."

    // ==============================================================================
    // 6. Pulizia Finale
    // ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "6. Pulizia Finale"
    PRINT "-------------------------------------------------------------------------------"

    // SPEED DROP INDEX (Legacy)
    SPEED DROP INDEX "CittaIdx" ON Clienti
    PRINT "  SPEED DROP INDEX (Legacy): OK."

    // SPEED DROP INDEX (legacy)
SPEED DROP INDEX "NomeIdx" ON Clienti
    PRINT "  SPEED.DROPINDEX (legacy): OK."

    // SPEED.EOF: vero oltre l'ultimo record
    SPEED GO Clienti, BOTTOM
    SPEED SKIP Clienti, 1
    IF SPEED.EOF(Clienti) THEN PRINT "  SPEED.EOF fine: si" ELSE PRINT "  SPEED.EOF fine: no."

    // SPEED CLOSE [alias | DATABASES]
    SPEED CLOSE Clienti
    PRINT "  SPEED CLOSE Clienti: OK."
    SPEED CLOSE DATABASES
    PRINT "  SPEED CLOSE DATABASES: OK."

    CHDIR ".."
    RMDIR g_work + "/speed_test_data", TRUE
    PRINT "  Directory di test rimossa."

    PRINT
    PRINT "Test SPEED Completato!"
END SUB
