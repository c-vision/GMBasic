' FILE: Databases/DbIsamFullTests.bas
' =================================================================================================
'       GUIDA TEORICA: DATABASE ISAM (Indexed Sequential Access Method)
' =================================================================================================
'
' Questo test copre le funzionalità del motore di database ISAM nativo di GM-BASIC.
' ISAM è un sistema di gestione dati basato su file, ottimizzato per l'accesso rapido tramite
' chiavi (indici) e la navigazione sequenziale ordinata.
'
' 1. CARATTERISTICHE DI ISAM IN GM-BASIC
'    - File Singolo: I dati e gli indici sono contenuti in un unico file `.isdb` (o gestiti insieme).
'    - Tipizzazione Forte: Ogni file ISAM è associato a un `TYPE` definito dall'utente.
'    - Indici Multipli: Supporta una chiave primaria (univoca) e indici secondari (anche non univoci).
'    - Transazionalità: Supporta transazioni ACID complete (`BEGIN`, `COMMIT`, `ROLLBACK`) con vari
'      livelli di isolamento (`READ COMMITTED`, `REPEATABLE READ`, `SERIALIZABLE`).
'    - Concorrenza: Gestisce il locking a livello di record o di file per l'accesso multi-utente.
'
' 2. SINTASSI
'    I comandi ISAM usano SOLO la sintassi legacy a spazio: `ISAM COMANDO ...`
'    (es. `ISAM OPEN "file" OF Tipo ...`). Non esiste forma qualificata per i comandi
'    (`ISAM.OPEN ...` non è valida). Le funzioni ISAM che restituiscono un valore
'    usano la sintassi qualificata con parentesi (es. `ISAM.EOF()`, `ISAM.RECNO()`).
'    I comandi che operano su una work area richiedono il numero file/alias come primo
'    argomento (il valore -1 indica la work area attiva).
'
' 3. OPERAZIONI CHIAVE
'    - `OPEN/USE`: Apre un database, definisce il tipo di record e gli indici. Richiede un `ALIAS`.
'    - `SEEK`: Cerca un record usando una chiave di indice.
'    - `GET/PUT`: Legge/Scrive l'intero record (tipizzato) dalla/alla posizione corrente.
'    - `APPEND`: Aggiunge un nuovo record.
'    - `DELETE/RECALL`: Gestione logica della cancellazione (soft delete).
'    - `PACK`: Rimozione fisica dei record cancellati.
'
' 4. RELAZIONI
'    ISAM supporta relazioni master-detail automatiche tramite `SET RELATION`. Quando il cursore
'    del padre si sposta, il figlio viene automaticamente posizionato sui record corrispondenti.
'
' 5. CONVERSIONE
'    Il comando `CONVERT` permette di migrare dati da DBF (legacy) a ISAM (moderno), preservando
'    la struttura e creando gli indici.
'
' =================================================================================================

IMPORT ISAM WITH ALIAS
IMPORT IO WITH ALIAS
IMPORT CONVERSIONS WITH ALIAS
IMPORT DATETIME WITH ALIAS
IMPORT DBF WITH ALIAS
IMPORT STRINGS WITH ALIAS

SUB Main
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ DbIsamFullTests - ISAM Test Suite (Sintassi Legacy)               ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    PRINT

    //==============================================================================
    // Setup Iniziale: Tipi Utente e Variabili
    // ==============================================================================
    
    ' Definizione della struttura del record 'Cliente'
    TYPE Cliente
        ID AS LONG
        Nome AS STRING * 20
        Citta AS STRING * 15
        Saldo AS CURRENCY
        Attivo AS BIT
        UltimoOrdine AS DATE
    END TYPE

    ' Definizione della struttura del record 'Ordine'
    TYPE Ordine
        ID_Ordine AS LONG
        ID_Cliente AS LONG
        Descrizione AS STRING * 30
        Importo AS CURRENCY
        DataOrdine AS DATE
    END TYPE

    DIM cPath AS STRING
    DIM hClienti AS LONG
    DIM hOrdini AS LONG
    DIM clienteRec AS Cliente
    DIM ordineRec AS Ordine
    DIM sFilter AS STRING
    DIM bResult AS BIT
    DIM lResult AS LONG
    DIM sResult AS STRING

    cPath = g_work
    IF IO.DirExists(cPath) THEN RMDIR cPath, TRUE
    MKDIR cPath

    PRINT "-------------------------------------------------------------------------------"
    PRINT "1. Creazione e Apertura Database ISAM"
    PRINT "-------------------------------------------------------------------------------"

    // Sintassi Legacy corretta con ALIAS
    ' Apre/Crea "work/clienti.isdb", lo associa al tipo Cliente e definisce gli indici.
    ISAM OPEN g_work + "/clienti.isdb" OF Cliente ALIAS Clienti INDEXED BY ID, Nome, Citta
    PRINT "Database 'clienti.isdb' aperto con alias 'Clienti'."
    
    // Apre con ALIAS
    ' Apre/Crea "work/ordini.isdb", lo associa al tipo Ordine e definisce gli indici.
