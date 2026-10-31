' FILE: Databases/DbLedgerModernTests.bas
' =================================================================================================
'       GUIDA TEORICA: GESTIONE CONTABILE MODERNA (LEDGER-CLI)
' =================================================================================================
'
' Questo test copre l'uso del sistema contabile integrato in GM-BASIC utilizzando esclusivamente
' la sintassi legacy a spazio (`LEDGER COMANDO ...`).
'
' 1. IL MODELLO A DOPPIA PARTITA (DOUBLE-ENTRY)
'    Il motore LEDGER si basa sul principio della partita doppia: ogni transazione deve bilanciarsi
'    a zero. La somma degli importi positivi e negativi deve essere uguale.
'    Es: Spesa (100) + Contanti (-100) = 0.
'
' 2. STRUTTURA DELLE TRANSAZIONI
'    Una transazione è composta da:
'    - Data
'    - Descrizione
'    - Stato (opzionale: *, !)
'    - Elenco di "Postings" (movimenti sui conti).
'
' 3. SINTASSI E TIPI DI DATO
'    - `LEDGER ADD hndl WITH data, desc {postings} INTO nId`: Aggiunge una transazione. I postings
'      sono passati come un dizionario `{ "Conto": importo }`.
'    - `LEDGER BALANCE hndl WHERE "query" INTO dest` e `LEDGER REGISTER hndl WHERE "query" INTO dest`:
'      Restituiscono dati strutturati (JSON/UDT) invece di stampare a video, permettendo
'      l'elaborazione programmatica dei report.
'
' 4. QUERY LINGUAGGIO
'    Le funzioni di reporting accettano stringhe di query potenti per filtrare i dati:
'    - `account:Spese`: Filtra per nome del conto (regex).
'    - `date:2023-01`: Filtra per periodo temporale.
'    - `desc:benzina`: Filtra per descrizione.
'
' 5. GESTIONE UDT E JSON
'    Questo test dimostra l'integrazione tra il motore LEDGER e il sistema di tipi di GM-BASIC.
'    I risultati delle query vengono restituiti come array di oggetti JSON, che vengono poi
'    mappati su UDT locali (`LedgerEntryDetail`, `LedgerBalanceEntry`) per un accesso tipizzato.
'
' =================================================================================================

IMPORT LEDGER WITH ALIAS
IMPORT CONVERSIONS WITH ALIAS
IMPORT DATETIME WITH ALIAS
IMPORT IO WITH ALIAS
IMPORT JSON WITH ALIAS

SUB Main
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

    ' Titolo del programma
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ DbLedgerModernTests - LEDGER-CLi Test Suite (Sintassi Legacy)              ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' ==============================================================================
    ' Setup Iniziale: Variabili Globali e Tipi Utente
    ' ==============================================================================
    
    ' Definizione di tipi UDT per mappare i risultati JSON del Ledger.
    TYPE LedgerBalanceEntry
        Account AS STRING * 80
        TotalAmount AS CURRENCY
    END TYPE

    TYPE LedgerEntryDetail
        TransactionId AS LONG
        Date AS DATE
        Status AS STRING * 1
        Description AS STRING * 100
        Account AS STRING * 80
        Amount AS CURRENCY
        Comment AS STRING * 100
        AmountIsExplicit AS BIT
        Currency AS STRING * 3
        ExchangeRate AS CURRENCY
        BaseAmount AS CURRENCY
    END TYPE

    DIM cPath AS STRING
    DIM hLedger AS LONG // Handle per il database Ledger
    DIM nRet AS LONG
    DIM dDate AS DATE
    DIM isValid AS BIT

    ' Dichiarazione delle variabili per i risultati
    DIM tuttiConti$() AS STRING
    DIM speseConti$() AS STRING
    DIM registerResults(0) AS LedgerEntryDetail
    DIM balanceResultsUdt(0) AS LedgerBalanceEntry
    DIM transactionDetailsJson() AS JSON

    ' Percorso temporaneo per i file Ledger
    cPath = g_work + "/ledger_test_data_modern"
    MKDIR cPath
    CHDIR cPath // Usa IO.CHDIR per impostare la directory di lavoro

    PRINT "-------------------------------------------------------------------------------"
    PRINT "1. Creazione e Apertura del Database Ledger 'contabilita.dat'"
    PRINT "-------------------------------------------------------------------------------"
    ' Apre il database (o lo crea) e restituisce un handle.
    hLedger = LEDGER.OPEN("contabilita.dat")
    PRINT "Database Ledger 'contabilita.dat' aperto. Handle: " + CONVERSIONS.CSTR(hLedger)

    ' ==============================================================================
    ' Aggiunta di Transazioni (ADD)
    ' ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "2. Aggiunta di Transazioni (ADD)"
    PRINT "-------------------------------------------------------------------------------"
    dDate = DATETIME.DATEVALUE("2023-01-01")

    ' Transazione 1: Salario (legacy, comando con INTO)
    ' Utilizza un dizionario letterale per definire i movimenti. La somma deve essere 0.
