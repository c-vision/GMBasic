' FILE: Http/HttpRoutes.bas
' =================================================================================================
'       GUIDA TEORICA: ROUTING AVANZATO E MIDDLEWARE NEL SERVER HTTP
' =================================================================================================
'
' Questo test dimostra le capacità di routing avanzate del server HTTP integrato, che permettono
' di gestire diverse URL e metodi HTTP con logiche separate e di intercettare le richieste
' con del codice "middleware".
'
' 1. ROUTING BASATO SU PERCORSO E METODO (`HTTP ROUTE`)
'    - `HTTP ROUTE` permette di associare una specifica subroutine a una combinazione di metodo
'      HTTP (`GET`, `POST`) e a un template di percorso (es. `/users/{id}`).
'
' 2. MIDDLEWARE (`HTTP USE`)
'    - Il middleware è una funzione che viene eseguita *prima* del gestore della rotta finale.
'      È ideale per compiti trasversali come logging, autenticazione o parsing di body.
'
' 3. FLUSSO DI CONTROLLO DEL MIDDLEWARE (`HTTP USENEXT` e `NEXTHANDLER`)
'    Il cuore del sistema middleware è la gestione del passaggio del controllo.
'
'    A. Firma della Subroutine Middleware (Due Varianti):
'       Una subroutine middleware può avere due firme:
'
'       - `SUB MioMiddleware(req AS HTTPREQUEST, res AS HTTPRESPONSE)`
'         Questa è la firma standard. Viene usata per operazioni sincrone. Per passare il
'         controllo, si usa semplicemente il comando `HTTP USENEXT`.
'
'       - `SUB MioMiddleware(req AS HTTPREQUEST, res AS HTTPRESPONSE, next AS NEXTHANDLER)`
'         Questa firma avanzata aggiunge un terzo parametro, `next AS NEXTHANDLER`.
'         **L'utilità di questo parametro è ESCLUSIVAMENTE per scenari ASINCRONI complessi.**
'
'    B. A Cosa Serve `next AS NEXTHANDLER`?
'       In un normale middleware sincrono, il parametro `next` è inutile. La chiamata `HTTP USENEXT`
'       funziona perfettamente senza di esso.
'
'       Il suo unico scopo è permettere di "salvare" la continuazione della catena di richiesta
'       per poterla invocare in un momento successivo da un'altra parte del programma.
'       Questo è fondamentale se il middleware deve avviare un'operazione in background
'       (es. una query su database con `GO SUB`) e attendere il risultato prima di decidere
'       se proseguire. La subroutine in background riceverà l'handle `next` e sarà lei a
'       chiamare `HTTP USENEXT nextHandle` per riprendere la catena.
'       L'Esempio 4 di questo file dimostra questo pattern avanzato.
'
'    Se `HTTP USENEXT` non viene mai chiamato, la catena si interrompe e la richiesta rimane
'    in attesa fino al timeout (a meno che il middleware non invii una risposta di errore).
'
' =================================================================================================

IMPORT HTTP
IMPORT JSON
IMPORT CONCURRENCY

' --- Database Fittizio in Memoria ---
GLOBAL Users AS DICT<LONG, JSON>
GLOBAL NextId AS LONG = 1