ISAM OPEN g_work + "/ordini.isdb" OF Ordine ALIAS Ordini INDEXED BY ID_Ordine, DataOrdine
    PRINT "Database 'ordini.isdb' aperto con alias 'Ordini'."

    // SELECT work area
ISAM SELECT Clienti // alias come identificatore
    PRINT "Work Area corrente (Clienti): " + CONVERSIONS.CSTR(ISAM.SELECTED(Clienti))
    ISAM SELECT Ordini // Sintassi Legacy con alias come identificatore
    PRINT "Work Area corrente (Ordini): " + CONVERSIONS.CSTR(ISAM.SELECTED(Ordini))
ISAM SELECT Clienti // Torna a Clienti

    ' Versione 1: Chiamata senza parametri (opera sull'area attiva 'Clienti')
    PRINT "RECNO() su area attiva: " + CONVERSIONS.CSTR(ISAM.RECNO())
    PRINT "LASTREC() su area attiva: " + CONVERSIONS.CSTR(ISAM.LASTREC())
    
    ' Versione 2: Chiamata con parametro esplicito (specificando l'area 'Ordini')
    PRINT "RECSIZE(Ordini): " + CONVERSIONS.CSTR(ISAM.RECSIZE(Ordini))
    
    // ==============================================================================
    // Popolamento Dati: APPEND, PUT, GET
    // ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "2. Aggiunta e Modifica Dati (APPEND, PUT, GET)"
    PRINT "-------------------------------------------------------------------------------"

    // Record 1 per Clienti (Sintassi Legacy APPEND specificando l'alias)
    clienteRec.ID = 1
    clienteRec.Nome = "Mario Rossi"
    clienteRec.Citta = "Roma"
    clienteRec.Saldo = 150.75
    clienteRec.Attivo = TRUE
    clienteRec.UltimoOrdine = DATETIME.DATEVALUE("2023-01-15")
    ISAM APPEND Clienti, clienteRec
    PRINT "Appeso cliente: " + clienteRec.Nome + " (ID: " + CONVERSIONS.CSTR(clienteRec.ID) + ")"

    // Record 2 per Clienti (APPEND specificando l'alias)
    clienteRec.ID = 2
    clienteRec.Nome = "Giulia Verdi"
    clienteRec.Citta = "Milano"
    clienteRec.Saldo = 2300.50
    clienteRec.Attivo = TRUE
    clienteRec.UltimoOrdine = DATETIME.DATEVALUE("2023-02-20")
ISAM APPEND clienteRec
    PRINT "Appeso cliente: " + clienteRec.Nome + " (ID: " + CONVERSIONS.CSTR(clienteRec.ID) + ")"

    // Record 3 per Clienti (Sintassi Legacy APPEND sull'area attiva)
    ISAM SELECT Clienti
    clienteRec.ID = 3
    clienteRec.Nome = "Andrea Bianchi"
    clienteRec.Citta = "Torino"
    clienteRec.Saldo = -120.00
    clienteRec.Attivo = FALSE
    clienteRec.UltimoOrdine = DATETIME.DATEVALUE("2023-03-01")
    ISAM APPEND clienteRec
    PRINT "Appeso cliente: " + clienteRec.Nome + " (ID: " + CONVERSIONS.CSTR(clienteRec.ID) + ")"

    // Record 4 per Clienti (per DELETE/RECALL)
    clienteRec.ID = 4
    clienteRec.Nome = "Laura Neri"
    clienteRec.Citta = "Roma"
    clienteRec.Saldo = 500.00
    clienteRec.Attivo = TRUE
    clienteRec.UltimoOrdine = DATETIME.DATEVALUE("2023-04-10")
    ISAM APPEND Clienti, clienteRec
    PRINT "Appeso cliente: " + clienteRec.Nome + " (ID: " + CONVERSIONS.CSTR(clienteRec.ID) + ")"

    PRINT "LASTREC(Clienti) dopo aggiunte: " + CONVERSIONS.CSTR(ISAM.LASTREC(Clienti))

    // Modifica un record con PUT (Legacy)
    ISAM SEEK Clienti, 1 // Posiziona su ID 1
    ISAM GET Clienti, clienteRec
    IF ISAM.FOUND(Clienti) THEN
        clienteRec.Citta = "Napoli"
        clienteRec.Saldo = 200.00
        ISAM PUT Clienti, clienteRec
        PRINT "Modificato cliente ID 1 a Napoli (Legacy PUT)."
    END IF

    // Modifica un record con PUT (Qualificata)
