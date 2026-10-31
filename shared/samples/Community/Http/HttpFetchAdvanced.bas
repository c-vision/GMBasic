' FILE: Http/HttpFetchAdvanced.bas
' ==============================================================================
' GUIDA TEORICA: FETCH avanzato — COOKIEJAR, simulazione browser, TLS pinning
' ==============================================================================
' Un solo server HTTPS locale (self-signed, certificato di test già presente in
' Network/server.pfx) serve tutti e tre i test — un solo HTTP SERVER attivo per
' processo, la stessa istanza copre l'intero sample.
'
' 1. COOKIEJAR — un contenitore di cookie indipendente (DIM j AS NEW COOKIEJAR),
'    assegnabile a opts.CookieJar, per mantenere sessioni multiple isolate
'    nello stesso processo (es. simulare più utenti/tab di un browser in
'    parallelo). Ogni jar mantiene i propri cookie fra richieste successive
'    che lo usano; jar diversi non si vedono a vicenda. j.Clear() rimuove
'    tutti i cookie memorizzati in quel jar.
'
' 2. opts.BrowserPreset ("chrome"|"firefox"|"safari") — invia lo stesso set
'    di header (User-Agent, Accept, Sec-Fetch-*, ecc., nell'ordine giusto)
'    che invierebbe davvero quel browser per una richiesta di navigazione.
'    Utile per servizi esterni che si comportano diversamente verso un
'    client "senza browser" percepito. Gli header espliciti impostati a
'    mano hanno sempre la precedenza sul preset per lo stesso nome.
'
' 3. opts.PinnedCertificateSha256 — certificate pinning: la richiesta
'    HTTPS riesce SOLO se il certificato del server ha esattamente questa
'    impronta SHA-256 (esadecimale, con o senza ':'), indipendentemente
'    dalla catena di fiducia del sistema — funziona anche con un
'    certificato self-signed, come quello usato qui. Indipendente da
'    AllowInsecureSSL: un pin esplicito È la decisione di fiducia (i test 1 e
'    2 sopra usano invece AllowInsecureSSL, perché non stanno verificando il
'    certificato — solo cookie e header).
' ==============================================================================

IMPORT CONCURRENCY
IMPORT JSON
IMPORT HTTP

SUB InfoHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM sess = req.SessionId
  IF sess = "" THEN
    ' LTRIM$ perche' STR$() su un intero positivo include sempre uno spazio iniziale per il
    ' segno (convenzione BASIC) — un cookie-value con uno spazio letterale non e' RFC 6265
    ' cookie-octet valido: System.Net.CookieContainer (usato da COOKIEJAR) lo normalizza
    ' silenziosamente alla memorizzazione, cosi' il valore letto da req.SessionId in una
    ' richiesta successiva non e' piu' uguale, byte per byte, a quello scritto qui — un
    ' sessionId risultava quindi SEMPRE "diverso" da se stesso al confronto con l'operatore =.
    sess = LTRIM$(STR$(INT(RND() * 1000000)))
    res.SetCookie("GM_BASIC_SESSION", sess)
  END IF
  DIM j AS JSON = NEW JSON
  j.sessionId = sess
  ' NOTA: req.Headers.UserAgent (proprietà tipizzata) NON funziona lato server —
  ' req.Headers è un oggetto JSON generico (JsonWrapper), non lo stesso
  ' CommonLib.Http.HttpHeaders tipizzato usato lato client per HTTPOPTIONS.Headers;
  ' l'accesso per proprietà cerca la chiave letterale "UserAgent" (senza trattino),
  ' che non esiste mai nell'header reale "User-Agent" — nessun errore, solo un
  ' risultato sempre vuoto. La via corretta, già cablata per questo scopo
  ' (nomi header con trattino non sono identificatori BASIC validi), è
  ' .Get(nome_letterale_header):
  j.userAgent = req.Headers.Get("User-Agent")
  res.SendJSON(j)
END SUB

' Certificato di test già presente nei sample Network/ (server.pfx, password
' "test", CN=localhost, valido fino al 2027) — impronta SHA-256 calcolata con
' `openssl pkcs12 -in server.pfx -clcerts -nokeys -passin pass:test |
'  openssl x509 -noout -fingerprint -sha256`.
DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
cfg.Port = 8103
cfg.CertificatePath = "../Network/server.pfx"
cfg.CertificatePassword = "test"
HTTP CONFIG cfg
HTTP ROUTE "GET", "/info", InfoHandler()
PRINT "Server HTTPS sulla porta 8103"
HTTP SERVER 8103

DIM wg AS WAITGROUP
GO SUB ClientTest(wg) IN wg
WAIT wg
HTTP SERVER STOP

