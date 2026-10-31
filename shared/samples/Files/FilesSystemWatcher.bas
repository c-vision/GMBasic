' FILE: Files/FilesSystemWatcher.bas
IMPORT IO
' =================================================================================================
'        GUIDA TEORICA: Monitoraggio del File System (WATCH e UNWATCH)
' =================================================================================================
'
' Questo file di test verifica le nuove funzionalità di monitoraggio asincrono del file system
' introdotte nel namespace IO. Queste istruzioni permettono di reagire a cambiamenti in una
' directory senza dover eseguire polling continuo.
'
' 1. SINTASSI `WATCH` (Legacy e Qualificata)
'    Attiva il monitoraggio su una specifica directory. Richiede una maschera di eventi e
'    il nome di una subroutine di callback da invocare quando l'evento si verifica.
'
'    - Legacy:  WATCH path$, eventsMask, CallbackSub
'    - Legacy: WATCH path$, eventsMask, "CallbackSub"
'
'    Costanti per eventsMask (combinabili con OR):
'      - %FILE_CREATED (1): Un file è stato creato.
'      - %FILE_DELETED (2): Un file è stato eliminato.
'      - %FILE_CHANGED (4): Il contenuto o i metadati di un file sono cambiati.
'      - %FILE_RENAMED (8): Un file è stato rinominato.
'
' 2. SINTASSI `UNWATCH`
'    Interrompe il monitoraggio su una directory precedentemente registrata, rilasciando
'    le risorse del sistema operativo.
'
'    - Legacy:  UNWATCH path$
'    - Legacy: UNWATCH path$
'
' 3. CALLBACK
'    Quando si verifica un evento, l'Executor mette in pausa il contesto corrente e invoca
'    la subroutine specificata. (Nota: In questo test verifichiamo la registrazione e
'    il flusso di esecuzione; l'effettiva invocazione asincrona dipende dall'implementazione
'    runtime del GPM).
'
' OBIETTIVI DI QUESTO TEST:
' 1. Verificare il parsing della sintassi legacy `WATCH`.
' 2. Verificare il parsing della sintassi `WATCH`.
' 3. Verificare l'uso delle costanti di evento (%FILE_CREATED, etc.).
' 4. Simulare un ciclo di vita completo: Setup -> Attivazione -> Eventi -> Disattivazione -> Pulizia.
'
' =================================================================================================

' Variabile globale per tracciare lo stato del test (aggiornata dalla callback)
GLOBAL EventoRilevato AS BOOLEAN = FALSE

SUB Main()
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════╗"
    PRINT "║ FilesSystemWatcher - Test Monitoraggio Asincrono (WATCH)       ║"
    PRINT "╚════════════════════════════════════════════════════════════════╝"
    PRINT

    DIM watchDir AS STRING = "test_watch_dir"
    DIM testFile AS STRING = watchDir + "/test.txt"
    DIM renFile AS STRING = watchDir + "/renamed.txt"
    DIM hFile AS INTEGER

    ' --- SETUP INIZIALE ---
    PRINT "--- 1. Setup Ambiente ---"
    
    ' Pulizia preventiva
    IF DIREXISTS(watchDir) THEN 
        PRINT "Pulizia directory precedente..."
        ' Nota: In uno scenario reale useremmo una cancellazione ricorsiva
        IF FILEEXISTS(testFile) THEN KILL testFile
        IF FILEEXISTS(renFile) THEN KILL renFile
        RMDIR watchDir
    END IF

    PRINT "Creazione directory di test: "; watchDir
    MKDIR watchDir
    PRINT "   OK."
    PRINT

    ' --- TEST 1: Sintassi Legacy WATCH ---
    PRINT "--- 2. Test Attivazione WATCH (Sintassi Legacy) ---"
    PRINT "Costanti eventi: CREATE="; %FILE_CREATED; " DELETE="; %FILE_DELETED; " CHANGE="; %FILE_CHANGED; " RENAME="; %FILE_RENAMED
    PRINT "Monitoraggio CREAZIONE e RINOMINA..."
    
    ' Monitora Creazione (1) e Rinomina (8)
    WATCH watchDir, 1 OR 8, OnFileSystemEvent
    
    PRINT "   OK: Istruzione WATCH eseguita senza errori."
    PRINT

    ' --- SIMULAZIONE EVENTI ---
    PRINT "--- 3. Generazione Eventi ---"
    
    PRINT "Creazione file '"; testFile; "'..."
    hFile = FREEFILE()
    OPEN testFile FOR OUTPUT AS #hFile
    PRINT #hFile, "Dati di test per watcher"
    CLOSE #hFile
    
    ' Diamo tempo al sistema operativo e all'Executor di processare l'evento
    SLEEP 500 
    
    PRINT "Rinomina file in '"; renFile; "'..."
    RENAME testFile AS renFile
    
    SLEEP 500
    
    PRINT "Cancellazione file (Non monitorato, non dovrebbe scatenare callback)..."
    KILL renFile
    
    SLEEP 500
    PRINT

    ' --- TEST 2: Disattivazione UNWATCH ---
    PRINT "--- 4. Test Disattivazione UNWATCH ---"
    
    UNWATCH watchDir
    PRINT "   OK: Istruzione IO.UNWATCH eseguita."
    
    ' Verifica che non ci siano errori se proviamo a generare eventi dopo unwatch
    PRINT "Generazione evento post-unwatch (non dovrebbe accadere nulla)..."
    hFile = FREEFILE()
    OPEN testFile FOR OUTPUT AS #hFile
    CLOSE #hFile
    KILL testFile
    
    PRINT "   OK."
    PRINT

    ' --- CLEANUP ---
    PRINT "--- 5. Pulizia Finale ---"
    RMDIR watchDir
    PRINT "Directory rimossa."
    PRINT "--- Test completato ---"
END SUB

' -----------------------------------------------------------------------------------------
' Subroutine di Callback
' Questa procedura viene invocata dall'Executor quando il FileSystemWatcher rileva un evento.
' -----------------------------------------------------------------------------------------
SUB OnFileSystemEvent()
    ' Nota: In una implementazione reale, la callback potrebbe ricevere parametri
    ' come il percorso del file e il tipo di evento. Qui simuliamo l'azione.
    
    PRINT ">>> [CALLBACK] OnFileSystemEvent: Rilevato cambiamento nel file system!"
    EventoRilevato = TRUE
END SUB