ISAM SEEK -1, 2 // Posiziona su ID 2
ISAM GET -1, clienteRec
    IF ISAM.FOUND(Clienti) THEN
        clienteRec.Citta = "Genova"
        clienteRec.Saldo = 3000.00
ISAM PUT -1, clienteRec
        PRINT "Modificato cliente ID 2 a Genova (Qualified PUT)."
    END IF

    // ==============================================================================
    // Navigazione e Ricerca: GO, SKIP, SEEK, GET
    // ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "3. Navigazione e Ricerca"
    PRINT "-------------------------------------------------------------------------------"

    // GO TOP (Legacy)
    ISAM GO Clienti, TOP
    PRINT "Posizionato a TOP. Record corrente: "
    ISAM GET Clienti, clienteRec
    PRINT "  ID: " + CONVERSIONS.CSTR(clienteRec.ID) + ", Nome: " + clienteRec.Nome + " (RECNO: " + CONVERSIONS.CSTR(ISAM.RECNO(Clienti)) + ")"

    // SKIP (Legacy)
    ISAM SKIP Clienti, 2
    PRINT "Saltato di 2 posizioni. Record corrente: "
    ISAM GET Clienti, clienteRec
    PRINT "  ID: " + CONVERSIONS.CSTR(clienteRec.ID) + ", Nome: " + clienteRec.Nome + " (RECNO: " + CONVERSIONS.CSTR(ISAM.RECNO(Clienti)) + ")"

    // GO BOTTOM (Qualificata)
ISAM GO -1, BOTTOM
    PRINT "Posizionato a BOTTOM. Record corrente: "
ISAM GET -1, clienteRec
    PRINT "  ID: " + CONVERSIONS.CSTR(clienteRec.ID) + ", Nome: " + clienteRec.Nome + " (RECNO: " + CONVERSIONS.CSTR(ISAM.RECNO(Clienti)) + ")"
    
    // SKIP (Qualificata)
ISAM SKIP -1, -1
    PRINT "Saltato indietro di 1. Record corrente: "
ISAM GET -1, clienteRec
    PRINT "  ID: " + CONVERSIONS.CSTR(clienteRec.ID) + ", Nome: " + clienteRec.Nome + " (RECNO: " + CONVERSIONS.CSTR(ISAM.RECNO(Clienti)) + ")"

    // Test BOF/EOF
ISAM GO -1, TOP
ISAM SKIP -1, -1
    PRINT "Dopo SKIP -1 da TOP: BOF(Clienti) = " + CONVERSIONS.CSTR(ISAM.BOF(Clienti)) + ", EOF(Clienti) = " + CONVERSIONS.CSTR(ISAM.EOF(Clienti))
ISAM GO -1, BOTTOM
ISAM SKIP -1, 1
    PRINT "Dopo SKIP 1 da BOTTOM: BOF(Clienti) = " + CONVERSIONS.CSTR(ISAM.BOF(Clienti)) + ", EOF(Clienti) = " + CONVERSIONS.CSTR(ISAM.EOF(Clienti))
ISAM GO -1, TOP // Riporta il cursore in una posizione valida

    // SEEK (Legacy con indice primario)
    ISAM SEEK Clienti, 3
    IF ISAM.FOUND(Clienti) THEN
        ISAM GET Clienti, clienteRec
        PRINT "  Trovato ID 3: " + clienteRec.Nome
    ELSE
        PRINT "  ID 3 non trovato."
    END IF

    // SEEK (Qualificata con indice secondario)
ISAM SET ORDER TO "Nome" ON -1 // Ordina per nome per testare SEEK su indice secondario
    PRINT "Ordine cambiato su indice 'Nome'."
ISAM SEEK -1, "Laura Neri" USING "Nome"
    IF ISAM.FOUND(Clienti) THEN
