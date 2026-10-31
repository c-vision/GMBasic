' FILE: Http/HttpFetch.bas
' =================================================================================================
'       GUIDA TEORICA: RICHIESTE HTTP AVANZATE (FETCH)
' =================================================================================================
'
' Il comando `FETCH` è lo strumento universale di GM-BASIC per le comunicazioni HTTP/HTTPS.
' Supporta due modalità operative distinte (Sincrona e Asincrona) e offre un controllo
' granulare sulla risposta tramite l'oggetto strutturato `FETCHRESULT`.
'
' 1. L'OGGETTO `FETCHRESULT` (Dati di Ritorno)
' ---------------------------------------------------------------------------------------------
' Quando usato come funzione sincrona, FETCH restituisce un'istanza del tipo `FETCHRESULT`.
' Questo oggetto incapsula tutti i dettagli della risposta del server:
'
'   PROPRIETÀ:
'   - .StatusCode (LONG)   : Il codice di stato HTTP (es. 200, 404, 500).
'   - .IsSuccess  (BIT)    : True se StatusCode è nell'intervallo 200-299.
'   - .Body       (STRING) : Il contenuto della risposta come testo. Nota: l'accesso a questa
'                            proprietà carica l'intero contenuto in memoria.
'   - .Headers    (OBJECT) : Oggetto `HTTPHEADERS` per leggere metadati (es. .ContentType).
'   - .ContentLength (LONG): La dimensione in bytes del contenuto (se fornita dal server).
'   - .Url        (STRING) : L'URL effettivo che ha fornito la risposta (utile dopo redirect).
'
'   METODI:
'   - .SaveToFile(path$)   : Scarica il contenuto direttamente su disco (Streaming).
'                            Scrive i dati a blocchi man mano che arrivano dalla rete.
'                            È fondamentale per file grandi per non esaurire la RAM.
'   - .GetBodyBytes()      : Restituisce il contenuto come ARRAY OF BYTE (per dati binari).
'
' 2. MODALITÀ SINCRONA (Bloccante)
' ---------------------------------------------------------------------------------------------
' Sintassi: `DIM risposta AS FETCHRESULT = FETCH(url$, [opzioni])`
'
' - L'esecuzione del programma si arresta finché il server non risponde o scade il timeout.
' - È il metodo consigliato per script lineari o quando il dato è necessario immediatamente per proseguire.
' - Gestione Errori: Utilizzare `TRY...CATCH` o `ON ERROR` per gestire timeout o errori di connessione.
'
' 3. MODALITÀ ASINCRONA (Non Bloccante)
' ---------------------------------------------------------------------------------------------
' Sintassi: `FETCH url$ [WITH opzioni] INTO NomeCallback`
'
' - L'esecuzione continua immediatamente alla riga successiva. La richiesta avviene in background.
' - Quando la richiesta termina, l'interprete esegue la `SUB NomeCallback`.
' - Firma Callback: `SUB NomeCallback(responseBody AS STRING, errorMsg AS STRING)`
' - Nota: In modalità asincrona classica, si riceve direttamente il corpo come stringa.
'
' 4. CONFIGURAZIONE RICHIESTA (`HTTPOPTIONS`)
' ---------------------------------------------------------------------------------------------
' Per richieste complesse (POST, Header custom), passare un oggetto `HTTPOPTIONS`:
'   DIM opts AS NEW HTTPOPTIONS
'   opts.Method = "POST"
'   opts.Body = "{...json...}"
'   opts.Headers.ContentType = "application/json"
'   opts.Timeout = 5000 ' ms
'
' =================================================================================================

IMPORT HTTP
IMPORT JSON
IMPORT IO
IMPORT CONCURRENCY

