' FILE: Http/HttpMultiPart.bas
' =================================================================================================
'       GUIDA TEORICA: GESTIONE NATIVA DI UPLOAD FILE (multipart/form-data)
' =================================================================================================
'
' Questo test dimostra una delle funzionalità più potenti del server HTTP di GM-BASIC: il parsing
' nativo dei form `multipart/form-data`. Questo formato è lo standard web per inviare
' form che contengono sia campi di testo che upload di file.
'
' 1. IL PROBLEMA: PARSING MANUALE
' ---------------------------------------------------------------------------------------------
' Il corpo di una richiesta `multipart/form-data` è complesso. Contiene "parti" multiple,
' separate da un delimitatore univoco (boundary), ognuna con i propri header (es. Content-Disposition).
' Scrivere un parser manuale per analizzare questa struttura è difficile e prono a errori.
'
' 2. LA SOLUZIONE GM-BASIC: `req.ParseMultipartForm()`
' ---------------------------------------------------------------------------------------------
' GM-BASIC astrae completamente questa complessità con un singolo metodo e un set di tipi di dato
' strutturati, rendendo l'upload di file un'operazione semplice e sicura.
'
'   - METODO: `req.ParseMultipartForm() AS HTTPPOSTEDDATA`
'     - Disponibile sull'oggetto `HTTPREQUEST` (`req`).
'     - Analizza il corpo della richiesta in entrata.
'     - Se il `Content-Type` è corretto (`multipart/form-data`), restituisce un oggetto
'       `HTTPPOSTEDDATA` popolato con i dati.
'     - Se il `Content-Type` non è corretto o il parsing fallisce, restituisce `NIL`.
'
' 3. I NUOVI TIPI DI DATO
' ---------------------------------------------------------------------------------------------
'
'   - `HTTPPOSTEDDATA`: Il contenitore principale. Ha due proprietà:
'     - `.FormFields AS ARRAY OF FORMFIELD`: Un array di tutti i campi di testo.
'     - `.FileFields AS ARRAY OF FILEFIELD`: Un array di tutti i file caricati.
'
'   - `FORMFIELD`: Rappresenta un singolo campo di testo.
'     - `.Name AS STRING`: Il nome del campo (dall'attributo `name` del tag `<input>`).
'     - `.Value AS STRING`: Il valore del campo.
'
'   - `FILEFIELD`: Rappresenta un singolo file caricato.
'     - `.Name AS STRING`: Il nome del campo del form (dall'attributo `name` del tag `<input type="file">`).
'     - `.FileName AS STRING`: Il nome originale del file sul computer dell'utente.
'     - `.Data AS ARRAY OF BYTE`: Il contenuto binario del file, pronto per essere salvato.
'
' 4. FLUSSO DI LAVORO TIPICO
' ---------------------------------------------------------------------------------------------
'   1. Definire una rotta che accetta richieste `POST` (es. `/upload`).
'   2. Nel gestore della rotta, chiamare `req.ParseMultipartForm()`.
'   3. Controllare se il risultato è `NIL` e gestire l'errore (es. inviare un 400 Bad Request).
'   4. Iterare su `formData.FormFields` per leggere i campi di testo.
'   5. Iterare su `formData.FileFields` per processare ogni file (es. salvarlo su disco).
'   6. Inviare una risposta di successo al client.
'
' =================================================================================================

IMPORT HTTP
IMPORT IO
IMPORT CONCURRENCY

' --- Programma Principale ---
SUB Main()
    CLS
    
    ' Crea una directory temporanea per gli upload, se non esiste.
    DIM uploadDir$ = "uploads"
    IF NOT DirExists(uploadDir$) THEN
        MKDIR uploadDir$
    END IF

    PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ HttpMultiPart - Server per l'Upload di File (multipart/form-data)        ║"
    PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
    PRINT
    
    ' Definisce una rotta che risponde alle richieste POST sull'URL "/upload"
    HTTP ROUTE "POST", "/upload", HandleUpload()
    
    ' Avvia il server HTTP sulla porta 8094.
    HTTP SERVER 8094
    
    PRINT "Server HTTP in ascolto sulla porta 8094."
    PRINT "La rotta per l'upload è: POST http://localhost:8094/upload"
    PRINT
    PRINT "--- Per testare, esegui questo comando in un altro terminale: ---"
    PRINT
    PRINT "curl -X POST http://localhost:8094/upload -F " + CHR$(34) + "description=Questo è un file di test" + CHR$(34) + " -F " + CHR$(34) + "uploadFile=@./HttpMultiPart.bas" + CHR$(34)
    PRINT
    PRINT "--------------------------------------------------------------------"
    PRINT
    ' --- Client di test autonomo: invia una richiesta di esempio e ferma il server ---
    DIM wg AS WAITGROUP
    GO SUB ClientTest(wg) IN wg
    WAIT wg

    HTTP SERVER STOP
