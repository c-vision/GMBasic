# IV.24 — Rete e web

## Cosa ottieni da questo capitolo

Un server HTTP che risponde, un client che chiama, e i socket quando HTTP non basta. La cosa che
cambia rispetto agli altri capitoli: qui il codice **aspetta**, e chi aspetta è un task GPM.

## 1. Server e client in un solo programma

Eseguito (`shared/samples/Http/HttpBodyJson.bas`, backend C99Native):

```
Server in ascolto sulla porta 8091
Invia una POST a http://localhost:8091/api/data con JSON body

Ricevuto: nome=test valore= 42
[CLIENT] Status:  200  Body: {"status":"ok","ricevuto":"test","timestamp":"2026-11-02T16:08:26"}
```

Il sample avvia il server su `8091`, gli manda una `POST` con un corpo JSON e stampa la risposta.
È il giro completo — ricezione, parsing del body, risposta, client — in un programma che termina
da solo, ed è per questo che questo esempio ha un output nel libro mentre i sample grafici no:
HTTP non richiede una finestra.

## 2. Il server: rotte e middleware

| Parte | A cosa serve | Sample |
|---|---|---|
| server e rotte | instradare un URL a un handler | `HttpRoutes.bas`, `HttpServer.bas` |
| body | leggere JSON, form, multipart | `HttpBodyJson.bas`, `HttpForms.bas`, `HttpMultiPart.bas` |
| cookie e sessione | stato fra richieste | `HttpServerCookies.bas`, `HttpSession.bas` |
| configurazione | porta, TLS, cartelle statiche | `HttpConfig.bas` |
| streaming | risposte lunghe senza buffer | guida `HTTP-Streaming-Guida-Utente` |
| MVC | controller, azioni, convenzioni | `Http-Mvc-Programmer-Manual`, `Mvc-Routing-Conventions` |

Un handler HTTP è una SUB: viene eseguita come task GPM, quindi può leggere un database o
chiamare un altro servizio senza bloccare il server.

## 3. Il client

Le chiamate uscenti stanno nel namespace HTTP (`FETCH` con callback per la versione asincrona):
`HttpFetch.bas`, `HttpFetchAdvanced.bas`, `HttpFetchIntoCallback.bas`. La forma con callback è
quella da usare in un programma con interfaccia: la chiamata parte, la UI continua, la risposta
arriva come evento.

## 4. Bilanciamento e più istanze

Un server che regge il carico non è un server più veloce: sono più istanze dietro qualcosa che
distribuisce. La guida `Load-Balancing-Guida-Utente` descrive la configurazione; il punto
operativo è che lo stato di sessione non può vivere nella memoria di una singola istanza —
altrimenti la seconda richiesta dell'utente finisce su un processo che non lo conosce.

## 5. Socket: TCP, UDP, TLS, WebSocket

| Livello | Namespace | Sample |
|---|---|---|
| TCP | `NET` | `NetTcpAdvanced.bas` |
| UDP | `NET` | `NetUdpAdvanced.bas` |
| TLS | `NET`/`SSL` | `NetSslConnect.bas`, `NetSslServer.bas` |
| errori TLS | — | `NetSslConnectFailures.bas`, `NetSslOpenFailures.bas` |
| WebSocket | `NET` | `WebSocketEcho.bas` |
| handler in GPM | — | `HttpGpmHandler.bas` |

I due sample di "failures" sono importanti quanto quelli che funzionano: un handshake TLS che
fallisce per certificato scaduto, nome sbagliato o protocollo non supportato va distinto, e quei
file mostrano i messaggi reali.

## 6. WebSocket, in due righe

Un WebSocket è un socket che parte da una richiesta HTTP e poi resta aperto nei due sensi. Il
sample `Network/WebSocketEcho.bas` manda un messaggio, riceve l'eco e chiude: è il test minimo che
dimostra che il canale è bidirezionale.

## 7. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| `address already in use` | la porta è occupata | cambia porta o chiudi il processo |
| la `POST` arriva vuota | body non letto prima della risposta | leggi il body nell'handler |
| il server si blocca con una richiesta | lavoro lungo sul thread di rete | dispatcha in un task GPM |
| TLS fallisce solo in produzione | certificato o nome host | controlla catena e `SNI` |
| sessione persa al secondo click | stato in memoria di un'istanza | sposta lo stato in un archivio condiviso |

## 8. Dove andare adesso

- **IV.25** — messaggistica e code, quando HTTP non è il canale giusto.
- **IV.26** — sicurezza: autenticazione, header, sessioni.
- **III.13** — il GPM che esegue gli handler.














<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Http-Mvc-Programmer-Manual

*Fonte: `Http-Mvc-Programmer-Manual`*


## 1. Panoramica

## 2. Il server HTTP

### 2.1 Configurazione (`HTTP CONFIG`)
| Proprietà | Tipo | Scopo |
|---|---|---|
| `Port` | INTEGER | porta di ascolto |
| `RequestTimeoutMs` | INTEGER | timeout richiesta (ms) |
| `DefaultRequestTimeout` | INTEGER | alias di `RequestTimeoutMs` |
| `MaxRequestBodySize` | LONG | limite corpo richiesta (byte; oltre → **413**) |
| `EnableKeepAlive` | BIT | keep-alive connessioni (default TRUE; rispettato anche dalle backend native) |
| `Http2` | BIT | offre **ALPN `h2`** sul server TLS (default FALSE: senza multiplexing h2 su una connessione è più lento di h1 con più connessioni; h2c resta accettato). Vedi [[HTTP-2-Milestone-1]] |
| `RateLimitRequestsPerMinute` | INTEGER | richieste/min consentite (oltre → **429**) |
| `RateLimitBurstSize` | INTEGER | burst (token bucket) |
| `CertificatePath` | STRING | TLS: percorso del certificato server — **PFX/PKCS#12** oppure **PEM** (certificato + chiave privata; se la chiave è in un file separato si cerca `<nome>-key.pem` / `<nome>.key` / `key.pem` nella stessa cartella) |
| `CertificatePassword` | STRING | password del certificato (solo PFX; ignorata per PEM) |
| `SecurityHeaders` | BIT | header di sicurezza automatici su ogni risposta |
| `AllowedIPs` | STRING | allow-list IP/CIDR (separati da virgola); vuoto = tutti |
| `BlockedIPs` | STRING | block-list IP/CIDR (oltre → **403**) |
| `Sessions` | BIT | abilita sessioni server-side su route/middleware |
| `SessionFile` | STRING | file SQLite per la persistenza sessioni (vuoto = in-memory) |

IMPORT HTTP
DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
cfg.Port = 8080
cfg.RequestTimeoutMs = 30000
cfg.MaxRequestBodySize = 15 * 1024 * 1024     ' 15 MB
cfg.SecurityHeaders = TRUE                     ' header di sicurezza su ogni risposta
cfg.AllowedIPs = "127.0.0.1, 10.0.0.0/8"      ' solo localhost + LAN 10/8
cfg.Sessions = TRUE                            ' sessioni su REQ("Session")
cfg.SessionFile = "work/sessions.db"           ' persistenza SQLite ("" = in-memory)
HTTP CONFIG cfg


### 2.2 Avvio/arresto (`HTTP SERVER` / `HTTP SERVER STOP`)

HTTP SERVER 8080        ' usa la config impostata (o la porta data)
' ... in un'altra SUB/onExit:
HTTP SERVER STOP


### 2.3 Route (`HTTP ROUTE`)

HTTP ROUTE "<METODO>", "<pattern>", Handler()


SUB InfoHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM risposta AS JSON = NEW JSON
    risposta.path = req.Path
    risposta.method = req.Method
    res.SendJSON(risposta)
END SUB

SUB UserHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM id AS LONG = VAL(req.Params["id"])     ' dal pattern {id}
    res.Send("User id: " + LTRIM$(STR(id)))
END SUB

HTTP ROUTE "GET", "/api/info", InfoHandler()
HTTP ROUTE "GET", "/api/users/{id}", UserHandler()


### 2.4 File statici (`HTTP STATIC`)

HTTP STATIC "/static", "./wwwroot"     ' URL prefix, cartella fisica


### 2.5 Middleware (`HTTP USE`)