ISAM GET -1, clienteRec
        PRINT "  Trovato 'Laura Neri' tramite indice Nome: " + CONVERSIONS.CSTR(clienteRec.ID)
    ELSE
        PRINT "  'Laura Neri' non trovato tramite indice Nome."
    END IF
ISAM SET ORDER TO "ID" ON -1 // Torna all'indice primario

    // ==============================================================================
    // Indicizzazione e Filtri: CREATE INDEX, SET ORDER, SET FILTER
    // ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "4. Indicizzazione e Filtri"
    PRINT "-------------------------------------------------------------------------------"

    // CREATE INDEX (Legacy)
    ISAM CREATE INDEX "SaldoIdx" ON Clienti KEY (Saldo) LEN 8
    PRINT "Creato indice 'SaldoIdx' sul campo Saldo (Legacy)."

    // CREATE INDEX (Qualificata)
ISAM CREATE INDEX "AttivoIdx" ON Clienti KEY ("CONVERSIONS.CSTR(Attivo)") LEN 1
    PRINT "Creato indice 'AttivoIdx' sul campo Attivo (Qualified, con espressione)."

    // SET ORDER (Legacy) — usa l'indice appena creato
    ISAM SET ORDER TO "SaldoIdx" ON Clienti
    PRINT "Ordine impostato su 'SaldoIdx'."
    ISAM GO Clienti, TOP
    PRINT "Clienti per Saldo:"
    DO WHILE NOT ISAM.EOF(Clienti)
       ISAM GET Clienti, clienteRec
       PRINT "  Saldo: " + CONVERSIONS.CSTR(clienteRec.Saldo) + ", Nome: " + clienteRec.Nome
       ISAM SKIP Clienti, 1
    LOOP

    // SET ORDER (Qualificata) — usa indice primario Nome
ISAM SET ORDER TO "Nome" ON -1
    PRINT "Ordine impostato su 'Nome'."
ISAM GO -1, TOP
    PRINT "Clienti per Nome:"
    DO WHILE NOT ISAM.EOF(Clienti)
ISAM GET -1, clienteRec
       PRINT "  Nome: " + CONVERSIONS.CSTR(clienteRec.Nome) + ", Citta: " + CONVERSIONS.CSTR(clienteRec.Citta)
ISAM SKIP -1
    LOOP
ISAM SET ORDER TO "ID" ON -1 // Torna all'indice primario ID

    // ==============================================================================
    // DROP INDEX
    // ==============================================================================

    // DROP INDEX (Legacy) — dopo averli usati
    ISAM DROP INDEX "SaldoIdx" ON Clienti
    PRINT "  ISAM DROP INDEX (Legacy): Indice 'SaldoIdx' rimosso."

    // DROP INDEX (Qualificata)
ISAM DROP INDEX "AttivoIdx" ON Clienti
PRINT "  ISAM DROP INDEX (Qualificato): Indice 'AttivoIdx' rimosso."
    
    // SET FILTER (Legacy)
    ISAM SET FILTER TO Citta = "Roma" AND Saldo > 100 ON Clienti
    PRINT "Filtro impostato: Clienti di Roma con Saldo > 100 (Legacy)."
    ISAM GO Clienti, TOP
    PRINT "Clienti filtrati (Legacy):"
    DO WHILE NOT ISAM.EOF(Clienti)
       ISAM GET Clienti, clienteRec
       PRINT "  ID: " + CONVERSIONS.CSTR(clienteRec.ID) + ", Nome: " + clienteRec.Nome + ", Citta: " + clienteRec.Citta
       ISAM SKIP Clienti, 1
    LOOP

    // SET FILTER (Qualificata)
ISAM SET FILTER TO FN(rec) => rec.Attivo = TRUE ON -1
    PRINT "Filtro impostato: Solo clienti attivi (Qualified)."
ISAM GO -1, TOP
    PRINT "Clienti filtrati (Qualified):"
    DO WHILE NOT ISAM.EOF(Clienti)
ISAM GET -1, clienteRec
       PRINT "  ID: " + CONVERSIONS.CSTR(clienteRec.ID) + ", Nome: " + clienteRec.Nome + ", Attivo: " + CONVERSIONS.CSTR(clienteRec.Attivo)
ISAM SKIP -1
    LOOP