' --- MAIN: Setup del Server e delle Rotte ---
SUB Main
    CLS
    PRINT "╔═══════════════════════════════════════════════════════════════════╗"
    PRINT "║ HttpRoutes - Test Routing Avanzato e Middleware                   ║"
    PRINT "╚═══════════════════════════════════════════════════════════════════╝"
    PRINT
    
    ' Popola il database fittizio
    Users = NEW DICT<LONG, JSON>

    ' --- REGISTRAZIONE MIDDLEWARE ---
    HTTP USE LoggerMiddleware()         ' Esempio 1: Middleware senza parametro 'next'.
    HTTP USE "/api", AuthMiddleware()   ' Esempio 2: Middleware con parametro 'next', ma usato implicitamente.
    HTTP USE "/api", AdvancedMiddleware() ' Esempio 3: Middleware che usa l'handle 'next' esplicitamente.
    HTTP USE "/api/users", AsyncMiddleware()    ' Esempio 4: Dimostra il pattern asincrono, il vero scopo di 'next'.

    ' --- REGISTRAZIONE DELLE ROTTE API ---
    HTTP ROUTE "GET", "/api/users", GetAllUsers()
    HTTP ROUTE "POST", "/api/users", CreateUser()
    HTTP ROUTE "GET", "/api/users/{id}", GetUserById()
    HTTP ROUTE "PUT", "/api/users/{id}", UpdateUser()
    HTTP ROUTE "DELETE", "/api/users/{id}", DeleteUser()
    HTTP ROUTE "GET", "/", ShowHomePage()
    
    ' Avvia il server.
    HTTP SERVER 8096
    
    PRINT "Server HTTP avviato su http://localhost:8096"
    PRINT "API Utenti disponibile su /api/users"
    PRINT "Esempio richiesta autorizzata: curl -H ""X-Auth-Token: password_segreta"" http://localhost:8096/api/users"

    ' --- Client di test autonomo: esercita la catena completa e ferma il server ---
    DIM wg AS WAITGROUP
    GO SUB ClientTest(wg) IN wg
    WAIT wg

    HTTP SERVER STOP
    PRINT "Server arrestato."
END SUB

SUB ClientTest(wg AS WAITGROUP)
    SLEEP 300

    DIM closeOpts AS HTTPOPTIONS = NEW HTTPOPTIONS
    closeOpts.Headers.Connection = "close"

    ' 1. Home page.
    DIM homeRes AS FETCHRESULT = FETCH("http://127.0.0.1:8096/", closeOpts)
    PRINT "[CLIENT] GET /: "; homeRes.StatusCode

    ' 2. Richiesta non autorizzata (nessun token) -> atteso 401.
    DIM unauthRes AS FETCHRESULT = FETCH("http://127.0.0.1:8096/api/users", closeOpts)
    PRINT "[CLIENT] GET /api/users (no token): "; unauthRes.StatusCode; " "; unauthRes.Body

    ' 3. Richiesta autorizzata con token -> atteso 200.
    DIM authOpts AS HTTPOPTIONS = NEW HTTPOPTIONS
    authOpts.Headers.Connection = "close"
    authOpts.Headers.Add("X-Auth-Token", "password_segreta")
    DIM listRes AS FETCHRESULT = FETCH("http://127.0.0.1:8096/api/users", authOpts)
    PRINT "[CLIENT] GET /api/users (con token): "; listRes.StatusCode; " "; listRes.Body

    ' 4. Crea un utente (esercita anche AsyncMiddleware, il pattern GO SUB su /api/users).
    DIM createOpts AS HTTPOPTIONS = NEW HTTPOPTIONS
    createOpts.Method = "POST"
    createOpts.Body = "{""nome"":""Mario""}"
    createOpts.Headers.ContentType = "application/json"
    createOpts.Headers.Add("X-Auth-Token", "password_segreta")
    createOpts.Headers.Connection = "close"
    DIM createRes AS FETCHRESULT = FETCH("http://127.0.0.1:8096/api/users", createOpts)
    PRINT "[CLIENT] POST /api/users: "; createRes.StatusCode; " "; createRes.Body

    ' 5. Rilegge l'utente appena creato.
    DIM getRes AS FETCHRESULT = FETCH("http://127.0.0.1:8096/api/users/1", authOpts)
    PRINT "[CLIENT] GET /api/users/1: "; getRes.StatusCode; " "; getRes.Body

    DONE wg
END SUB


' ============================================================================
' --- GESTORI DELLE ROTTE (Route Handlers) ---
' ============================================================================

