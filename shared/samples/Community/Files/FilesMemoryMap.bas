REM FILE: Files/MemoryMap.bas
' =================================================================================================
'        GUIDA TEORICA: Memory Mapped Files (MEMORYMAP, IO.MMAPPEEK, MMAPPOKE, IO.MMAPREADBYTES, MMAPWRITEBYTES)
' =================================================================================================
'
' Questo file di test verifica le funzionalità avanzate di I/O tramite Memory Mapped Files (MMF) in GM-BASIC.
' Gli MMF permettono di mappare un file del disco direttamente nello spazio di indirizzamento della
' memoria virtuale del programma. Questo offre prestazioni eccezionali per l'accesso casuale a
' file di grandi dimensioni (anche GB/TB) senza la necessità di caricarli interamente in RAM.
' Il sistema operativo gestisce in modo efficiente il caricamento e lo scaricamento delle pagine del file.
'
' COMANDI E FUNZIONI INTRODOTTI:
'
' 1.  `MEMORYMAP.MEMORYMAP(filePath AS STRING) AS LONG` (Funzione)
'     Mappa un file specificato da `filePath` in memoria e restituisce un handle numerico (`LONG`)
'     che rappresenta la mappatura. Questo handle è necessario per tutte le altre operazioni MMF.
'     Il file verrà creato se non esiste.
'     Se il file esiste, la sua dimensione fisica viene mantenuta.
'
' 2.  `MEMORYMAP.MMAPPOKE(handle AS LONG, offset AS LONG, value AS BYTE)` (Comando/Statement)
'     Scrive un singolo `byte` (`value`) all' `offset` specificato (0-based) all'interno del file
'     mappato dall' `handle` fornito.
'     Accesso diretto e veloce in scrittura per i Memory Mapped Files.
'
' 3.  `MEMORYMAP.MMAPPEEK(handle AS LONG, offset AS LONG) AS BYTE` (Funzione)
'     Legge un singolo `byte` dall' `offset` specificato (0-based) all'interno del file
'     mappato dall' `handle` fornito, e restituisce il `byte` letto.
'     Accesso diretto e veloce in lettura per i Memory Mapped Files.
'
' 4.  `MEMORYMAP.MMAPREADBYTES(handle AS LONG, offset AS LONG, count AS LONG) AS BYTE()` (Funzione)
'     Legge un blocco di `count` byte a partire dall' `offset` specificato (0-based) dal file
'     mappato dall' `handle`. Restituisce un nuovo array di byte (`BYTE()`) contenente i dati letti.
'     Specifico per Memory Mapped Files.
'
' 5.  `MEMORYMAP.MMAPWRITEBYTES(handle AS LONG, offset AS LONG, sourceArray AS BYTE(), [count AS LONG])` (Comando/Statement)
'     Scrive un blocco di byte dal `sourceArray` nel file mappato dall' `handle`, a partire dall' `offset`.
'     - `sourceArray`: L'array di byte da cui leggere i dati.
'     - `count` (opzionale): Il numero di byte da scrivere. Se omesso, viene scritto l'intero contenuto
'       di `sourceArray`.
'     Specifico per Memory Mapped Files.
'
' OBIETTIVI DI QUESTO TEST:
' 1. Verificare il parsing e la registrazione di tutti i nuovi comandi e funzioni MMF.
' 2. Testare l'apertura e chiusura di un Memory Mapped File.
' 3. Testare le operazioni `MEMORYMAP.MMAPPOKE` e `MEMORYMAP.MMAPPEEK` per la scrittura e lettura di singoli byte.
' 4. Testare le operazioni `MEMORYMAP.MMAPREADBYTES` e `MEMORYMAP.MMAPWRITEBYTES` per la scrittura e lettura di blocchi di byte.
' 5. Dimostrare che gli accessi sono efficienti e corretti.
'
' =================================================================================================

IMPORT MEMORYMAP ' Importa il namespace per i comandi/funzioni MMF
IMPORT IO         ' Importa il namespace per i comandi IO generici (CLOSE, FILEEXISTS, KILL)

