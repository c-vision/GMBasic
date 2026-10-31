' FILE: Files/FilesLocking.bas
' =================================================================================================
'        GUIDA TEORICA: Gestione Avanzata dei File - Locking e Permessi
' =================================================================================================
'
' Questo file di test verifica le funzionalità avanzate di I/O su file in GM-BASIC,
' concentrandosi sulla gestione della concorrenza tramite locking esplicito e le modalità
' di apertura specificate nella clausola OPEN.
'
' 1. SINTASSI `OPEN` ESTESA
'    Oltre alle modalità base (INPUT, OUTPUT, RANDOM), il comando `OPEN` supporta clausole
'    per un controllo granulare sull'accesso e sulla condivisione dei file.
'    - `ACCESS READ | WRITE | READ WRITE`: Specifica i permessi con cui il file viene aperto.
'      Tentar di scrivere su un file aperto in `ACCESS READ` genererà un errore a runtime.
'    - `LOCK SHARED | READ | WRITE`: Definisce come il file può essere acceduto da *altri* processi
'      mentre è aperto dal nostro programma.
'      - `SHARED`: (Default) Altri processi possono leggere e scrivere.
'      - `LOCK READ`: Impedisce ad altri processi di leggere il file.
'      - `LOCK WRITE`: Impedisce ad altri processi di scrivere sul file.
'      - `LOCK READ WRITE`: Impedisce qualsiasi accesso da altri processi.
'
' 2. COMANDI `LOCK` E `UNLOCK`
'    Questi comandi permettono di bloccare e sbloccare dinamicamente parti di un file già aperto,
'    una funzionalità cruciale per applicazioni multi-utente o multi-tasking che operano su
'    un database condiviso (file RANDOM).
'    - `LOCK #handle`: Blocca l'intero file. Nessun altro processo può leggere o scrivere.
'    - `LOCK #handle, recordNum`: Blocca un singolo record. Altri processi possono ancora
'      accedere a record diversi.
'    - `UNLOCK #handle [, recordNum]`: Rilascia il blocco corrispondente.
'
' OBIETTIVI DI QUESTO TEST:
' 1. Verificare che il Parser riconosca correttamente la sintassi estesa di `OPEN`.
' 2. Verificare che il Parser riconosca la sintassi di `LOCK` e `UNLOCK` (sia legacy che qualificata).
' 3. Testare a runtime che l'Executor applichi i permessi `ACCESS` (tentativo di scrittura su file read-only).
'    (Nota: la verifica effettiva del locking tra processi non è automatizzabile in un singolo script
'     ma la correttezza sintattica e l'esecuzione senza errori dei comandi ne validano l'implementazione).
'
' =================================================================================================

TYPE TestRecord
    ID AS LONG
    Nome AS STRING * 30
END TYPE

SUB Main()
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════╗"
    PRINT "║ FilesLocking - Test Gestione Avanzata File (ACCESS e LOCK)     ║"
    PRINT "╚════════════════════════════════════════════════════════════════╝"
    PRINT

    DIM rec AS TestRecord
    DIM filePath AS STRING = "test_locking.dat"
    DIM handle AS INTEGER = 1

    ON ERROR GOTO GestoreErrori

    ' --- TEST 1: Sintassi OPEN estesa ---
    PRINT "--- 1. Test sintassi OPEN estesa ---"
    
    ' Apre un file con permessi di sola lettura
    PRINT "Apertura in modalità ACCESS READ..."
    OPEN filePath FOR RANDOM ACCESS READ AS #handle LEN = SIZEOF(rec)
    CLOSE #handle
    PRINT "   OK: File aperto e chiuso in ACCESS READ."

    ' Apre un file con lock in scrittura per altri processi
    PRINT "Apertura in modalità LOCK WRITE..."
    OPEN filePath FOR OUTPUT LOCK WRITE AS #handle
    PRINT #handle, "Test lock write"
    CLOSE #handle
    PRINT "   OK: File aperto e chiuso in LOCK WRITE."
    PRINT

    ' --- TEST 2: Validazione ACCESS READ a runtime ---
    PRINT "--- 2. Test scrittura su file in ACCESS READ (deve fallire) ---"
    rec.ID = 1
    rec.Nome = "Record Iniziale"
    
    ' Crea e popola il file
    OPEN filePath FOR RANDOM AS #handle LEN = SIZEOF(rec)
    PUT #handle, 1, rec
    CLOSE #handle
    
    ' Riapre in sola lettura
    OPEN filePath FOR RANDOM ACCESS READ AS #handle LEN = SIZEOF(rec)
    PRINT "File riaperto in ACCESS READ. Tento di scrivere..."
    
    rec.Nome = "Record Modificato"
    PUT #handle, 1, rec ' Questa operazione DEVE scatenare un errore
    
    PRINT "   ERRORE: La scrittura su un file read-only non ha fallito!"
    CLOSE #handle
    GOTO FineTest

GestoreErrori:
    IF ERR.NUMBER = 70 THEN ' Permission Denied
        PRINT "   OK: Tentativo di scrittura fallito come previsto (Errore 70: Permission Denied)."
        CLOSE #handle
        RESUME ProssimoTest
    ELSE
        PRINT "   ERRORE INATTESO: "; ERR.NUMBER; " - "; ERR.DESCRIPTION
        CLOSE #handle
        GOTO FineTest
    END IF
    
ProssimoTest:
    PRINT

    ' --- TEST 3: Comandi LOCK e UNLOCK ---
    PRINT "--- 3. Test comandi LOCK e UNLOCK (sintassi e esecuzione) ---"
    OPEN filePath FOR RANDOM AS #handle LEN = SIZEOF(rec)
    
    ' Blocca un record specifico
    PRINT "Blocco il record #1..."
    LOCK #handle, 1
    PRINT "   OK: LOCK #1, 1 eseguito."
    
    ' Sblocca il record
    PRINT "Sblocco il record #1..."
    UNLOCK #handle, 1
    PRINT "   OK: UNLOCK #1, 1 eseguito."
    
    ' Blocca l'intero file
    PRINT "Blocco l'intero file..."
    LOCK #handle
    PRINT "   OK: LOCK #1 eseguito."
    
    ' Sblocca l'intero file
    PRINT "Sblocco l'intero file..."
    UNLOCK #handle
    PRINT "   OK: UNLOCK #1 eseguito."
    
    ' Test comandi LOCK/UNLOCK
    PRINT "Test comandi LOCK/UNLOCK..."
    LOCK handle, 2
    PRINT "   OK: LOCK h, 2 eseguito."
    UNLOCK handle, 2
    PRINT "   OK: UNLOCK h, 2 eseguito."

    CLOSE #handle
    PRINT

FineTest:
    ' Pulizia finale
    IF FILEEXISTS(filePath) THEN KILL filePath
    PRINT "--- Test completato ---"
END SUB