ISAM SET FILTER TO "" ON -1 // Rimuove il filtro

    // ==============================================================================
    // Locking Manuale: LOCK, UNLOCK, RLOCK(), FLOCK()
    // ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "5. Locking Manuale"
    PRINT "-------------------------------------------------------------------------------"

    // RLOCK() (Legacy)
    ISAM SEEK Clienti, 1
    bResult = ISAM.RLOCK(Clienti)
    IF CONVERSIONS.CSTR(bResult) THEN
       PRINT "Lock record ID 1 acquisito con RLOCK() (Legacy)."
       ISAM UNLOCK Clienti, RECORD
       PRINT "Lock record ID 1 rilasciato (Legacy)."
    ELSE
       PRINT "Lock record ID 1 FALLITO con RLOCK() (Legacy)."
    END IF

    // FLOCK() (Qualificata)
    bResult = ISAM.FLOCK(Clienti)
    IF CONVERSIONS.CSTR(bResult) THEN
       PRINT "Lock file acquisito con ISAM.FLOCK(Clienti) (Qualified)."
ISAM UNLOCK -1 // Rilascia lock file (Qualified)
       PRINT "Lock file rilasciato (Qualified)."
    ELSE
       PRINT "Lock file FALLITO con ISAM.FLOCK(Clienti) (Qualified)."
    END IF

    // LOCK 1, RECORD (Legacy - bloccante)
    PRINT "Tentativo di LOCK Clienti, RECORD (Legacy - bloccante).."
    ISAM LOCK Clienti, RECORD
    PRINT "Lock record ID 1 acquisito con LOCK Clienti, RECORD (Legacy)."
    ISAM UNLOCK Clienti, RECORD
    PRINT "Lock record ID 1 rilasciato (Legacy)."

    // ISAM.LOCK RECORD (legacy - bloccante)
ISAM SEEK -1, 2 // Posiziona su ID 2
PRINT "Tentativo di ISAM LOCK -1, RECORD (Qualified - bloccante).."
ISAM LOCK -1, RECORD
PRINT "Lock record ID 2 acquisito con ISAM LOCK -1, RECORD (Qualified)."
ISAM UNLOCK -1, RECORD
    PRINT "Lock record ID 2 rilasciato (Qualified)."

    // ==============================================================================
    // Transazioni: BEGIN TRANSACTION, COMMIT, ROLLBACK
    // ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "6. Test Transazioni"
    PRINT "-------------------------------------------------------------------------------"

    ISAM BEGIN TRANSACTION LEVEL REPEATABLE READ
       PRINT "Iniziata transazione (REPEATABLE READ). Saldo ID=3 prima mod:"
ISAM SEEK -1, 3
ISAM GET -1, clienteRec
       PRINT "  Saldo: " + CONVERSIONS.CSTR(clienteRec.Saldo)
       clienteRec.Saldo = clienteRec.Saldo + 100
ISAM PUT -1, clienteRec
       PRINT "  Saldo ID=3 dopo PUT (non commesso): " + CONVERSIONS.CSTR(clienteRec.Saldo)
    ISAM ROLLBACK
    PRINT "Rollback eseguito. Verifico saldo ID=3:"
ISAM SEEK -1, 3
ISAM GET -1, clienteRec
    PRINT "  Saldo ID=3 dopo ROLLBACK: " + CONVERSIONS.CSTR(clienteRec.Saldo) // Dovrebbe essere il valore originale
    IF clienteRec.Saldo = -120 THEN
        PRINT "  [ASSERT] ROLLBACK OK"
    ELSE
        PRINT "  [ASSERT] ROLLBACK FAIL: atteso -120, ottenuto " + CONVERSIONS.CSTR(clienteRec.Saldo)
    END IF

    ISAM BEGIN TRANSACTION
       PRINT "Iniziata nuova transazione. Saldo ID=4 prima mod:"
ISAM SEEK -1, 4
ISAM GET -1, clienteRec
       PRINT "  Saldo: " + CONVERSIONS.CSTR(clienteRec.Saldo)
       clienteRec.Saldo = clienteRec.Saldo * 2
       clienteRec.Nome = "Laura Neri (Aggiornata)"
ISAM PUT -1, clienteRec
       PRINT "  Saldo ID=4 dopo PUT (non commesso): " + CONVERSIONS.CSTR(clienteRec.Saldo)
    ISAM COMMIT
    PRINT "Commit eseguito. Verifico saldo ID=4:"