SUB LoggerMiddleware(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    PRINT "[log] "; req.Method; " "; req.Path
END SUB

SUB AuthMiddleware(req AS HTTPREQUEST, res AS HTTPRESPONSE, next AS NEXTHANDLER)
    IF req.Headers.Get("Authorization") = "" THEN
        res.StatusCode = 401
        res.Send("{" + CHR$(34) + "error" + CHR$(34) + ":" + CHR$(34) + "no auth" + CHR$(34) + "}")
    ELSE
        HTTP USE NEXT          ' passa al prossimo handler della catena
    END IF
END SUB

HTTP USE LoggerMiddleware()      ' su tutto
HTTP USE "/api", AuthMiddleware() ' solo su /api/*


SUB AsyncMiddleware(req AS HTTPREQUEST, res AS HTTPRESPONSE, next AS NEXTHANDLER)
    GO SUB LavoroInBackground(req, res, next)   ' passa l'handle alla SUB
END SUB

SUB LavoroInBackground(req AS HTTPREQUEST, res AS HTTPRESPONSE, nh AS NEXTHANDLER)
    SLEEP 500                                    ' lavoro lungo (DB, rete, …)
    HTTP USE NEXT nh                             ' riprende la catena con l'handle
END SUB


### 2.6 WebSocket

SUB WsMessageHandler(ws AS WEBSOCKET, msg AS STRING)
    ws.Send("Echo: " + msg)
END SUB
SUB WsCloseHandler(ws AS WEBSOCKET)
    PRINT "[ws chiuso]"
END SUB
SUB WsErrorHandler(ws AS WEBSOCKET, err AS STRING)
    PRINT "[ws errore] "; err
END SUB
SUB WsHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE, ws AS WEBSOCKET)
    ws.OnMessage = "WsMessageHandler"
    ws.OnClose = "WsCloseHandler"
    ws.OnError = "WsErrorHandler"
END SUB

HTTP ROUTE "GET", "/ws", WsHandler()


### 2.7 Sicurezza del server HTTP (teoria e pratica)

IMPORT STRINGS
DIM safe$ = STRINGS.HTMLENCODE(userInput$)   ' entità per & < > " '
DIM link$ = STRINGS.URLENCODE(userInput$)    ' percent per URL/query
DIM js$   = STRINGS.JSENCODE(userInput$)     ' per embed in JS


## 3. La richiesta (`HTTPREQUEST`)

### 3.1 Proprietà
| Proprietà | Tipo | Descrizione |
|---|---|---|
| `Method` | STRING | `GET`/`POST`/... |
| `Path` | STRING | percorso richiesto |
| `QueryString` | STRING | query string grezza |
| `Body` | STRING | corpo (testo) |
| `BodyJson` | JSON | corpo JSON parsato (se Content-Type JSON) |
| `BodyBytes` | BYTE[] | corpo come byte |
| `Headers` | HTTPHEADERS | header della richiesta |
| `Params` | JSON | parametri dal pattern `{id}` |
| `Query` | JSON | query string parsata |
| `RemoteIp` | STRING | IP client |
| `SessionId` | STRING | id sessione dal cookie `GM_BASIC_SESSION` |

### 3.2 Metodi
| Metodo | Ritorna | Descrizione |
|---|---|---|
| `req.ParseForm()` | JSON | parsifica il body `application/x-www-form-urlencoded` in un JSON con le chiavi dei campi |
| `req.ParseMultipartForm()` | HTTPPOSTEDDATA | parsifica `multipart/form-data` |

### 3.3 Header (`HTTPHEADERS`)

DIM token AS STRING = req.Headers.Get("Authorization")
req.Headers.Add("X-Custom", "1")


### 3.4 Form e multipart

' form-urlencoded
DIM form AS JSON = req.ParseForm()
PRINT form.nome

' multipart (file upload)
DIM dati AS HTTPPOSTEDDATA = req.ParseMultipartForm()
' dati.FormFields → array FORMFIELD (Name, Value)
' dati.FileFields  → array FILEFIELD (Name, FileName, Data)


## 4. La risposta (`HTTPRESPONSE`)

### 4.1 Proprietà
| Proprietà | Tipo | Descrizione |
|---|---|---|
| `StatusCode` | INTEGER | 200/201/400/404/... |
| `ContentType` | STRING | media type della risposta |
| `Headers` | HTTPHEADERS | header della risposta |

### 4.2 Metodi
| Metodo | Descrizione |
|---|---|
| `res.Send(body)` | invia testo (completa la risposta) |
| `res.SendJSON(data)` | invia come `application/json` |
| `res.SendBytes(bytes[, contentType])` | invia byte |
| `res.SendFile(path[, contentType])` | invia un file dal disco come **byte esatti** (binario: uno ZIP/PNG con byte NUL non viene troncato) |
| `res.SetCookie(name, value[, path, httpOnly, secure])` | imposta un cookie |
| `res.SetStatus(code)` / `res.Status(code)` | imposta lo status (fix Z4) |
| `res.SetHeader(name, value)` | imposta un header (fix Z4) |
| `res.Redirect(url[, code])` | redirect (default 302) |
| `res.Render(page[, model])` | renderizza una pagina BSP |
| `res.StartStream(contentType)` | **apre la risposta in streaming** (chunked): header sul filo subito |
| `res.StartStreaming(ct)` / `res.Stream(ct)` | alias di `StartStream` |
| `res.Write(testo)` | accoda nel flusso (apre da solo se non è aperto) |
| `res.Flush()` | manda ORA i byte in coda al client |
| `res.EndStream()` / `res.CloseStream()` | chiude il flusso (no-op se non è aperto) |

res.StatusCode = 404
res.Send("{" + CHR$(34) + "error" + CHR$(34) + ":" + CHR$(34) + "Not found" + CHR$(34) + "}")
' oppure:
res.SetStatus(404)
res.SetHeader("Content-Type", "application/json")
res.SendJSON(payload)
res.SetCookie("GM_BASIC_SESSION", sessionId)


#### 4.3 `res.SendFile(path[, contentType])` — semantica completa
| Parametro | Obbligatorio | Descrizione |
|---|---|---|
| `path` | Sì | Path del file (relativo o assoluto). Il file è **letto subito** all'invocazione (non lazy): chi lo cancella subito dopo (es. `KILL tmp`) non svuota la risposta. |
| `contentType` | No | Media type esplicito dell'header `Content-Type`. Se **omesso**, è **inferito dall'estensione** di `path`. |
| caso | comportamento |
|---|---|
| `Range: bytes=a-b` / `a-` / `-n` | **206 Partial Content** con `Content-Range`, `Accept-Ranges: bytes` |
| fetta fuori dal file | **416** con `Content-Range: bytes */<tot>` |
| `If-None-Match` con l'`ETag` del file | **304** senza corpo (il client riusa la cache) |
| `HEAD` | stessi header di una GET (`Content-Length` compreso), nessun corpo |

## 5. Il client HTTP

### 5.1 `FETCH(url [, options])` → `FETCHRESULT`

DIM res AS FETCHRESULT = FETCH("https://postman-echo.com/get")
PRINT res.StatusCode
PRINT res.Body

| Proprietà | Tipo | Descrizione |
|---|---|---|
| `Method` | STRING | `GET`/`POST`/`PUT`/`DELETE`/... |
| `Body` | STRING | corpo della richiesta |
| `Headers` | HTTPHEADERS | header richiesta |
| `Timeout` | INTEGER | timeout (ms) |
| `UseCookies` | BIT | abilita cookie |
| `CookieJar` | COOKIEJAR | jar per i cookie (metodo `Clear`) |
| `Username`/`Password` | STRING | basic auth |
| `BearerToken` | STRING | bearer token |
| `ProxyUrl`/`ProxyUsername`/`ProxyPassword` | STRING | proxy |
| `ClientCertificatePath`/`...Password` | STRING | client cert |
| `AllowRedirects`/`MaxRedirects` | BIT/INTEGER | redirect |
| `AllowInsecureSSL` | BIT | TLS non verificato |
| `BrowserPreset` | STRING | preset user-agent |
| `PinnedCertificateSha256` | STRING | pinning |

IMPORT HTTP
DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS
opts.Method = "POST"
opts.Body = "{ \"nome\": \"x\" }"
opts.Headers.Add("Content-Type", "application/json")
opts.BearerToken = "tok123"
DIM res AS FETCHRESULT = FETCH("https://api.example.com/items", opts)
IF res.IsSuccess THEN
    res.SaveToFile("download.json")