LEDGER ADD hLedger WITH dDate, "Salario Gennaio" {
        "Entrate:Salario": -1000.00,
        "Spese:Alimenti:Cena": 100.00,
        "Attivita:Banca": 900.00
    } INTO nRet
    PRINT "Aggiunta transazione 'Salario Gennaio'. ID: " + CONVERSIONS.CSTR(nRet)
    DIM txId1 AS LONG = nRet

    ' Transazione 2: Acquisto libri
    dDate = DATETIME.DATEADD("D", 5, dDate)
LEDGER ADD hLedger WITH dDate, "Acquisto Libri" {
        "Spese:Libri": 50.00,
        "Attivita:Contanti": -50.00
    } INTO nRet
    PRINT "Aggiunta transazione 'Acquisto Libri'. ID: " + CONVERSIONS.CSTR(nRet)
    DIM txId2 AS LONG = nRet

    ' Transazione 3: Pagamento Cliente A (legacy, comando con INTO)
    dDate = DATETIME.DATEADD("D", 10, dDate)
LEDGER ADD hLedger WITH dDate, "Pagamento Cliente A" {
        "Attivita:Banca": 200.00,
        "Entrate:Servizi:Consulenza": -200.00
    } INTO nRet
    PRINT "Aggiunta transazione 'Pagamento Cliente A'. ID: " + CONVERSIONS.CSTR(nRet)

    ' Transazione 4: Spesa per utenze con Tag
    dDate = DATETIME.DATEADD("D", 15, dDate)
LEDGER ADD hLedger WITH dDate, "Utenze Febbraio;Tag:bollette" {
        "Spese:Utenze:Elettricita": 80.00,
        "Attivita:Banca": -80.00
    } INTO nRet
    PRINT "Aggiunta transazione 'Utenze Febbraio'. ID: " + CONVERSIONS.CSTR(nRet)

    ' Transazione 5: Acquisto estero multi-valuta (USD → base @ 0.95)
    dDate = DATETIME.DATEADD("D", 1, dDate)
LEDGER ADD hLedger WITH dDate, "Acquisto PayPal USD" {
        "Attivita:PayPal": 100.00 USD @ 0.95,
        "Equity:FX": -95.00
    } INTO nRet
    PRINT "Aggiunta transazione FX 'Acquisto PayPal USD'. ID: " + CONVERSIONS.CSTR(nRet)

    ' ==============================================================================
    ' Recupero Dettagli Transazione (GET)
    ' ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "3. Recupero Dettagli Transazione (GET)"
    PRINT "-------------------------------------------------------------------------------"
    
    ' Recupera i dettagli di una transazione specifica come array di oggetti JSON.
LEDGER GET hLedger ID txId1 INTO transactionDetailsJson
    IF UBOUND(transactionDetailsJson) >= 0 THEN
        PRINT "Dettagli Transazione ID " + CONVERSIONS.CSTR(txId1) + ":"
        FOR i = 0 TO UBOUND(transactionDetailsJson)
            ' Accesso dinamico alle proprietà dell'oggetto JSON.
            PRINT "  Descrizione: " + transactionDetailsJson(i).Description + ", Conto: " + transactionDetailsJson(i).Account + ", Importo: " + CONVERSIONS.CSTR(transactionDetailsJson(i).Amount)
        NEXT i
    ELSE
        PRINT "Transazione ID " + CONVERSIONS.CSTR(txId1) + " non trovata."
    END IF

    ' ==============================================================================
    ' Registrazione e Bilancio (REGISTER, BALANCE)
    ' ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "4. Registrazione e Bilancio (REGISTER, BALANCE)"
    PRINT "-------------------------------------------------------------------------------"

    PRINT "Registro delle transazioni per 'Spese:Libri':"
    DIM registerResultsJsonTemp() AS JSON
    ' Esegue una query REGISTER filtrata per conto.
LEDGER REGISTER hLedger WHERE "account:Spese:Libri" INTO registerResultsJsonTemp
    
    ' Mappa i risultati JSON sull'UDT LedgerEntryDetail.
    REDIM registerResults(UBOUND(registerResultsJsonTemp))
    FOR i = 0 TO UBOUND(registerResultsJsonTemp)
        registerResults(i).TransactionId = registerResultsJsonTemp(i).TransactionId
        registerResults(i).Date = registerResultsJsonTemp(i).Date
        registerResults(i).Status = registerResultsJsonTemp(i).Status
        registerResults(i).Description = registerResultsJsonTemp(i).Description
        registerResults(i).Account = registerResultsJsonTemp(i).Account
        registerResults(i).Amount = registerResultsJsonTemp(i).Amount
        registerResults(i).Comment = registerResultsJsonTemp(i).Comment
        registerResults(i).AmountIsExplicit = registerResultsJsonTemp(i).AmountIsExplicit
    NEXT i
    
    FOR i = 0 TO UBOUND(registerResults)
        PRINT "  Data: " + DATETIME.DTOS(registerResults(i).Date) + ", Desc: " + registerResults(i).Description + ", Conto: " + registerResults(i).Account + ", Imp: " + CONVERSIONS.CSTR(registerResults(i).Amount)
    NEXT i

    PRINT
    PRINT "Bilancio per 'Attivita:Banca':"
    DIM balanceJsonTemp() AS JSON
    ' Calcola il saldo corrente per un conto specifico.