ISAM SEEK -1, 4
ISAM GET -1, clienteRec
    PRINT "  Saldo ID=4 dopo COMMIT: " + CONVERSIONS.CSTR(clienteRec.Saldo)
    PRINT "  Nome ID=4 dopo COMMIT: " + clienteRec.Nome
    IF clienteRec.Saldo = 1000 AND clienteRec.Nome = "Laura Neri (Aggiorna" THEN
        PRINT "  [ASSERT] COMMIT OK"
    ELSE
        PRINT "  [ASSERT] COMMIT FAIL: Saldo=" + CONVERSIONS.CSTR(clienteRec.Saldo) + " Nome='" + clienteRec.Nome + "'"
    END IF

    // ==============================================================================
    // Cancellazione e Manutenzione: DELETE, RECALL, PACK, REINDEX
    // ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "7. Cancellazione e Manutenzione"
    PRINT "-------------------------------------------------------------------------------"

    // DELETE (Legacy)
    ISAM SEEK Clienti, 4
    ISAM DELETE Clienti
    PRINT "Record ID 4 marcato come cancellato (Legacy DELETE)."
    PRINT "LASTREC(Clienti) attivo: " + CONVERSIONS.CSTR(ISAM.LASTREC(Clienti))

    // RECALL (Qualificata)
ISAM RECALL -1 // Richiama il record corrente (ID 4)
    PRINT "Record ID 4 richiamato (Qualified RECALL)."
    PRINT "LASTREC(Clienti) attivo: " + CONVERSIONS.CSTR(ISAM.LASTREC(Clienti))

    // Marca di nuovo per PACK
    ISAM SEEK Clienti, 4
    ISAM DELETE Clienti
    PRINT "Record ID 4 marcato di nuovo per cancellazione (Legacy DELETE)."

    // PACK (Legacy)
    ISAM PACK Clienti
    PRINT "Eseguito PACK su Clienti. Record cancellati rimossi fisicamente (Legacy)."
    PRINT "LASTREC(Clienti) dopo PACK: " + CONVERSIONS.CSTR(ISAM.LASTREC(Clienti))
    // Nota: PACK può invalidare il cursore, riposizioniamo
ISAM GO -1, TOP

    // REINDEX (Qualificata)
ISAM REINDEX -1
    PRINT "Eseguito REINDEX su Clienti. Indici ricostruiti (Qualified)."

    // ==============================================================================
    // Relazioni: SET RELATION
    // ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "8. Relazioni (SET RELATION)"
    PRINT "-------------------------------------------------------------------------------"

    // Popola Ordini con dati
ISAM SELECT Ordini
    ordineRec.ID_Ordine = 101
    ordineRec.ID_Cliente = 1
    ordineRec.Descrizione = "Ordine 1 Cliente 1"
    ordineRec.Importo = 50.00
    ordineRec.DataOrdine = DATETIME.DATEVALUE("2023-01-20")
ISAM APPEND ordineRec

    ordineRec.ID_Ordine = 102
    ordineRec.ID_Cliente = 1
    ordineRec.Descrizione = "Ordine 2 Cliente 1"
    ordineRec.Importo = 75.00
    ordineRec.DataOrdine = DATETIME.DATEVALUE("2023-01-25")
ISAM APPEND ordineRec

    ordineRec.ID_Ordine = 103
    ordineRec.ID_Cliente = 2
    ordineRec.Descrizione = "Ordine 1 Cliente 2"
    ordineRec.Importo = 120.00
    ordineRec.DataOrdine = DATETIME.DATEVALUE("2023-02-28")
ISAM APPEND ordineRec
ISAM SELECT Clienti // Torna a Clienti

    // SET RELATION (Legacy)
    ISAM SET RELATION ON Clienti TO ID INTO Ordini
    PRINT "Relazione Cliente (Clienti) a Ordini su ID stabilita (Legacy)."

    ISAM GO Clienti, TOP // Sposta il cursore nel padre
    PRINT "Navigazione con relazione:"
    DO WHILE NOT ISAM.EOF(Clienti)
       ISAM GET Clienti, clienteRec
       PRINT "  Cliente ID: " + CONVERSIONS.CSTR(clienteRec.ID) + ", Nome: " + clienteRec.Nome
ISAM SELECT Ordini // Seleziona l 'area figlio
       IF ISAM.FOUND(Ordini) THEN
          DO WHILE NOT ISAM.EOF(Ordini) AND ISAM.RECNO(Ordini) > 0 // Assicurati che non sia EOF e che il cursore sia valido