END IF


### 5.2 Helper rapidi

DIM s1 AS STRING = GET("https://example.com/")          ' → STRING
DIM s2 AS STRING = POST("https://example.com/api", "a=1", "application/x-www-form-urlencoded")
DIM s3 AS STRING = PUT("https://example.com/api/1", "{ }", "application/json")
DIM s4 AS STRING = DELETE("https://example.com/api/1")


## 6. Le pagine BSP (Basic Server Pages)

### 6.1 Direttive e sintassi

<%@ LAYOUT "_Layout.bsp" %>          direttiva di layout
<%@ MODEL Tipo %>                    dichiara MODEL = NEW Tipo
<%@ IMPORT Ns, ... %>                import extra
<%@ CACHE 60 %>                      cache secondi
<% PRINT ...; %>                     codice BASIC inline
<% IF cond THEN %> ... <% END IF %>  controllo di flusso
@section Nome { ... }                sezione di layout
@RenderSection("Nome")               punto di inserzione (nel layout)
@Html.Partial("_Header.bsp")         partial inline
@Model.Prop                          stampa il campo del MODEL
<% RENDERBODY %>                     corpo della pagina (nel layout)


### 6.2 Esempio (layout + section + partial)

<!DOCTYPE html><html><head><title>@RenderSection("Title")</title></head>
<body><header>@Html.Partial("_Header.bsp")</header>
<main><% RENDERBODY %></main>
<footer>@RenderSection("Footer")</footer></body></html>


<%@ LAYOUT "_Layout.bsp" %>
@section Title { Registrazione }
<h1>Ciao</h1>
<% PRINT "utente="; QUERY["user"]; %>


## 7. Il framework MVC

### 7.1 Struttura

IMPORT HTTP
IMPORT JSON
IMPORT MVC

PUBLIC CLASS UsersController INHERITS MVC.HttpController
    ' GET /api/users/{id}
    PUBLIC SUB Get(id AS LONG)
        IF users[key].name <> "" THEN
            SELF.Response.SendJSON(users[key])
        ELSE
            SELF.Response.StatusCode = 404
            SELF.Response.Send("{\"error\":\"Not found\"}")
        END IF
    END SUB
    ' GET /api/users
    PUBLIC SUB GetAll()


### 7.2 Matrice dei verbi e route automatiche
| Metodo | Verbo | Route convenzionale | Parametri |
|---|---|---|---|
| `GetAll` | GET | `/api/<controller>` | nessuno |
| `Get` | GET | `/api/<controller>/{id}` | 1 |
| `Post` | POST | `/api/<controller>` | nessuno |
| `Put` | PUT | `/api/<controller>/{id}` | 1 |
| `Delete` | DELETE | `/api/<controller>/{id}` | 1 |
| `Patch` | PATCH | `/api/<controller>/{id}` | 1 |

### 7.3 Base `HttpController` (helper)

### 7.4 Binding per convenzione nome
| Sorgente | Accesso | Esempio |
|---|---|---|
| Form url-encoded | `req.ParseForm()` | `frm.nome` |
| Body JSON | `req.BodyJson` | `body.email` |
| Path param | passato dalla bridge | `Get(id AS LONG)` |
| Query string | `req.Query["x"]` / `QUERY["x"]` | `QUERY["page"]` |

### 7.5 Validazione esplicita (attributi = metadata inerti, gap Z6)

PUBLIC SUB Post()
    DIM frm AS JSON = SELF.Form(SELF.Request)
    DIM errors AS JSON = NEW JSON
    SELF.Required(errors, "nome", frm.nome, "Il nome è obbligatorio")
    SELF.Email(errors, "email", frm.email, "Email non valida")
    IF SELF.HasErrors(errors) THEN
        SELF.SendValidationErrors(SELF.Response, errors)   ' 400 + { errors }
    ELSE
        SELF.Response.StatusCode = 201
        SELF.Response.SendJSON(frm)
    END IF
END SUB


### 7.6 Esempio completo: API CRUD MVC

IMPORT HTTP
IMPORT JSON
IMPORT MVC

DIM articoli AS JSON = NEW JSON

PUBLIC CLASS ArticoliController INHERITS MVC.HttpController
    PUBLIC SUB GetAll()
        SELF.Response.SendJSON(articoli)
    END SUB
    PUBLIC SUB Get(id AS LONG)
        DIM key AS STRING = LTRIM$(STR(id))
        IF articoli[key].nome = "" THEN
            SELF.Response.StatusCode = 404
            SELF.Response.Send("{\"error\":\"Non trovato\"}")
        ELSE


## 8. Sessioni e cookie (teoria e uso)

### 8.1 Le sessioni "engine" (consigliato, 2026-09-02)

IMPORT HTTP
DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
cfg.Sessions = TRUE
HTTP CONFIG cfg