SUB ClientTest(wg AS WAITGROUP)
  SLEEP 300

  ' --- 1. COOKIEJAR: due jar isolati + verifica che Clear() funzioni ---
  PRINT ""
  PRINT "--- COOKIEJAR ---"
  DIM jarA AS COOKIEJAR = NEW COOKIEJAR
  DIM jarB AS COOKIEJAR = NEW COOKIEJAR

  DIM optsA AS HTTPOPTIONS = NEW HTTPOPTIONS
  optsA.AllowInsecureSSL = TRUE
  optsA.CookieJar = jarA
  DIM r1 AS FETCHRESULT = FETCH("https://127.0.0.1:8103/info", optsA)
  DIM j1 AS JSON = JSON.PARSE(r1.Body)

  DIM r2 AS FETCHRESULT = FETCH("https://127.0.0.1:8103/info", optsA)
  DIM j2 AS JSON = JSON.PARSE(r2.Body)
  ' sessionId e' un numero casuale (STR$(INT(RND() * 1000000)), vedi InfoHandler) — non stampato
  ' direttamente: renderebbe l'output non deterministico run su run, facendo fallire il confronto
  ' testo-contro-testo di test_pcode anche a fronte di un comportamento corretto. Si stampa invece
  ' solo l'ESITO del confronto che il test vuole davvero dimostrare.
  PRINT "  jarA, 2a richiesta == 1a (il jar mantiene il cookie): "; (j2.sessionId = j1.sessionId)

  DIM optsB AS HTTPOPTIONS = NEW HTTPOPTIONS
  optsB.AllowInsecureSSL = TRUE
  optsB.CookieJar = jarB
  DIM r3 AS FETCHRESULT = FETCH("https://127.0.0.1:8103/info", optsB)
  DIM j3 AS JSON = JSON.PARSE(r3.Body)
  PRINT "  jarB, 1a richiesta <> jarA (jar isolati): "; (j3.sessionId <> j1.sessionId)

  jarA.Clear()
  DIM r4 AS FETCHRESULT = FETCH("https://127.0.0.1:8103/info", optsA)
  DIM j4 AS JSON = JSON.PARSE(r4.Body)
  PRINT "  jarA dopo Clear() <> jarA originale (Clear() ha svuotato il jar): "; (j4.sessionId <> j1.sessionId)

  ' --- 2. BrowserPreset: lo stesso client, User-Agent diverso per preset ---
  PRINT ""
  PRINT "--- BrowserPreset ---"
  DIM presets(2) AS STRING
  presets(0) = "chrome"
  presets(1) = "firefox"
  presets(2) = "safari"
  DIM i AS INTEGER
  FOR i = 0 TO 2
    DIM optsP AS HTTPOPTIONS = NEW HTTPOPTIONS
    optsP.AllowInsecureSSL = TRUE
    optsP.BrowserPreset = presets(i)
    DIM rp AS FETCHRESULT = FETCH("https://127.0.0.1:8103/info", optsP)
    DIM jp AS JSON = JSON.PARSE(rp.Body)
    PRINT "  preset="; presets(i); " -> User-Agent ricevuto dal server: "; jp.userAgent
  NEXT i

  ' --- 3. PinnedCertificateSha256: pin corretto passa, pin sbagliato fallisce ---
  PRINT ""
  PRINT "--- PinnedCertificateSha256 ---"
  DIM optsPinOk AS HTTPOPTIONS = NEW HTTPOPTIONS
  optsPinOk.PinnedCertificateSha256 = "07:02:1B:DA:C9:03:81:81:AF:0D:F6:8A:00:83:53:D9:5A:F3:6F:F1:A2:31:E8:62:EA:BB:A4:81:48:6E:54:75"
  DIM rOk AS FETCHRESULT = FETCH("https://127.0.0.1:8103/info", optsPinOk)
  PRINT "  Pin CORRETTO -> IsSuccess="; rOk.IsSuccess; " StatusCode="; rOk.StatusCode; " (atteso: successo, nonostante il certificato sia self-signed e AllowInsecureSSL NON sia impostato)"

  ' Un rifiuto TLS (come una qualunque altra rottura a livello di connessione —
  ' timeout, DNS, pin sbagliato) nella FORMA-FUNZIONE `DIM r AS FETCHRESULT =
  ' FETCH(...)` NON viene rilanciata come eccezione BASIC: FetchFunction cattura
  ' l'errore di rete e restituisce un FETCHRESULT con StatusCode=0 / IsSuccess=0
  ' (stesso comportamento su IL e PCodeVM). ON ERROR serve solo per errori di
  ' programmazione/compile: qui il rifiuto si verifica controllando IsSuccess.
  DIM optsPinBad AS HTTPOPTIONS = NEW HTTPOPTIONS
  optsPinBad.PinnedCertificateSha256 = "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00"
  DIM rBad AS FETCHRESULT = FETCH("https://127.0.0.1:8103/info", optsPinBad)
  IF rBad.IsSuccess = 0 THEN
    PRINT "  Pin SBAGLIATO -> connessione rifiutata come atteso (IsSuccess="; rBad.IsSuccess; " StatusCode="; rBad.StatusCode; ")"
  ELSE
    PRINT "  ERRORE nel test: la richiesta con pin sbagliato non avrebbe dovuto avere successo (IsSuccess="; rBad.IsSuccess; ")"
  END IF

  PRINT ""
  PRINT "--- Test completati ---"
  DONE wg
END SUB