ISAM GET -1, ordineRec
             IF ordineRec.ID_Cliente = clienteRec.ID THEN
                PRINT "    - Ordine ID: " + CONVERSIONS.CSTR(ordineRec.ID_Ordine) + ", Importo: " + CONVERSIONS.CSTR(ordineRec.Importo)
ISAM SKIP -1
             ELSE
                EXIT DO // Ordini di un altro cliente
             END IF
          LOOP
       ELSE
          PRINT "    (Nessun ordine trovato per questo cliente)"
       END IF
ISAM SELECT Clienti // Torna al padre
       ISAM SKIP Clienti, 1
    LOOP

ISAM SET RELATION ON -1 // Sintassi legacy: Rimuove la relazione
    PRINT "Relazione rimossa."

    // ==============================================================================
    // 9. Operazioni I/O Massivo (COPY TO, SORT TO, EXPORT, IMPORT)
    // ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "9. Operazioni I/O Massivo (COPY TO, SORT TO, EXPORT, IMPORT)"
    PRINT "-------------------------------------------------------------------------------"

    // COPY TO (Legacy)
    ISAM COPY TO g_work + "/clienti_roma.isdb" ON Clienti FOR Citta = "Roma"
    PRINT "Copiati clienti di Roma in 'clienti_roma.isdb' (Legacy COPY TO)."

    // COPY TO (Qualificata)
ISAM COPY TO g_work + "/clienti_attivi.isdb" ON -1 FOR FN(rec) => rec.Attivo = TRUE
    PRINT "Copiati clienti attivi in 'clienti_attivi.isdb' (Qualified COPY TO)."

    // SORT TO (Legacy)
    ISAM SORT TO g_work + "/clienti_ordinati.isdb" ON Clienti ON Citta, Nome
    PRINT "Creato 'clienti_ordinati.isdb' ordinato per Citta e Nome (Legacy SORT TO)."

    // SORT TO (Qualificata)
ISAM SORT TO g_work + "/clienti_ordinati_q.isdb" ON -1 ON Nome, ID /D FOR FN(rec) => rec.Saldo > 0
    PRINT "Creato 'clienti_ordinati_q.isdb' ordinato per Nome e ID decrescente (Qualified SORT TO)."

    // EXPORT (Legacy)
    ISAM EXPORT Clienti TO g_work + "/clienti_export.csv" AS CSV
    PRINT "Esportato Clienti in 'clienti_export.csv' (Legacy EXPORT)."

    // EXPORT (Qualificata)
ISAM EXPORT Clienti TO g_work + "/clienti_export.json" AS JSON
    PRINT "Esportato Clienti in 'clienti_export.json' (Qualified EXPORT)."

    // IMPORT (Legacy)
    lResult = IO.FCREATE(g_work + "/temp_import.csv")
    IO.FWRITE(lResult, "ID,Nome,Citta,Saldo,Attivo,UltimoOrdine" + CHR(13) + CHR(10))
    IO.FWRITE(lResult, "5,Luca Rossi,Verona,750.50,TRUE,2023-05-01" + CHR(13) + CHR(10))
    IO.FWRITE(lResult, "6,Sara Bianchi,Bologna,1200.00,FALSE,2023-06-15" + CHR(13) + CHR(10))
    IO.FCLOSE(lResult)
    ISAM IMPORT Clienti FROM g_work + "/temp_import.csv" AS CSV
    PRINT "Importato da 'temp_import.csv' in Clienti (Legacy IMPORT)."
    KILL g_work + "/temp_import.csv"

    // IMPORT (Qualificata)
    lResult = IO.FCREATE(g_work + "/temp_import.json")
    IO.FWRITE(lResult, "[" + CHR(13) + CHR(10))
    IO.FWRITE(lResult, "  {""ID"":7,""Nome"":""Paolo Neri"",""Citta"":""Firenze"",""Saldo"":900,""Attivo"":true,""UltimoOrdine"":""2023-07-01""}" + CHR(13) + CHR(10))
    IO.FWRITE(lResult, "]" + CHR(13) + CHR(10))
    IO.FCLOSE(lResult)
ISAM IMPORT Clienti FROM g_work + "/temp_import.json" AS JSON
    PRINT "Importato da 'temp_import.json' in Clienti (Qualified IMPORT)."
    KILL g_work + "/temp_import.json"

    // ==============================================================================
    // Validazione, Backup
    // ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "10. Validazione e Backup"
    PRINT "-------------------------------------------------------------------------------"

    // VALIDATE ISAM (Legacy)
    ISAM VALIDATE Clienti
    PRINT "Eseguita validazione su Clienti (Legacy VALIDATE)."

    // VALIDATE ISAM (Qualificata)