LEDGER BALANCE hLedger WHERE "account:Attivita:Banca" INTO balanceJsonTemp
    REDIM balanceResultsUdt(UBOUND(balanceJsonTemp))
    FOR i = 0 TO UBOUND(balanceJsonTemp)
        balanceResultsUdt(i).Account = balanceJsonTemp(i).Account
        balanceResultsUdt(i).TotalAmount = balanceJsonTemp(i).TotalAmount
    NEXT i

    FOR i = 0 TO UBOUND(balanceResultsUdt)
        PRINT "  Conto: " + balanceResultsUdt(i).Account + ", Saldo: " + CONVERSIONS.CSTR(balanceResultsUdt(i).TotalAmount)
    NEXT i

    PRINT
    PRINT "Bilancio complessivo di tutte le spese (WHERE):"
    ' Esegue una query BALANCE con filtro wildcard (*) per aggregare i sottoconti.
LEDGER BALANCE hLedger WHERE "account:Spese:*" INTO balanceJsonTemp
    REDIM balanceResultsUdt(UBOUND(balanceJsonTemp))
    FOR i = 0 TO UBOUND(balanceJsonTemp)
        balanceResultsUdt(i).Account = balanceJsonTemp(i).Account
        balanceResultsUdt(i).TotalAmount = balanceJsonTemp(i).TotalAmount
    NEXT i
    
    FOR i = 0 TO UBOUND(balanceResultsUdt)
        PRINT "  Conto: " + balanceResultsUdt(i).Account + ", Saldo: " + CONVERSIONS.CSTR(balanceResultsUdt(i).TotalAmount)
    NEXT i

    PRINT
    PRINT "Bilancio completo (tutti gli account):"
LEDGER BALANCE hLedger INTO balanceJsonTemp
    REDIM balanceResultsUdt(UBOUND(balanceJsonTemp))
    FOR i = 0 TO UBOUND(balanceJsonTemp)
        balanceResultsUdt(i).Account = balanceJsonTemp(i).Account
        balanceResultsUdt(i).TotalAmount = balanceJsonTemp(i).TotalAmount
    NEXT i

    FOR i = 0 TO UBOUND(balanceResultsUdt)
        PRINT "  Conto: " + balanceResultsUdt(i).Account + ", Saldo: " + CONVERSIONS.CSTR(balanceResultsUdt(i).TotalAmount)
    NEXT i

    ' ==============================================================================
    ' Modifica Transazioni (EDIT)
    ' ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "5. Modifica Transazioni (EDIT)"
    PRINT "-------------------------------------------------------------------------------"

    PRINT "Prima modifica: saldo 'Spese:Libri'."
LEDGER BALANCE hLedger WHERE "account:Spese:Libri" INTO balanceJsonTemp
    REDIM balanceResultsUdt(UBOUND(balanceJsonTemp))
    FOR i = 0 TO UBOUND(balanceJsonTemp)
        balanceResultsUdt(i).Account = balanceJsonTemp(i).Account
        balanceResultsUdt(i).TotalAmount = balanceJsonTemp(i).TotalAmount
    NEXT i
    IF UBOUND(balanceResultsUdt) >= 0 THEN PRINT "  Saldo precedente: " + CONVERSIONS.CSTR(balanceResultsUdt(0).TotalAmount)

    ' Modifica una transazione esistente (identificata da txId2).
    ' Sostituisce completamente i dati della transazione.
LEDGER EDIT hLedger ID txId2 WITH DATETIME.DATEADD("D", 6, dDate), "Acquisto Libri Aggiornato" {
        "Spese:Libri": 60.00,
        "Attivita:Contanti": -60.00
    } INTO nRet
    PRINT "Transazione ID " + CONVERSIONS.CSTR(txId2) + " modificata. Nuova ID: " + CONVERSIONS.CSTR(nRet)

    PRINT "Dopo modifica: saldo 'Spese:Libri'."