END SUB

SUB ClientTest(wg AS WAITGROUP)
    SLEEP 300

    DIM boundary$ = "----GMBasicBoundary1234567890"
    DIM crlf$ = CHR$(13) + CHR$(10)
    DIM body$ = "--" + boundary$ + crlf$
    body$ = body$ + "Content-Disposition: form-data; name=" + CHR$(34) + "description" + CHR$(34) + crlf$ + crlf$
    body$ = body$ + "Questo è un file di test" + crlf$
    body$ = body$ + "--" + boundary$ + crlf$
    body$ = body$ + "Content-Disposition: form-data; name=" + CHR$(34) + "uploadFile" + CHR$(34) + "; filename=" + CHR$(34) + "test.txt" + CHR$(34) + crlf$
    body$ = body$ + "Content-Type: text/plain" + crlf$ + crlf$
    body$ = body$ + "Contenuto del file di prova." + crlf$
    body$ = body$ + "--" + boundary$ + "--" + crlf$

    DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS
    opts.Method = "POST"
    opts.Body = body$
    opts.Headers.ContentType = "multipart/form-data; boundary=" + CHR$(34) + boundary$ + CHR$(34)
    opts.Headers.Connection = "close"
    DIM res AS FETCHRESULT = FETCH("http://127.0.0.1:8094/upload", opts)
    PRINT "[CLIENT] Status: "; res.StatusCode; " Body: "; res.Body
END SUB


' --- Gestore della Rotta di Upload ---
' Questa subroutine viene invocata automaticamente per ogni richiesta POST a /upload.
SUB HandleUpload(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    
    PRINT
    PRINT "--- Richiesta ricevuta sulla rotta /upload ---"
    
    ' 1. Esegue il parsing del corpo della richiesta.
    DIM formData AS HTTPPOSTEDDATA = req.ParseMultipartForm()
    
    ' 2. Controlla se il parsing ha avuto successo.
    IF formData IS NIL THEN
        PRINT "  -> Errore: Content-Type non è multipart/form-data o il corpo è malformato."
        res.StatusCode = 400 ' Bad Request
        res.Send("Content-Type non valido. Atteso multipart/form-data.")
        RETURN
    END IF
    
    ' 3. Processa i campi di testo.
    PRINT "  Campi di testo ricevuti:"
    IF UBOUND(formData.FormFields) = -1 THEN
        PRINT "    (Nessuno)"
    ELSE
        FOR EACH campo AS FORMFIELD IN formData.FormFields
            PRINT "    - "; campo.Name; " = "; CHR$(34); campo.Value; CHR$(34)
        NEXT
    END IF
    
    ' 4. Processa i file caricati.
    PRINT "  File ricevuti:"
    IF UBOUND(formData.FileFields) = -1 THEN
        PRINT "    (Nessuno)"
    ELSE
        FOR EACH file AS FILEFIELD IN formData.FileFields
            PRINT "    - Nome campo: "; file.Name
            PRINT "      Nome file originale: "; file.FileName
            
            ' Costruisce un percorso sicuro per salvare il file.
            DIM savePath$ = "uploads/" + file.FileName
            
            ' Salva i dati binari su disco.
            DIM hFile = FREEFILE()
            OPEN savePath$ FOR BINARY AS #hFile
            PUT #hFile, , file.Data
            CLOSE #hFile
            
            PRINT "      File salvato in: "; savePath$
            PRINT "      Dimensione: "; LEN(file.Data); " bytes"
        NEXT
    END IF
    
    ' 5. Invia una risposta di successo.
    res.StatusCode = 200
    res.Send("Upload completato con successo. Controlla la console del server per i dettagli.")
    
END SUB