ISAM VALIDATE Ordini
    PRINT "Eseguita validazione su Ordini (Qualified VALIDATE)."

    // BACKUP (Legacy)
    ISAM BACKUP Clienti TO g_work + "/clienti_backup.isdb"
    PRINT "Backup di Clienti in 'clienti_backup.isdb' (Legacy BACKUP)."

    // BACKUP (Qualificata)
    ISAM SELECT Ordini // Imposta l'area attiva su Ordini per il backup
ISAM BACKUP -1 TO g_work + "/ordini_backup.isdb"
    PRINT "Backup di Ordini in 'ordini_backup.isdb' (Qualified BACKUP)."

    // ==============================================================================
    // Conversione da DBF
    // ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "11. Conversione da DBF"
    PRINT "-------------------------------------------------------------------------------"
    
    TYPE DbfTestType
        Codice AS LONG
        Descrizione AS STRING * 30
        Valore AS CURRENCY
    END TYPE

    DIM dbfTestRec AS DbfTestType

    DBF CREATE g_work + "/dbf_source.dbf" FROM TYPE DbfTestType
    DBF USE g_work + "/dbf_source.dbf" ALIAS DbfSource AS DbfTestType
    DBF APPEND BLANK
    DBF REPLACE Codice WITH 10, Descrizione WITH "Articolo A", Valore WITH 12.34
    DBF APPEND BLANK
    DBF REPLACE Codice WITH 20, Descrizione WITH "Articolo B", Valore WITH 56.78
    DBF CLOSE

    ISAM CONVERT DBF g_work + "/dbf_source.dbf" TO ISAM g_work + "/isam_from_dbf.isdb" TYPE DbfTestType KEY "Codice" LEN 8
    PRINT "Convertito 'dbf_source.dbf' in 'isam_from_dbf.isdb'."

    ISAM OPEN g_work + "/isam_from_dbf.isdb" OF DbfTestType ALIAS IsamConverted INDEXED BY Codice
    PRINT "Verifica dati convertiti:"
    ISAM GO IsamConverted, TOP
    DO WHILE NOT ISAM.EOF(IsamConverted)
        ISAM GET IsamConverted, dbfTestRec
        PRINT "  Codice: " + CONVERSIONS.CSTR(dbfTestRec.Codice) + ", Desc: " + dbfTestRec.Descrizione + ", Val: " + CONVERSIONS.CSTR(dbfTestRec.Valore)
        ISAM SKIP IsamConverted, 1
    LOOP
    ISAM CLOSE IsamConverted
    KILL g_work + "/dbf_source.dbf"
    KILL g_work + "/dbf_source.gms" // DBF schema file
    KILL g_work + "/isam_from_dbf.isdb"

    // ==============================================================================
    // Pulizia Finale
    // ==============================================================================
    PRINT
    PRINT "-------------------------------------------------------------------------------"
    PRINT "12. Pulizia Finale"
    PRINT "-------------------------------------------------------------------------------"

    ISAM CLOSE Clienti
    ISAM CLOSE Ordini

    ' Pulisci i file nella subdirectory work/
    KILL cPath + "/clienti.isdb"
    KILL cPath + "/clienti.isdb.wal"
    KILL cPath + "/ordini.isdb"
    KILL cPath + "/ordini.isdb.wal"
    KILL cPath + "/clienti_backup.isdb"
    KILL cPath + "/ordini_backup.isdb"
    KILL cPath + "/clienti_roma.isdb"
    KILL cPath + "/clienti_attivi.isdb"
    KILL cPath + "/clienti_ordinati.isdb"
    KILL cPath + "/clienti_ordinati_q.isdb"
    KILL cPath + "/clienti_export.csv"
    KILL cPath + "/clienti_export.json"
    KILL cPath + "/temp_import.json"
    KILL cPath + "/dbf_source.dbf"
    KILL cPath + "/dbf_source.gms"
    KILL cPath + "/dbf_source.fpt"
    KILL cPath + "/dbf_source.wal"
    KILL cPath + "/isam_from_dbf.isdb"
    KILL cPath + "/isam_from_dbf.isdb.wal"
    RMDIR cPath

    PRINT
    PRINT "Test ISAM Completato!"

END SUB