LEDGER BALANCE hLedger WHERE "account:Spese:Libri" INTO balanceJsonTemp
    REDIM balanceResultsUdt(UBOUND(balanceJsonTemp))
    FOR i = 0 TO UBOUND(balanceJsonTemp)
        balanceResultsUdt(i).Account = balanceJsonTemp(i).Account
        balanceResultsUdt(i).TotalAmount = balanceJsonTemp(i).TotalAmount
    NEXT i
    IF UBOUND(balanceResultsUdt) >= 0 THEN PRINT "  Saldo attuale: " + CONVERSIONS.CSTR(balanceResultsUdt(0).TotalAmount)

    ' ==============================================================================
    ' Gestione Account (ACCOUNTS)
    ' ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "6. Gestione Account (ACCOUNTS)"
    PRINT "-------------------------------------------------------------------------------"

    ' Recupera la lista di tutti i conti definiti nel ledger.
LEDGER ACCOUNTS hLedger INTO tuttiConti$
    PRINT "Lista di tutti gli account principali:"
    FOR i = 0 TO UBOUND(tuttiConti$)
        PRINT "  - " + tuttiConti$(i)
    NEXT i

    PRINT
    ' Recupera solo i sottoconti di "Spese".
LEDGER ACCOUNTS hLedger "Spese" INTO speseConti$()
    PRINT "Lista di tutti gli account sotto 'Spese':"
    FOR i = 0 TO UBOUND(speseConti$)
        PRINT "  - " + speseConti$(i)
    NEXT i

    ' ==============================================================================
    ' Salvataggio e Chiusura (SAVE, CLOSE)
    ' ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "7. Salvataggio e Chiusura (SAVE, CLOSE)"
    PRINT "-------------------------------------------------------------------------------"

LEDGER SAVE hLedger
    PRINT "Modifiche salvate nel database 'contabilita.dat'."

LEDGER CLOSE hLedger
    PRINT "Database Ledger chiuso."

    PRINT "Riapro il database per verificare la persistenza..."
    hLedger = LEDGER.OPEN("contabilita.dat")
    PRINT "Database riaperto. Handle: " + CONVERSIONS.CSTR(hLedger)

    PRINT "Verifico il saldo di 'Spese:Libri' dopo riapertura:"
LEDGER BALANCE hLedger WHERE "account:Spese:Libri" INTO balanceJsonTemp
    REDIM balanceResultsUdt(UBOUND(balanceJsonTemp))
    FOR i = 0 TO UBOUND(balanceJsonTemp)
        balanceResultsUdt(i).Account = balanceJsonTemp(i).Account
        balanceResultsUdt(i).TotalAmount = balanceJsonTemp(i).TotalAmount
    NEXT i
    IF UBOUND(balanceResultsUdt) >= 0 THEN PRINT "  Saldo verificato: " + CONVERSIONS.CSTR(balanceResultsUdt(0).TotalAmount)

LEDGER CLOSE hLedger

    ' ==============================================================================
    ' Integrità e Manutenzione (VALIDATE, PACK, DELETE)
    ' ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "8. Integrità e Manutenzione (VALIDATE, PACK, DELETE)"
    PRINT "-------------------------------------------------------------------------------"

    hLedger = LEDGER.OPEN("contabilita.dat")
    ' Verifica che tutte le transazioni siano bilanciate (somma = 0).
LEDGER VALIDATE hLedger INTO isValid
    IF isValid THEN
        PRINT "Il giornale è valido."
    ELSE
        PRINT "ERRORE: Il giornale non è bilanciato o corrotto!"
    END IF
LEDGER CLOSE hLedger

    hLedger = LEDGER.OPEN("contabilita.dat")
    dDate = DATETIME.TODAY()
LEDGER ADD hLedger WITH dDate, "Transazione da Eliminare" {
        "Spese:Varie": 50.00,
        "Attivita:Contanti": -50.00
    } INTO nRet
    PRINT "Aggiunta transazione per test DELETE/PACK con ID: " + CONVERSIONS.CSTR(nRet)
LEDGER SAVE hLedger

    ' Marca la transazione come cancellata (soft delete).
LEDGER DELETE hLedger ID nRet
    PRINT "Transazione ID " + CONVERSIONS.CSTR(nRet) + " marcata come cancellata."
LEDGER SAVE hLedger

    ' Rimuove fisicamente le transazioni cancellate e compatta il file.
LEDGER PACK hLedger
    PRINT "Eseguito PACK sul database Ledger. Lo spazio del record cancellato è stato recuperato."
LEDGER CLOSE hLedger

    ' ==============================================================================
    ' Pulizia Finale
    ' ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "9. Pulizia Finale"
    PRINT "-------------------------------------------------------------------------------"

    CHDIR ".."

    KILL cPath + "/contabilita.dat"
    KILL cPath + "/contabilita.idx"
    RMDIR cPath
    PRINT "File di test del Ledger e directory eliminati."

    PRINT
    PRINT "Test LEDGER-CLi Completato!"

END SUB