' --- handler di login ---
SUB Login(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    IF req.Form("user") = "admin" AND req.Form("pass") = "admin-pw" THEN
        REQ("Session")("user") = "admin"      ' scrittura viva
        REQ("Session")("role") = "admin"
        res.SendJSON({ status = "ok" })
    ELSE
        res.SetStatus(401)
    END IF
END SUB


### 8.2 Persistenza su file (opzionale)

cfg.SessionFile = "work/sessions.db"   ' "" = in-memory
HTTP CONFIG cfg


### 8.3 Modalità "legacy" (manual cookies)

### 8.4 Logout e scadenza

## 9. Gotcha e limiti (sintesi)

## 10. Riferimento rapido

' SERVER
HTTP CONFIG cfg                     HTTP SERVER porta       HTTP SERVER STOP
HTTP ROUTE "GET", "/p/{id}", H()    HTTP STATIC "/s", dir   HTTP USE "/api", M()
WebSocket: handler con (req, res, ws AS WEBSOCKET); ws.OnMessage = "Sub"

' RICHIESTA (req)
req.Method  req.Path  req.QueryString  req.Body  req.BodyJson  req.BodyBytes
req.Headers.Get("x")  req.Params["id"]  req.Query["q"]  req.RemoteIp  req.SessionId
req.ParseForm() → JSON          req.ParseMultipartForm() → HTTPPOSTEDDATA

' RISPOSTA (res)
res.StatusCode = 200  res.ContentType  res.Headers
res.Send(s)  res.SendJSON(o)  res.SendBytes(b)  res.SendFile(p[, ct])   ' p binario ok; ct inferito se omesso
res.SetStatus(c)  res.SetHeader(n, v)  res.SetCookie(n, v)  res.Redirect(u)

' CLIENT


## 11. Sicurezza aggiornata (2026-09-02) — cosa c'è e cosa manca

## 12. Librerie di sistema in include/ (IMPORT sempre risolto)

## 13. Parità 4 backend — membri HTTP/MVC toccati (agg. 2026-10-08)

### 13.1 Disponibilità per backend
| Membro | IL | PCode | VM/C | C99Native | Note |
|---|---|---|---|---|---|
| `HTTPRESPONSE.Send/SendJSON` | ✅ | ✅ | ✅ | ✅ (fix 2026-10-08) | — |
| `HTTPRESPONSE.SetStatus` | ✅ | ✅ | ✅ | ✅ (fix `70a876ee5`+emitter) | Z4 |
| `HTTPRESPONSE.SetHeader` | ✅ | ✅ | ✅ | ✅ (fix `65aa2f4a9`) | Z4 |
| `HTTPRESPONSE.Redirect` | ✅ | ✅ | ✅ | ✅ (fix `65aa2f4a9`) | — |
| `HTTPRESPONSE.SetCookie` | ✅ | ✅ | ✅ | ✅ | — |
| `HTTPREQUEST.ParseForm` | ✅ | ✅ | ✅ (**fix 2026-10-08**, era assente) | ✅ (**fix 2026-10-08**) | commit `3c62b0a71`/`60fb27b7a` |
| **campo EREDITATO via istanza** (`c.CampoDellaBase`) | ✅ (**fix 2026-10-08**) | ✅ | ✅ | ✅ | fix Linker su base qualificata; MiniTest `mvc/inherits_qualified` |
| `HTTPREQUEST.ParseMultipartForm` | ✅ | ✅ | ✅ | ✅ | — |
| `INHERITS <base qualificata>` (`MVC.HttpController`) | ✅ | ✅ | ✅ | ✅ (**fix `5e53bd478`/`60fb27b7a`**) | struct derivata FLAT con i campi ereditati |
| MVC discovery + bridge + route | ✅ | ✅ | ✅ | ✅ (port C, `83789ba80`) | — |
| Assegnazione indicizzata JSON `x[k]=v` | ✅ | ✅ | ✅ | ✅ | verificato 2026-10-08 (`DIM d AS JSON…: d["a"]=1` → `1`) |

### 13.2 Test e sample che coprono questa superficie

### 13.3 Divergenze APERTE trovate dalla verifica (con repro)

### HTTP-Streaming-Guida-Utente

*Fonte: `HTTP-Streaming-Guida-Utente`*


## 1. A cosa serve

res.StartStream("text/event-stream")   ' header sul filo ADESSO
res.Write("data: uno" + CHR$(10) + CHR$(10))
res.Flush()                            ' il client lo vede subito
res.EndStream()                        ' chiusura del flusso


## 2. Superficie completa
| Metodo | Parametri | Effetto |
|---|---|---|
| `res.StartStream(contentType)` | 1: media type | apre il flusso: status + header + `Transfer-Encoding: chunked`, subito |
| `res.StartStreaming(contentType)` | idem | **alias** di `StartStream` |
| `res.Stream(contentType)` | idem | **alias** di `StartStream` |
| `res.Write(testo)` | 1: testo | accoda nel flusso; se il flusso non è aperto lo **apre** (content-type default) |
| `res.Flush()` | nessuno | manda **ora** al client i byte in coda |
| `res.EndStream()` | nessuno | chiude il flusso (flush + chunk terminale) |
| `res.CloseStream()` | idem | **alias** di `EndStream` |
| Con lo stream aperto… | comportamento |
|---|---|
| `res.Send(testo)` | si comporta come `Write`: il testo va **nel flusso** (nessun output perso) |
| `res.SetStatus(n)` / `res.StatusCode = n` | **solo PRIMA** di `StartStream`: dopo, la testata è già partita |
| `res.SetHeader(nome, valore)` / `res.SetCookie(...)` | **solo PRIMA** di `StartStream` (idem) |
| `res.SendJSON(x)` / `res.SendFile(p)` | **non** sono consapevoli dello stream: una risposta già streammata non li invia. Usare `res.Write(JSON.STRINGIFY(x))` |

## 3. Sintassi e parametri, metodo per metodo

### 3.1 `res.StartStream(contentType)` (e alias `StartStreaming`, `Stream`)

res.StartStream(<contentType>)

| Parametro | Obbligatorio | Tipo | Descrizione |
|---|---|---|---|
| `contentType` | sì | STRING | Media type dell'header `Content-Type` (es. `"text/event-stream"`, `"application/x-ndjson"`, `"text/plain"`). Se stringa vuota, vale `text/plain; charset=utf-8`. |

### 3.2 `res.Write(testo)`

res.Write(<testo>)

| Parametro | Obbligatorio | Tipo | Descrizione |
|---|---|---|---|
| `testo` | sì | STRING | Byte da accodare nel flusso (UTF-8). Può essere vuoto (no-op). |

### 3.3 `res.Flush()`

res.Flush()


### 3.4 `res.EndStream()` (e alias `CloseStream`)

res.EndStream()
res.CloseStream()


## 4. Cosa cambia rispetto a `res.Send`
| | `res.Send(...)` | streaming |
|---|---|---|
| quando parte la risposta | alla **fine** dell'handler | **subito**, poi a ogni `Flush` |
| `Content-Length` | sì (calcolato) | no: `Transfer-Encoding: chunked` |
| gzip/br, ETag/304, render cache | sì | **no** (sono semantiche di una risposta completa) |
| timeout della richiesta | si applica | **non** si applica alla durata dello stream |
| quando la risposta è "pronta" | al primo `Send` | alla fine dell'handler (dopo `EndStream`) |

## 5. Esempi

### 5.1 SSE minimo (log live)

SUB Eventi(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM i AS INTEGER
  res.StartStream("text/event-stream")
  FOR i = 1 TO 3
    res.Write("data: evento " + LTRIM$(STR$(i)) + CHR$(10) + CHR$(10))
    res.Flush()
    SLEEP 300
  NEXT i
  res.EndStream()
END SUB


### 5.2 Export NDJSON (alias `Stream`/`CloseStream`, content-type preciso)

SUB Righe(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM i AS INTEGER
  res.Stream("application/x-ndjson")
  FOR i = 1 TO 1000
    res.Write("{" + CHR$(34) + "row" + CHR$(34) + ":" + LTRIM$(STR$(i)) + "}" + CHR$(10))
  NEXT i
  res.CloseStream()
END SUB


### 5.3 Status esplicito + `Send` dentro lo stream

SUB Creato(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.SetStatus(201)              ' PRIMA: entra nella status-line dello stream
  res.StartStreaming("text/plain")
  res.Send("phase1;")             ' con lo stream aperto, Send = Write
  res.Write("phase2")
  res.Flush()
  res.Send(";done")
  res.EndStream()
END SUB


### 5.4 Casi limite (tutti deterministici, coperti dal MiniTest)

res.EndStream()          ' nessuno stream: NO-OP → poi una risposta normale è valida
res.Write("x")           ' apre da solo lo stream (text/plain)
res.StartStream("a/b")   ' seconda chiamata: no-op (status/content-type non cambiano)
res.Write("y")           ' dopo EndStream: ignorata


## 6. Disponibilità sulle 4 backend
| backend | streaming HTTP/1.1 | HTTP/2 (DATA progressivi) |
|---|---|---|
| **IL** | ✅ | ❌ (server .NET, HTTP/1.1) |
| **PCodeVM** | ✅ | ❌ |
| **VM/C** | ✅ | ✅ (h2c sempre; h2 su TLS con `HTTP CONFIG Http2 = TRUE`) |
| **C99Native** | ✅ | ✅ (idem) |

## 7. MiniTest e sample di riferimento
| cosa | file |
|---|---|
| **tutta la superficie + alias + casi limite (10 casi)** | `shared/minitests/http/stream_methods.bas` |
| corpo chunked, `Transfer-Encoding` letto dal client, route normale dopo uno stream | `shared/minitests/http/stream_chunked.bas` |
| campi di `HTTP CONFIG` usati con lo streaming (`MaxRequestBodySize` → 413, `Http2`, `EnableKeepAlive`) | `shared/minitests/http/config_limits.bas` |
| **sample completo e complesso** (SSE + export NDJSON + POST 201, autoverifica) | `shared/samples/Http/HttpStreamingApp.bas` |
| sample introduttivo (SSE + route normale) | `shared/samples/Http/HttpStreaming.bas` |
| streaming su HTTP/2 (curl + gate) | `shared/samples/Http/Http2Server.bas`, `tools/sh/http2_smoke.sh` |
| app reale che usa SSE | `shared/webapp` → `GET /api/events/stream` |

zsh tools/sh/test_minitests_parity.sh "http/stream_methods.bas"   # 4 backend
zsh tools/sh/test_minitests_parity.sh "http/stream_chunked.bas"
zsh tools/sh/test_minitests_parity.sh "http/config_limits.bas"
zsh tools/sh/parity4 shared/samples/Http/HttpStreamingApp.bas


## 8. Errori da evitare

## 9. HTTP/2 in due righe

## 10. Riferimenti

### Load-Balancing-Guida-Utente

*Fonte: `Load-Balancing-Guida-Utente`*


## 1. Uso

REQUIRE "netlb"
IMPORT NETLB WITH ALIAS

SUB Main()
  DIM h AS LONG
  h = NETLB.POOLNEW(NETLB_P2C, 42)          ' policy P2C, seed 42 (0 = default)
  NETLB.BACKENDADD(h, "10.0.0.1:80", 1000, 0)   ' nome, peso (milli: 1000=1.0), cap (0=illim.)
  NETLB.BACKENDADD(h, "10.0.0.2:80", 1000, 0)
  DIM id AS LONG
  id = NETLB.PICK(h, "")                 ' "" = policy dinamica; "chiave" = policy d'affinita'
  NETLB.BEGINREQ(h, id)                  ' contabilita': 1 richiesta in corso
  ' ... usa il backend id ...
  NETLB.ENDREQ(h, id, 1, 12)             ' ok, latenza ms -> aggiorna EWMA + health passivo
  NETLB.RELEASE(h)
END SUB


## 2. Politiche (costanti globali `NETLB_*`)
| Costante | Policy | Quando |
|---|---|---|
| `NETLB_RR` | Round Robin | backend omogenei, stateless |
| `NETLB_WRR` | Smooth Weighted RR (nginx) | capacità diverse |
| `NETLB_RANDOM` | Random | base |
| `NETLB_LC` | Least Connections | durate variabili |
| `NETLB_WLC` | Weighted Least Connections | capacità + carico |
| `NETLB_LRT` | Least Response Time (EWMA) | latenza osservata |
| `NETLB_P2C` | **Power of Two Choices + EWMA** | **default consigliato** |
| `NETLB_CHASH` | Consistent Hashing + vnodes | affinità persistente |
| `NETLB_HRW` | Rendezvous / HRW | affinità **stateless** (skeleton O(log n) opz.) |
| `NETLB_MAGLEV` | Maglev (tabella O(1)) | affinità ad alta scala |
| `NETLB_JIQ` | Join-Idle-Queue | idle-first O(1), fallback P2C (reti ad alta concorrenza) |

## 3. Membri (namespace `NETLB`)
| Membro | Firma (FFI) | Cosa |
|---|---|---|
| `AVAILABLE` | `() -> LONG` | 1 = lib presente |
| `POOLNEW` | `(policy, seed) -> handle` | crea il pool (handle >0, 0 err) |
| `BACKENDADD` | `(h, name, weight_milli, cap) -> id` | aggiunge un backend (−1 err) |
| `BACKENDREMOVE` | `(h, id) -> ok` | **draining** (nessun nuovo pick) |
| `BACKENDCOUNT` / `READY` | `(h) -> n` | numero backend / 1 se ≥1 sano |
| `SETPOLICY` | `(h, policy) -> ok` | cambia policy |
| `SETWEIGHT` | `(h, id, weight_milli) -> ok` | cambia peso |
| `SETCAP` | `(h, id, cap) -> ok` | max richieste in corso sul backend (0=illim.) |
| `SLOWSTART` | `(h, window_ms) -> ok` | ramp del peso sui nuovi backend (0=off) |
| `RETRYBUDGET` | `(h, ratio_milli) -> ok` | budget retry (200 = 20%) |
| `SETNOW` | `(h, now_ms) -> ok` | orologio host (slow-start) |
| `MAGLEVBUILD` | `(h, m) -> ok` | (ri)costruisce la tabella Maglev (m primo) |
| `SETSTICKY` | `(h, ttl_ms) -> ok` | sticky soft: affinità cookie→backend con TTL (0=off) |
| `SETHRWSKEL` | `(h, m, f) -> ok` | HRW gerarchico O(log n) (m cluster, f fanout; 0=off) |
| `HRWHASHES` | `(h) -> long` | n. hash dell'ultimo pick HRW (introspezione) |
| `PICK` | `(h, key) -> id` | scegle un backend |
| `PICKEXCL` | `(h, key, excl) -> id` | come PICK, esclude gli id in `excl="1,2"` |
| `BEGINREQ`/`ENDREQ` | `(h, id[, ok, lat_ms]) -> ok` | eventi richiesta |
| `PROBE` | `(h, id, ok) -> ok` | health attivo |
| `HEALTHY` | `(h, id) -> 0/1` | stato di salute |
| `SHOULDRETRY` | `(h, id, attempt) -> 0/1` | conviene ritentare? (budget) |
| `INFLIGHT`/`EWMA`/`LOAD`/`PICKCOUNT` | `(h, id) -> long` | metriche |
| `IDLECOUNT` / `STICKYCOUNT` | `(h) -> n` | backend idle (coda JIQ) / voci sticky soft |
| `RELEASE` | `(h) -> ok` | libera il pool |

## 3bis. Dettaglio dei membri (parametri)

## 4. Pattern

## 5. Limiti noti

## 6. Riferimenti

### Mvc-Http-Guida-Utente

*Fonte: `Mvc-Http-Guida-Utente`*


## Concetto

IMPORT MVC

PUBLIC CLASS UsersController INHERITS MVC.HttpController
  PUBLIC SUB Get(id AS LONG)
    ' ... lookup ...
    SELF.Response.SendJSON(utente)   ' 200 application/json
  END SUB
END CLASS


## Come funziona

## Response (helper)

## Avviare il server

## Handler HTTP nativi (`HTTP ROUTE` + `HTTPREQUEST`/`HTTPRESPONSE`)

IMPORT HTTP
DIM cfg AS HTTPCONFIG
cfg = NEW HTTPCONFIG
cfg.Port = 8786
HTTP CONFIG cfg
HTTP ROUTE "GET", "/info", InfoHandler()
HTTP SERVER 8786
' ... il server gira in GPM; per fermarlo: HTTP SERVER STOP

SUB InfoHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM w AS STRING
  w = req.Query("what")              ' parametro query string ("" se assente)
  DIM o AS JSON
  o = NEW JSON
  o("what") = w
  o("n") = 7


### `req` (HTTPREQUEST)
| Membro | Tipo | Descrizione |
|---|---|---|
| `req.Query("k")` | STRING | valore del parametro query `k` (dict dei parametri) |
| `req.QueryString` | STRING | query string grezza |
| `req.Method` / `req.Path` | STRING | metodo e path |
| `req.Body` | STRING | corpo della richiesta |
| `req.IP` | STRING | IP remoto |
| `req.Params("k")` | STRING | parametro di route `{k}` |
| `req.Headers.Get("k")` | STRING | header |
| `req.ParseMultipartForm()` | — | parse multipart |

### `res` (HTTPRESPONSE)
| Membro | Effetto |
|---|---|
| `res.ContentType = "type"` | header Content-Type |
| `res.Send(testo)` | Body = testo |
| `res.SendJSON(valore)` | Body = JSON (val/JSON/lista) |
| `res.SetStatus(codice)` | status HTTP |
| `res.SetHeader(nome, valore)` / `res.SetCookie(...)` | header/cookie |
| `res.Redirect(url)` | 302 |

### Mvc-Routing-Conventions

*Fonte: `Mvc-Routing-Conventions`*


## 1. Il controller

PUBLIC CLASS UsersController INHERITS HttpController
    PUBLIC SUB GetAll(req AS HTTPREQUEST, res AS HTTPRESPONSE)
        SELF.Json(res, users)                 ' 200 application/json
    END SUB
    PUBLIC SUB Get(req AS HTTPREQUEST, res AS HTTPRESPONSE, id AS LONG)
        IF users[key].nome <> "" THEN SELF.Json(res, users[key]) ELSE SELF.NotFound(res)
    END SUB
    PUBLIC SUB Post(req AS HTTPREQUEST, res AS HTTPRESPONSE)
        res.StatusCode = 201
        SELF.Json(res, req.Body)
    END SUB
END CLASS


## 2. Le route

HTTP ROUTE "GET", "/api/users", CtrlGetAll()
HTTP ROUTE "GET", "/api/users/{id}", CtrlGet()
SUB CtrlGet(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM id AS LONG = VAL(req.Params["id"])
  ctl.Get(req, res, id)
END SUB


## 3. Binding per convenzione nome
| Sorgente | BASIC | Esempio |
|---|---|---|
| Form url-encoded | `req.ParseForm()` → JSON | `frm.nome` |
| Body JSON | `req.BodyJson` → JSON | `body.email` |
| Path param | `req.Params["id"]` → STRING | `VAL(req.Params["id"])` |
| Query string | `req.Query["x"]` | `QUERY["x"]` (keyword) |

## 4. Validazione (esplicita — gap Z6)

DIM frm AS JSON = SELF.Form(req)
DIM errors AS JSON = NEW JSON
SELF.Required(errors, "nome", frm.nome, "Il nome è obbligatorio")
SELF.Email(errors, "email", frm.email, "Email non valida")
SELF.StringLength(errors, "nome", frm.nome, 2, 50, "2-50 caratteri")
IF SELF.HasErrors(errors) THEN
    SELF.SendValidationErrors(res, errors)      ' 400 + { "errors": { campo: msg } }
ELSE
    ' ... 201
END IF


## 5. View engine BSP (layout / section / partial)

## 6. Gotcha del linguaggio (scoperti in Fase B/C)

## 7. Gap del linguaggio (stato)

### Net-Guida-Utente

*Fonte: `Net-Guida-Utente`*


## Funzioni
| Funzione | Firma | Ritorna | Note |
|---|---|---|---|
| `NET.CONN(h)` | `CONN(handle)` | DOUBLE (1=connesso, 0=no) | Esito della connessione sul handle |
| `NET.LOCALHOST()` | `LOCALHOST()` | STRING | nome host locale (parità `Dns.GetHostName`) |
| `NET.LOCALPORT(h)` | `LOCALPORT(handle)` | DOUBLE | porta locale del socket |
| `NET.REMOTEIP(h)` | `REMOTEIP(handle)` | STRING | IP remoto del peer |
| `NET.REMOTEPORT(h)` | `REMOTEPORT(handle)` | DOUBLE | porta remota del peer |
| `NET.GETHOSTIP(host)` | `GETHOSTIP(hostname)` | STRING | risolve un nome host in IP |
| `NET.RECV(h)` | `RECV(handle)` | DOUBLE | byte disponibili in lettura (FIONREAD) |
| `NET.PEEK(h, n)` | `PEEK(handle, n)` | STRING | sbircia fino a n byte senza consumarli |

## Esempio (verificato: `shared/samples/Network/NetConnIpRecv.bas`, parity4 PASS)

IMPORT NET
DIM h AS LONG = 0
PRINT NET.CONN(h)            ' 0 (nessuna connessione)
PRINT NET.LOCALHOST()        ' nome host locale
PRINT NET.LOCALPORT(h)       ' 0
PRINT NET.PEEK(h, 1)         ' "" (niente da sbirciare)


## Determinismo

## Sorgente runtime

## Verifica

### WebDashboard-Guide

*Fonte: `WebDashboard-Guide`*


## 1. Cos'è (in parole semplici)

## 2. Prerequisiti (5 minuti)

### 2.1 GMBasic installato

git clone <url> GMBasic
cd GMBasic
dotnet build # deve dire 0 errori


### 2.2 Dove sono i file

<monorepo>/                       # GMBasicMonoRepo
├── shared/include/               # LIBRERIE DI SISTEMA IN BASIC — qui modifichi
│   ├── AgentsDashboard.inc       # helper dashboard (badge, righe tabella)
│   ├── WebCanvas.inc             # bridge Canvas → WebSocket
│   └── WasmBridge.inc            # bridge WASM → JS
├── shared/webapp/                # LA WEBAPP — qui modifichi
│   ├── app.bas                   # launcher (avvia i due server + proxy + WS)
│   ├── README.md                 # architettura, endpoint, verifica end-to-end
│   ├── pages/
│   │   ├── index.bsp             # 5 tab (Agents/Teams/Log/Canvas/WASM) — HTML+ BASIC
│   │   ├── canvas.bsp            # pagina solo Canvas (standalone)
│   │   └── wasm.bsp              # pagina solo WASM (standalone)
│   └── static/
│       ├── canvas.js             # libreria JS che disegna su <canvas>
│       └── wasm.js               # loader WASM
└── shared/samples/Ai/


### 2.3 Concetti BASIC minimi

' Un agente è un assistente con un nome
AI AGENT CREATE "Demo"
AI AGENT SET "Demo" systemPrompt "Sei un assistente utile."

' Chiedere qualcosa (sincrono, ritorna una stringa)
DIM risposta AS STRING = AI.AGENTASKSYNC("Demo", "Ciao, chi sei?")

' Lista e stato
DIM nomi() = AI.AGENTLIST()          ' array di STRING
DIM stato$ = AI.AGENTSTATUS("Demo")  ' "name=Demo model=..."

' Log (solo se TRACE attivo)
AI AGENT TRACE VERBOSE
DIM eventi() = AI.AGENTEVENTS()      ' array di STRING "[AGENT Asked] agent=Demo ..."


## 3. Avvio rapido (2 minuti)

### 3.1 Lanciare la WEBAPP completa

# dalla ROOT del monorepo — RUNTIME C (canonica, default): un solo eseguibile nativo
GMBasic/native/gmb_compiler_c/build/gmb run --project shared/webapp

# gestione completa (start/stop/status/logs/url) — default C
tools/sh/webapp_up.sh start

# alternativa C# (PCode in-process, per confronto/parità)
tools/sh/webapp_up.sh start csharp
GMBasicSharp/CompilerCli/bin/Debug/net10.0/gmbcs run --engine pcode shared/webapp/app.bas


=== GMBasic WEBAPP — Agents + Canvas + WASM ===

Seed: creo agenti demo...
Seed: 3 agenti creati (Demo, Analyst, Support)
TRACE VERBOSE attivato (eventi nel Log)
Agent API: http://localhost:8765/api/agents  (GET)
Dashboard: http://localhost:8090/index.bsp  (5 tab: Agents/Teams/Log/Canvas/WASM)
Canvas:    http://localhost:8090/canvas.bsp
WASM:      http://localhost:8090/wasm.bsp
API proxy: http://localhost:8090/api/agents  (proxato → :8765)

CTRL+C per fermare


curl http://localhost:8765/api/agents | python3 -m json.tool
# [{"name":"Demo","status":"Idle","permission":"YOLO"}, ...]

curl http://localhost:8090/api/agents | python3 -m json.tool
# stesso JSON, ma via proxy same-origin (evita CORS)

curl -X POST http://localhost:8765/api/agent/Demo/prompt \
  -H "Content-Type: application/json" \
  -d '{"prompt":"Ciao, 2+2?"}' | python3 -m json.tool
# {"agent":"Demo","answer":"4"}


### 3.1bis Gestione del processo, configurazione, selftest (v2)

tools/sh/webapp_up.sh start     # avvia in background (DEFAULT: compilatore C, `gmb run`)
tools/sh/webapp_up.sh start csharp   # alternativa: gmbcs --engine pcode
tools/sh/webapp_up.sh status    # pid + health + runtime (provider, agenti, eventi, uptime)
tools/sh/webapp_up.sh logs      # tail -f del log
tools/sh/webapp_up.sh stop      # ferma
tools/sh/webapp_up.sh restart   # stop + start

| sezione | chiave | significato |
|---|---|---|
| `[server]` | `dashboardPort` (8090) | pagine BSP + statici + proxy + WebSocket |
| | `apiPort` (8765) | `AI AGENT SERVE` |
| | `allowedIPs` (`"127.0.0.1"`) | console **locale**: solo loopback ("" = nessun filtro) |
| | `sessions` | cookie di sessione (per middleware futuri) |
| `[app]` | `seedDemo` | crea Demo/Analyst/Support se il registry è vuoto |
| | `autoTrace` | `AI AGENT TRACE VERBOSE` all'avvio (popola il Log) |
| | `refreshMs` (2500) | polling delle tab (0 = solo manuale) |
| `[paths]` | `pages`, `static` | relativi alla cartella della app |

tools/sh/webapp_selftest.sh     # HTTP: gira su webapp_up.sh → runtime C di default

# click-through della UI in un browser headless (Chromium/Obscura via CDP):
# palette, rail, Inbox, scorciatoie, tema, creazione agente dal form, filtri,
# SSE live, WebSocket canvas, zero errori JS. Exit 1 se un controllo fallisce.
bash tools/sh/gmb_c99native_run.sh ../tools/basic/webapp_ui_cdp.bas wcdp
# esito atteso: 20 PASS / 0 WARN / 0 FAIL, screenshot in /tmp/webapp_ui.png


### 3.1ter Il compilatore BSP in C è COMPLETO

### 3.1ter-bis Cosa fa il motore BSP (prestazioni e freschezza)
| funzione | comportamento | attivazione |
|---|---|---|
| **ETag + 304** | ricarica senza corpo se la pagina non è cambiata | sempre |
| **gzip** | HTML > 1 KB compresso (46 KB → ~16 KB) | sempre |
| **Hot reload** | modificare la pagina **o un `.inc` importato** la ricompila alla richiesta successiva, senza riavviare | sempre |
| **Memoizzazione resa** | la pagina non viene rieseguita per N ms | `bspCacheMs` in `.webapp.toml` (0 = off) |
| **Privacy** | niente memoizzazione per richieste con cookie | automatica |
| **Motore C** | la pagina è eseguita dalla VM/C (`libgmb_bsp`, P/Invoke): **8,8× (c=1) / 13× (c=10)** su `index.bsp`, con lo **stesso stato** del managed (agenti/team/workflow/memoria via hook `gmb_state_hook.h`) | `bspEngine = "c"` (default dell'app) · `bspEnginePages` per forzare pagine |

### 3.1quater Persistenza (agenti e team)
| cosa | file (gitignorato) | quando viene scritto |
|---|---|---|
| agente | `shared/webapp/bin/agents/<nome>.agent.json` | create/update (e `AI AGENT SAVE`) |
| team | `shared/webapp/bin/teams/<nome>.team.json` | create / add member |

### 3.1ter Endpoint PROPRI della app (oltre al proxy)
| metodo | path | cosa fa |
|---|---|---|
| GET | `/api/health` | liveness |
| GET | `/api/status` | provider/modello/agenti/eventi/uptime/porte/reachability |
| GET | `/api/state` | **stato esplicito**: `agents:[{name,state}]` (idle/working/waiting/error), `inbox:[{state,agent,kind,detail}]`, `counts` |
| POST | `/api/agents/{name}/pause` · `/resume` · `/stop` | azioni sulla **coda** (`AI.AGENTPAUSE/RESUME/STOP`); `ok:false` = niente in coda |
| POST | `/api/agents` | crea agente (`409` se esiste) |
| PATCH | `/api/agents/{name}` | aggiorna i soli campi presenti |
| DELETE | `/api/agents/{name}` | elimina |
| POST | `/api/agents/{name}/ask` | risposta **sincrona** (`AI.AGENTASKSYNC`) |
| POST | `/api/teams` | crea team (`409` se esiste) |
| POST | `/api/teams/{name}/agents` | aggiunge un membro |
| DELETE | `/api/teams/{name}` | elimina il team |

### 3.2 Lanciare i sample minimi (senza webapp)

dotnet run --project test_ilrun -- shared/samples/AI/WebAgentDashboard.bas
# Badge Idle: <span class='badge idle'>Idle</span>
# Title: GMBasic — Agents Operating Center
# [OK]

dotnet run --project test_ilrun -- shared/samples/AI/CanvasDemo.bas
# CanvasDemo — WEBCANVAS v1 → [OK]

dotnet run --project test_ilrun -- shared/samples/AI/WasmHello.bas
# WasmBridge v1 / Canvas: gmb-canvas → [OK]


## 4. Le tab — cosa vedi e come funziona (v2: 8 tab)

### 4.1 Tab Agents

DIM anames() = AI.AGENTLIST()
FOR i = 0 TO UBOUND(anames)
  DIM nm AS STRING = anames(i)
  DIM perm AS STRING = AI.AGENTGET(nm, "permission")
  PRINT DASHBOARD.AgentRowRichHtml(nm, "Idle", perm, "-", AI.AGENTGET(nm, "model"))
NEXT i


const j = await (await fetch('/api/agents')).json();
tb.innerHTML = j.map(a=>`<tr><td>${a.name}</td>...</tr>`).join('');


### 4.2 Tab Teams

### 4.3 Tab Log

### 4.4 Tab Canvas

### 4.5 Tab WASM

### 4.6 Tab Workflows

### 4.7 Tab Memory

### 4.8 Tab Brain

## 5. Come modificare — tutto in BASIC

### 5.1 Cambiare aspetto (colori, badge, layout) — `include/AgentsDashboard.inc` + `index.bsp`

PUBLIC CONST COLOR_HEADER_FROM$ = "#1e293b"  ' cambia qui
PUBLIC CONST COLOR_HEADER_TO$ = "#334155"
PUBLIC CONST COLOR_PRIMARY$ = "#2563eb"


PUBLIC FUNCTION StatusBadgeHtml(status AS STRING) AS STRING
    IF status = "Idle" OR status = "idle" THEN RETURN "<span class='badge idle'>Idle</span>"
    IF status = "Working" THEN RETURN "<span class='badge working'>Working</span>"
    ' aggiungi il tuo:
    IF status = "Busy" THEN RETURN "<span class='badge busy'>Busy</span>"
    RETURN "<span class='badge'>" + status + "</span>"
END FUNCTION


PUBLIC FUNCTION AgentRowHtml(name AS STRING, status AS STRING, permission AS STRING, teams AS STRING) AS STRING
    RETURN "<tr><td>" + name + "</td><td>" + StatusBadgeHtml(status) + "</td>..."
END FUNCTION


' in webapp/pages/index.bsp, dentro <% %> della tab Agents:
PRINT DASHBOARD.AgentRowRichHtml(nm, "Idle", perm, "-", AI.AGENTGET(nm, "model"))
' invece di AgentRowHtml


<style>
  .badge.idle { background: #e0e7ff; color: #3730a3; } /* tuo colore */
  .card { border-radius: 16px; } /* più stondato */
</style>


setInterval(()=>{refreshAgents();refreshTeams();refreshLog();}, 2000);


### 5.2 Aggiungere una colonna o un campo API

PUBLIC FUNCTION AgentRowWithTempHtml(name AS STRING, temp AS STRING) AS STRING
    RETURN "<tr><td>" + name + "</td><td>" + temp + "</td></tr>"
END FUNCTION


DIM t AS STRING = AI.AGENTGET(nm, "temperature")
PRINT DASHBOARD.AgentRowWithTempHtml(nm, t)


### 5.3 Canvas — aggiungere una forma

PUBLIC SUB Star(ws AS ANY, x AS LONG, y AS LONG, r AS LONG, col AS LONG)
    DIM j AS JSON = NEW JSON
    j["op"] = "star"
    j["v"] = PROTOCOL_VERSION%
    j["x"] = x : j["y"] = y : j["r"] = r : j["c"] = col
    ws.Send(JSON.STRINGIFY(j))
END SUB


case "star": {
  ctx.fillStyle = palette(cmd.c);
  // disegna una stella 5 punte
  const {x,y,r} = cmd;
  ctx.beginPath();
  for(let i=0;i<5;i++){ /* ... */ }
  ctx.fill();
  break;
}


WEBCANVAS.Star(ws, 100, 100, 20, 4)


### 5.4 WASM — aggiungere un import JS

' DECLARE SUB WasmStar LIB "env" ALIAS "wasm_star" (x AS LONG, y AS LONG, r AS LONG, c AS LONG)
' (decommenta quando il flag wasm32 sarà attivo; per ora è commentato per non rompere IL)


wasm_star: (x,y,r,c) => {
  const ctx = getWasmCtx(); if(!ctx) return;
  // disegna stella
},


## 6. API HTTP (per chi vuole chiamare da curl o JS)
| Metodo | Path | Body | Risposta | Note |
|---|---|---|---|---|
| `GET` | `/api/agents` | — | `[{"name":"Demo","status":"Idle","permission":"YOLO"}]` | via `AgentStatusManager.GetAgentsJson()` |
| `GET` | `/api/teams` | — | `[{"name":"Squadra","strategy":"Sequential","memberCount":2}]` | — |
| `POST` | `/api/agent/{name}/prompt` | `{"prompt":"ciao"}` | `{"agent":"Demo","answer":"..."}` | chiama `AgentServer.Ask` |
| `GET` | `/api/events` | — | `[{"detail":"[AGENT Asked] ..."}]` | proxy BASIC fallback `AI.AGENTEVENTS()` se `GET /api/events` non esiste ancora su 8765 |
| `GET` | `/ws/canvas` | WS upgrade | — | WebSocket per Canvas, vedi `canvas.js` |

curl http://localhost:8090/api/agents | python3 -m json.tool
curl -X POST http://localhost:8090/api/agent/Demo/prompt \
  -H "Content-Type: application/json" \
  -d '{"prompt":"Racconta una barzelletta"}' | python3 -m json.tool


## 7. Struttura file (per orientarsi)

include/AgentsDashboard.inc  — 115 righe, NAMESPACE DASHBOARD, costanti + 10 helper
include/WebCanvas.inc        — 80 righe, NAMESPACE WEBCANVAS, 8 primitive + Frame
include/WasmBridge.inc       — 40 righe, NAMESPACE WASMBRIDGE, 3 stub + Info()
webapp/app.bas               — 185 righe, launcher + 4 proxy + WS handler
webapp/pages/index.bsp       — 356 righe, 5 tab HTML+ BASIC+JS
webapp/pages/canvas.bsp      — 60 righe standalone
webapp/pages/wasm.bsp        — 70 righe standalone
webapp/static/canvas.js      — 80 righe replay + palette
webapp/static/wasm.js        — 60 righe loader + importObject
shared/samples/AI/WebAgentDashboard.bas — 60 righe smoke
shared/samples/AI/CanvasDemo.bas        — 60 righe smoke WS
shared/samples/AI/WasmHello.bas         — 30 righe smoke WASM


## 8. Troubleshooting (errori comuni)
| Errore | Causa | Fix |
|---|---|---|
| `ERRORE: Expected an expression atom` su `AI AGENT SET` | Hai usato le virgole: `AI AGENT SET Demo, systemPrompt, "ciao"` | Togli le virgole: `AI AGENT SET "Demo" systemPrompt "ciao"` (forma senza virgole dal 2026-09-06) |
| `Agent '0' is not registered` | Hai scritto `AI AGENT SET Demo ...` senza virgolette sul nome — `Demo` è visto come variabile (valore 0) | Metti le virgolette: `AI AGENT SET "Demo" ...` |
| `LCASE` non definito | Hai usato `LCASE$(x)` — non esiste (usa `LCase`/`LOWER` non portabili) | Sostituisci con `IF x="Idle" OR x="idle"` come in `AgentsDashboard.inc` |
| `Label not marked` su `RETURN` in `SUB` | `IF cond THEN RETURN` dentro una `SUB` senza `END IF` | Usa `IF cond THEN ... END IF` senza `RETURN`, o `IF ... THEN ... ELSE ... END IF` |
| `StatusCode` su `ANY` | `DIM r = FETCH(...)` senza tipo → `r` è `ANY`, non ha `.StatusCode` | Dichiara `DIM r AS FETCHRESULT = FETCH(...)` |
| `FOR i%` → `no FOR active` | Loop con `%` suffix e `NEXT i%` | Usa `FOR idx = 0 TO ...` + `NEXT idx` senza `%`, o `FOR i AS LONG` |
| `InvalidProgram` su `WasmBridge.Info()` | `Info()` con `LTRIM$(STR$(%))` + costanti `$/%` | Semplifica `Info()` a `RETURN "WasmBridge v1"` (già fatto) |
| `PROTOCOL_VERSION` → 0 | Hai letto `WEBCANVAS.PROTOCOL_VERSION` senza `%` | Usa con suffisso: `WEBCANVAS.PROTOCOL_VERSION%` |
| `gmb_vm_host non trovato` | Hai lanciato con VM/C senza buildare il runtime | `cmake -S GMBasic/native/gmb_vm_c -B GMBasic/native/gmb_vm_c/build && cmake --build GMBasic/native/gmb_vm_c/build` oppure usa `test_ilrun` (IL, non serve VM/C) |
| `404` su `/api/agents` dal browser | Stai chiamando `fetch("http://127.0.0.1:8765/api/agents")` cross-port | Usa `fetch("/api/agents")` same-origin (proxato da `app.bas` su 8090) |
| Canvas non disegna | Hai cambiato `WebCanvas.inc` ma non `canvas.js` (o viceversa) | Cambia entrambi: JSON `{op:"star"}` deve avere case in `canvas.js` |
| WASM `404 /static/prog.wasm` | Non hai ancora compilato il `.wasm` | Normale in V1 — clicca **Demo draw (JS fallback)** o compila: `clang --target=wasm32 -o webapp/static/prog.wasm /tmp/prog.c ...` |

## 9. Prossimi passi (V1.1, opzionali, 30 righe C#)

## 10. Riferimenti

## 11. Aggiornamento V1.1 — 2026-09-08 09:00 (shim C# 30 righe)

# 1. Avvia la webapp come prima
dotnet run --project REPL -- webapp/app.bas &
# 2. Curl arricchito
curl http://localhost:8765/api/agents | python3 -m json.tool
# ora vedi model/teams/toolCount/hasKeypair
curl http://localhost:8765/api/events | python3 -m json.tool
# ora vedi timestamp/kind/agent/detail strutturati
# 3. WASM flag
dotnet run --project REPL -- shared/samples/AI/WasmHello.bas --target wasm
# hint + esecuzione IL


## 12. Aggiornamento V1.2 — 2026-09-08 09:30 (team detail/broadcast + sink)

# Team detail (dopo aver creato un team, es. TEAM CREATE Squadra)
curl http://localhost:8767/api/teams/Squadra | python3 -m json.tool
curl -X POST http://localhost:8767/api/team/Squadra/broadcast \
  -H "Content-Type: application/json" -d '{"message":"hello"}' | python3 -m json.tool


## 13. Aggiornamento V1.2e–g — 2026-09-08 11:20 (workflows CRUD + Memory/Summary, 40 righe C#)

<!-- END SURFACE -->








<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **HTTP.DELETE(x1)** — `PRINT "["; HTTP.DELETE("http://127.0.0.1:8197/del"); "]"` (shared/minitests/http/roundtrip.bas:31)
- **HTTP.DETECTCONTENTTYPE(x1)** — `PRINT "type: "; HTTP.DETECTCONTENTTYPE("a")   ' nessun magic -> "" (stringa non binaria)` (shared/samples/Security/UploadValidationDemo.bas:24)
- **HTTP.FETCH(x1)** — `70 DO WHILE SQL.FETCH(rs)` (shared/samples/Databases/SqlMinimalCrud.bas:15)
- **HTTP.GET(x1)** — `PRINT "http.get: "; LEFT(httpCall$, 300)` (shared/samples/Connectors/ConnectorDatabaseHttp.bas:40)
- **HTTP.POST(x1)** — `PRINT "http.post: "; LEFT(httpPost$, 300)` (shared/samples/Connectors/ConnectorDatabaseHttp.bas:43)
- **HTTP.PUT(x1)** — `PRINT "["; HTTP.PUT("http://127.0.0.1:8197/echo", "xyz"); "]"` (shared/minitests/http/roundtrip.bas:30)
- **HTTP.SAFEFILENAME(x1)** — `PRINT "safe: "; HTTP.SAFEFILENAME("../../etc/passwd.png")` (shared/samples/Security/UploadValidationDemo.bas:14)
- **HTTP.SESSIONCREATE(x1, x2)** — `DIM cookie$ = HTTP.SESSIONCREATE(user$, "admin")` (shared/samples/Security/ServerAuthDemo.bas:32)
- **NET.CONN(x1)** — `PRINT "NET.CONN(0):       "; NET.CONN(h)` (shared/samples/Network/NetConnIpRecv.bas:8)
- **NET.GETHOSTIP(x1)** — `PRINT "NET.GETHOSTIP(127.0.0.1):  "; NET.GETHOSTIP("127.0.0.1")` (shared/samples/Network/NetConnIpRecv.bas:13)
- **NET.LOCALHOST()** — `PRINT "LOCALHOST len: "; LEN(NET.LOCALHOST())` (shared/samples/Network/NetworkFullCoverage.bas:20)
- **NET.LOCALPORT(x1)** — `PRINT "LOCALPORT: "; NET.LOCALPORT(0)` (shared/samples/Network/NetworkFullCoverage.bas:18)
- **NET.PEEK(x1)** — `PRINT "PEEK len (handle invalido): "; LEN(NET.PEEK(0, 16))` (shared/samples/Network/NetworkFullCoverage.bas:24)
- **NET.RECV(x1)** — `PRINT "NET.RECV(0):       "; NET.RECV(h)` (shared/samples/Network/NetConnIpRecv.bas:14)
- **NET.REMOTEIP(x1)** — `PRINT "REMOTEIP len: "; LEN(NET.REMOTEIP(0))` (shared/samples/Network/NetworkFullCoverage.bas:14)
- **NET.REMOTEPORT(x1)** — `PRINT "REMOTEPORT: "; NET.REMOTEPORT(0)` (shared/samples/Network/NetworkFullCoverage.bas:16)
<!-- END DOMAIN TABLE -->
