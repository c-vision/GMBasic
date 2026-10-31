' FILE: Security/JwtRouteDemo.bas
' =================================================================================================
'       SECURITY — route protetta con JWT (E2E: server + client FETCH nello stesso file)
' =================================================================================================
' Pattern: una route di login emette un token HS256 (AUTH.JWTCREATE); una route protetta
' legge l'header Authorization: Bearer <token>, verifica la firma (AUTH.JWTVERIFY) e restituisce
' i claims (AUTH.JWTPAYLOAD) solo se il token è valido.
'
' E2E (client FETCH in goroutine, come HttpSession.bas):
'   GET /api/login            -> 200, body = token
'   GET /api/protected (senza Bearer)   -> 401
'   GET /api/protected (con Bearer)     -> 200, JSON { protected=true, claims=... }
'
' NB: la scrittura della sessione da handler route NON è esposta sul tipo HTTPREQUEST
' (vedi vault Bugs/(open) - JwtRouteDemo-Non-Compila-Req-Session): per lo stato di login
' lato server usare HTTP.SESSIONCREATE (pattern in ServerAuthDemo.bas). Qui l'autorizzazione
' è stateless via Bearer token.
' Eseguibile su IL e PCodeVM.

IMPORT HTTP
IMPORT AUTH
IMPORT CONCURRENCY

SUB ProtectedRoute(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    ' estrae "Bearer <token>" dall'header Authorization
    DIM auth$ = req.Headers.Get("Authorization")
    DIM token$ = ""
    IF LEN(auth$) > 7 THEN
        IF LEFT(auth$, 7) = "Bearer " THEN token$ = MID(auth$, 8, LEN(auth$) - 7)
    END IF

    IF token$ = "" OR AUTH.JWTVERIFY(token$, secret$) = FALSE THEN
        res.SetStatus(401)
        res.Send("Unauthorized")
        RETURN
    END IF

    DIM claims$ = AUTH.JWTPAYLOAD(token$)
    res.SendJSON({ protected = TRUE, claims = claims$ })
END SUB

SUB LoginRoute(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    ' emette un token HS256 con i claims (login fittizio)
    DIM q$ = CHR$(34)
    DIM claims$ = "{" + q$ + "sub" + q$ + ":" + q$ + "alice" + q$ + "," + q$ + "role" + q$ + ":" + q$ + "admin" + q$ + "}"
    DIM token$ = AUTH.JWTCREATE(claims$, secret$)
    res.Send(token$)
END SUB

' MAIN
DIM secret$ = "demo-secret"
DIM port AS LONG = 18085

HTTP ROUTE "GET", "/api/login", LoginRoute()
HTTP ROUTE "GET", "/api/protected", ProtectedRoute()

HTTP SERVER port

DIM wg AS WAITGROUP
GO SUB ClientTest(wg, port) IN wg
WAIT wg
HTTP SERVER STOP

SUB ClientTest(wg AS WAITGROUP, p AS LONG)
    SLEEP 300
    DIM base$ = "http://127.0.0.1:" + LTRIM$(STR$(p))
    DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS
    opts.Headers.Connection = "close"

    ' 1) login -> token
    DIM loginRes AS FETCHRESULT = FETCH(base$ + "/api/login", opts)
    PRINT "[CLIENT] login status="; loginRes.StatusCode
    DIM token$ = loginRes.Body

    ' 2) route protetta SENZA Bearer -> 401
    DIM noAuthRes AS FETCHRESULT = FETCH(base$ + "/api/protected", opts)
    PRINT "[CLIENT] no-bearer status="; noAuthRes.StatusCode

    ' 3) route protetta CON Bearer -> 200 + claims
    DIM opts2 AS HTTPOPTIONS = NEW HTTPOPTIONS
    opts2.Headers.Connection = "close"
    opts2.Headers.Authorization = "Bearer " + token$
    DIM okRes AS FETCHRESULT = FETCH(base$ + "/api/protected", opts2)
    PRINT "[CLIENT] bearer status="; okRes.StatusCode
    PRINT "[CLIENT] bearer body="; okRes.Body

    IF loginRes.StatusCode = 200 AND noAuthRes.StatusCode = 401 AND okRes.StatusCode = 200 THEN
        PRINT "JWT-ROUTE-E2E-OK"
    ELSE
        PRINT "JWT-ROUTE-E2E-FAIL login="; loginRes.StatusCode; " noauth="; noAuthRes.StatusCode; " auth="; okRes.StatusCode
    END IF
END SUB