SUB ShowHomePage(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    res.ContentType = "text/html"
    res.Send("<h1>Server Attivo</h1><p>Usa un client API per testare /api/users</p>")
END SUB

SUB GetAllUsers(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    res.SendJSON(Users.Values().ToJson())
END SUB

SUB CreateUser(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM newUser AS JSON = JSON.Parse(req.Body)
    newUser.id = NextId
    Users[NextId] = newUser
    NextId = NextId + 1
    res.StatusCode = 201 ' Created
    res.SendJSON(newUser)
END SUB

SUB GetUserById(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM id AS LONG = VAL(req.Params["id"])
    IF Users.ContainsKey(id) THEN
        res.SendJSON(Users[id])
    ELSE
        res.StatusCode = 404
        res.SendJSON({"error": "Utente non trovato"})
    END IF
END SUB

SUB UpdateUser(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM id AS LONG = VAL(req.Params["id"])
    IF Users.ContainsKey(id) THEN
        DIM updatedData AS JSON = JSON.Parse(req.Body)
        Users[id].nome = updatedData.nome
        res.SendJSON(Users[id])
    ELSE
        res.StatusCode = 404
        res.SendJSON({"error": "Utente non trovato"})
    END IF
END SUB

SUB DeleteUser(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM id AS LONG = VAL(req.Params["id"])
    IF Users.Remove(id) THEN
        res.StatusCode = 204 ' No Content
        res.Send("")
    ELSE
        res.StatusCode = 404
        res.SendJSON({"error": "Utente non trovato"})
    END IF
END SUB


' ============================================================================
' --- MIDDLEWARE ---
' ============================================================================

' Esempio 1: Middleware semplice **SENZA** il parametro 'next'.
' Questa è la forma più comune per operazioni sincrone.
SUB LoggerMiddleware(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    PRINT "[LOG] ", NOW(), " - ", req.Method, " ", req.Path
    ' Chiama semplicemente HTTP USENEXT per passare il controllo.
    HTTP USE NEXT
END SUB

' Esempio 2: Middleware di autenticazione **CON** il parametro 'next', ma che lo ignora.
' Dimostra che la presenza del parametro è opzionale e che la chiamata implicita funziona comunque.
SUB AuthMiddleware(req AS HTTPREQUEST, res AS HTTPRESPONSE, next AS NEXTHANDLER)
    PRINT "[AUTH] Controllo autorizzazione per rotta API..."
    DIM token$ = req.Headers.Get("X-Auth-Token")
    
    IF token$ = "password_segreta" THEN
        PRINT "[AUTH] Autorizzato."
        ' L'utente è autorizzato. Chiama HTTP USENEXT senza specificare l'handle 'next'.
        HTTP USE NEXT
    ELSE
        PRINT "[AUTH] NON Autorizzato. Blocco la richiesta."
        ' Interrompe la catena e invia una risposta di errore. NON chiama HTTP USENEXT.
        res.StatusCode = 401 ' Unauthorized
        res.SendJSON({"error": "Token di autenticazione non valido o mancante."})
    END IF
END SUB

' Esempio 3: Middleware che usa **esplicitamente** l'handle 'next'.
' Il risultato è funzionalmente identico all'Esempio 2, ma dimostra la sintassi esplicita.
' Questa sintassi è necessaria solo in scenari avanzati come quello dell'Esempio 4.
SUB AdvancedMiddleware(req AS HTTPREQUEST, res AS HTTPRESPONSE, myNextHandler AS NEXTHANDLER)
    PRINT "[ADVANCED] Middleware con handle esplicito in esecuzione."
    ' Passa esplicitamente l'handle.
    HTTP USE NEXT myNextHandler
END SUB

' Esempio 4: Dimostrazione del VERO scopo dell'handle 'next' in uno scenario asincrono.
SUB AsyncMiddleware(req AS HTTPREQUEST, res AS HTTPRESPONSE, next AS NEXTHANDLER)
    PRINT "[ASYNC] Middleware asincrono avviato. Avvio operazione in background..."
    ' In uno scenario reale, qui si potrebbe avviare una query su DB o un'altra chiamata di rete.
    ' Passiamo l'handle 'next' alla subroutine in background.
    GO SUB OperazioneLunga(req, res, next)
    ' La funzione middleware termina QUI, ma la richiesta non è ancora passata all'handler successivo.
END SUB

' Subroutine helper che simula un'operazione asincrona.
' Viene eseguita in una goroutine separata.
SUB OperazioneLunga(req AS HTTPREQUEST, res AS HTTPRESPONSE, nextHandle AS NEXTHANDLER)
    SLEEP 500 ' Simula un lavoro di 500ms
    PRINT "[ASYNC] Operazione in background completata."
    
    ' Dopo aver completato il lavoro, è questa subroutine che decide di
    ' riprendere la catena di middleware usando l'handle che le è stato passato.
    ' Senza l'handle, sarebbe stato impossibile farlo da qui.
    HTTP USE NEXT nextHandle
END SUB