SUB Main
    CLS
    PRINT "╔══════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ HttpFetch - FETCHRESULT, Streaming e Chiamate Asincrone                      ║"
    PRINT "╚══════════════════════════════════════════════════════════════════════════════╝"
    PRINT
    
    ON ERROR GOTO GestoreErrori

    '----------------------------------------------------------------------
    ' 1. Esempio Sincrono: GET e Ispezione FETCHRESULT
    '----------------------------------------------------------------------
    PRINT "--- 1. FETCH Sincrono (GET) ---"
    
    DIM res AS FETCHRESULT = FETCH("https://postman-echo.com/get")
    
    PRINT "Status Code: "; res.StatusCode
    PRINT "Successo:    "; res.IsSuccess
    PRINT "Tipo Cont.:  "; res.Headers.ContentType
    ' NOTE: il body di /get è l'ECHO dell'identità del client HTTP (User-Agent/Accept di
    ' libcurl vs HttpClient gestito differiscono) → lunghezza NON deterministica cross-leg.
    ' La proprietà ContentLength è dimostrata in sezione 3 sulla resource statica (stessa
    ' dimensione per ogni stack).
    PRINT

    '----------------------------------------------------------------------
    ' 2. Esempio Sincrono: POST con Opzioni e JSON
    '----------------------------------------------------------------------
    PRINT "--- 2. FETCH Sincrono (POST con Options) ---"
    
    DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS
    opts.Method = "POST"
    opts.Body = "{""prodotto"": ""GM-BASIC"", ""versione"": ""1.0""}"
    opts.Headers.ContentType = "application/json"
    opts.Headers.UserAgent = "GM-BASIC-Client/2.0"
    
    DIM postRes AS FETCHRESULT = FETCH("https://postman-echo.com/post", opts)
    
    IF postRes.IsSuccess THEN
        PRINT "POST riuscita (Status "; postRes.StatusCode; ")"
        DIM jsonRes AS JSON = JSON.PARSE(postRes.Body)
        PRINT "Dati ricevuti dal server: "; jsonRes("json")("prodotto")
    ELSE
        PRINT "POST fallita."
    END IF
    PRINT

    '----------------------------------------------------------------------
    ' 3. Esempio Sincrono: Streaming su File (SaveToFile)
    '----------------------------------------------------------------------
    PRINT "--- 3. FETCH Sincrono (Streaming / SaveToFile) ---"
    
    DIM imageUrl$ = "https://www.google.com/images/branding/googlelogo/1x/googlelogo_color_272x92dp.png"
    DIM localPath$ = "test_image.png"
    
    PRINT "Avvio download..."
    DIM imgRes AS FETCHRESULT = FETCH(imageUrl$)
    
    IF imgRes.IsSuccess THEN
        PRINT "Connessione OK. Scrittura su disco in corso..."
        ' ContentLength su resource statica: identico su tutte le gambe (5969 byte).
        PRINT "Body Len:    "; imgRes.ContentLength
        ' Questo metodo scrive a blocchi, risparmiando memoria
        imgRes.SaveToFile(localPath$)
        
        IF FileExists(localPath$) THEN
            PRINT "File salvato correttamente: "; localPath$
            PRINT "Dimensione: "; FolderSize(localPath$); " bytes"
            KILL localPath$ ' Pulizia
        ELSE
            PRINT "ERRORE: File non creato."
        END IF
    ELSE
        PRINT "Download fallito: "; imgRes.StatusCode
    END IF
    PRINT

    '----------------------------------------------------------------------
    ' 4. Esempio Asincrono: Callback
    '----------------------------------------------------------------------
    PRINT "--- 4. FETCH Asincrono (Callback) ---"
    PRINT "(Il programma continua mentre la richiesta è in corso...)"
    
    DIM wg AS WAITGROUP
    
    ' Avviamo il fetch asincrono dentro una Goroutine per attendere il completamento col WaitGroup
    GO SUB EseguiAsync(wg) IN wg
    
    PRINT "--> Istruzione successiva al lancio del task..."
    
    ' Attende che la callback sia stata eseguita
    WAIT wg
    
    PRINT
    PRINT "--- Tutti i test completati ---"
    END

GestoreErrori:
    PRINT "ERRORE DI RUNTIME: "; ERR.Description
    RESUME NEXT
END SUB

' -----------------------------------------------------------------------------
' Helper per la parte Asincrona
' -----------------------------------------------------------------------------

SUB EseguiAsync(wg AS WAITGROUP)
    ' Sintassi Command: FETCH url [WITH options] INTO Callback
    ' Nota: La callback riceve (responseBody$, error$)
    FETCH "https://www.uuidtools.com/api/generate/v4" INTO HandleResponse()
END SUB

SUB HandleResponse(response$, error$)
    PRINT
    PRINT "   [CALLBACK ASINCRONA ESEGUITA]"
    IF error$ <> "" THEN
        PRINT "   Errore: "; error$
    ELSE
        ' Qui riceviamo la stringa grezza (comportamento legacy/semplificato per i comandi async)
        ' O, in futuro, potrebbe ricevere un FetchResult. Per ora assumiamo stringa.
        DIM j AS JSON = JSON.PARSE(response$)
        PRINT "   UUID Generato: "; j(0)   ' uuidtools ritorna ["uuid"]
    END IF
    PRINT "   [FINE CALLBACK]"
END SUB