SUB Main()
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════╗"
    PRINT "║ FilesMemoryMap - Test Memory Mapped Files (IO.MEMORYMAP Suite) ║"
    PRINT "╚════════════════════════════════════════════════════════════════╝"
    PRINT

    DIM filePath AS STRING = "mapped_data.bin"
    DIM handleMMap AS LONG
    DIM dataSize AS LONG = 20 ' Dimensione del file mappato per i test (20 byte)
    DIM i AS LONG

    ' Array di byte per test di lettura/scrittura di blocchi
    DIM testBuffer(dataSize - 1) AS BYTE
    ' CORREZIONE: La dichiarazione è ora un semplice array di byte per corrispondere
    ' al tipo di ritorno di MMAPREADBYTES.
    DIM readBuffer(dataSize - 1) AS BYTE

    ON ERROR GOTO GestoreErrori

    ' --- 1. Preparazione del file ---
    PRINT "--- 1. Preparazione del file di test ('"; filePath; "') ---"
    ' Crea o sovrascrive il file
    OPEN filePath FOR BINARY AS #1 LEN = 1 ' Lunghezza minima, MEMORYMAP.MEMORYMAP può estenderlo
    CLOSE #1
    PRINT "   OK: File '"; filePath; "' preparato."
    PRINT

    ' --- 2. Mappatura del file in memoria (Funzione MEMORYMAP.MEMORYMAP) ---
    PRINT "--- 2. Mappatura del file (MEMORYMAP.MEMORYMAP) ---"
    PRINT "Mappo il file '"; filePath; "' in memoria come handle..."
    handleMMap = MEMORYMAP.MEMORYMAP(filePath) ' Usa MEMORYMAP.MEMORYMAP come funzione
    PRINT "   OK: File mappato. Handle: "; handleMMap
    PRINT "   Nota: Il file mappato ha ora dimensione minima. Le scritture estenderanno implicitamente."
    PRINT

    ' --- 3. Test MEMORYMAP.MMAPPOKE (scrittura singolo byte) ---
    PRINT "--- 3. Test MEMORYMAP.MMAPPOKE (Scrittura singoli byte) ---"
    PRINT "Scrivo valori da 0 a 9 nel file mappato ai primi 10 offset..."
    FOR i = 0 TO 9
        MMAPPOKE handleMMap, i, CBYTE(i)
        PRINT "   MEMORYMAP.MMAPPOKE handleMMap, "; i; ", "; CBYTE(i)
    NEXT i
    PRINT "   OK: Scrittura con MEMORYMAP.MMAPPOKE completata."
    PRINT

    ' --- 4. Test MEMORYMAP.MMAPPEEK (lettura singolo byte) ---
    PRINT "--- 4. Test MEMORYMAP.MMAPPEEK (Lettura singoli byte) ---"
    PRINT "Leggo i valori dai primi 10 offset con MEMORYMAP.MMAPPEEK..."
    FOR i = 0 TO 9
        DIM byteRead AS BYTE = MEMORYMAP.MMAPPEEK(handleMMap, i)
        IF byteRead = CBYTE(i) THEN
            PRINT "   MEMORYMAP.MMAPPEEK(h, "; i; ") -> "; byteRead; " (OK)"
        ELSE
            PRINT "   MEMORYMAP.MMAPPEEK(h, "; i; ") -> "; byteRead; " (ERRORE: Atteso "; CBYTE(i); ")"
            GOTO FineTest
        END IF
    NEXT i
    PRINT "   OK: Lettura con MEMORYMAP.MMAPPEEK completata e verificata."
    PRINT

    ' --- 5. Test MEMORYMAP.MMAPWRITEBYTES (scrittura di un blocco di byte) ---
    PRINT "--- 5. Test MEMORYMAP.MMAPWRITEBYTES (Scrittura blocco di byte) ---"
    PRINT "Preparo un buffer con valori da 10 a 29..."
    FOR i = 0 TO dataSize - 1
        testBuffer(i) = CBYTE(10 + i)
    NEXT i
    
    PRINT "Scrivo l'intero testBuffer() all'offset 0 con MEMORYMAP.MMAPWRITEBYTES..."
    MMAPWRITEBYTES handleMMap, 0, testBuffer()
    PRINT "   OK: MEMORYMAP.MMAPWRITEBYTES dell'intero buffer completata."
    PRINT

    ' --- 6. Test MEMORYMAP.MMAPREADBYTES (lettura di un blocco di byte) ---
    PRINT "--- 6. Test MEMORYMAP.MMAPREADBYTES (Lettura blocco di byte) ---"
    PRINT "Leggo un blocco di "; dataSize; " byte dall'offset 0 con MEMORYMAP.MMAPREADBYTES..."
    readBuffer() = MEMORYMAP.MMAPREADBYTES(handleMMap, 0, dataSize)
    PRINT "   OK: MEMORYMAP.MMAPREADBYTES completata. Verifico i dati..."
    
    FOR i = 0 TO dataSize - 1
        IF readBuffer(i) = testBuffer(i) THEN
            ' PRINT "   readBuffer("; i; ") -> "; readBuffer(i); " (OK)"
        ELSE
            PRINT "   readBuffer("; i; ") -> "; readBuffer(i); " (ERRORE: Atteso "; testBuffer(i); ")"
            GOTO FineTest
        END IF
    NEXT i
    PRINT "   OK: Dati letti con MEMORYMAP.MMAPREADBYTES verificati correttamente."
    PRINT

    ' --- 7. Test MEMORYMAP.MMAPWRITEBYTES con conteggio parziale ---
    PRINT "--- 7. Test MEMORYMAP.MMAPWRITEBYTES con conteggio parziale ---"
    DIM partialBuffer(4) AS BYTE
    FOR i = 0 TO 4 : partialBuffer(i) = CBYTE(i + 100) : NEXT i ' 100, 101, 102, 103, 104
    
    PRINT "Scrivo 3 byte da partialBuffer() all'offset 5 con MEMORYMAP.MMAPWRITEBYTES (conteggio parziale)..."
    MMAPWRITEBYTES handleMMap, 5, partialBuffer(), 3 ' Scrive 100, 101, 102 a offset 5, 6, 7
    PRINT "   OK: Scrittura parziale con MEMORYMAP.MMAPWRITEBYTES completata."
    
    PRINT "Verifico i byte all'offset 5, 6, 7 (dovrebbero essere 100, 101, 102)..."
    IF MEMORYMAP.MMAPPEEK(handleMMap, 5) = 100 AND MEMORYMAP.MMAPPEEK(handleMMap, 6) = 101 AND MEMORYMAP.MMAPPEEK(handleMMap, 7) = 102 THEN
        PRINT "   OK: Byte parziali verificati correttamente."
    ELSE
        PRINT "   ERRORE: Dati parziali non corrispondenti."
        GOTO FineTest
    END IF
    PRINT


    ' --- 8. Chiusura Handle e Pulizia Finale ---
    PRINT "--- 8. Chiusura Handle e Pulizia Finale ---"
    CLOSE handleMMap ' IO.CLOSE è un comando di I/O generico, rimane nel namespace IO
    PRINT "   OK: Handle del Memory Mapped File chiuso."
    
    IF IO.FILEEXISTS(filePath) THEN ' IO.FILEEXISTS è una funzione di I/O generica
        KILL filePath ' IO.KILL è un comando di I/O generico
        PRINT "   OK: File '"; filePath; "' eliminato."
    ELSE
        PRINT "   WARNING: Il file '"; filePath; "' non esiste per la pulizia."
    END IF
    PRINT

    GOTO FineTest

GestoreErrori:
    PRINT "   ERRORE DI RUNTIME: "; ERR.NUMBER; " - "; ERR.DESCRIPTION; " (Riga: "; ERR.LINENUMBER; ")"
    PRINT "   Contesto: File '"; ERR.SOURCELINETEXT; "'"
    ' Tenta di chiudere l'handle se era stato aperto
    IF handleMMap > 0 THEN CLOSE handleMMap
    ' Tentativo di pulizia file anche in caso di errore
    IF IO.FILEEXISTS(filePath) THEN
        ON ERROR GOTO EndErrorRecovery ' Evita loop se KILL fallisce
        KILL filePath
        PRINT "   (Tentativo di pulizia file '"; filePath; "' completato.)"
    END IF
EndErrorRecovery:
    PRINT "--- Test IO.MEMORYMAP interrotto a causa di errore ---"
    END ' Termina il programma
    
FineTest:
    PRINT "--- Test IO.MEMORYMAP SUITE completato con successo ---"
END SUB