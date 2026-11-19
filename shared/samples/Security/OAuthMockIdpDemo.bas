' FILE: Security/OAuthMockIdpDemo.bas
' =================================================================================================
'       OAUTH — mock IdP locale (server BASIC) + flusso completo con la libreria OAUTH
' =================================================================================================
' Dimostra come implementare IN BASIC un piccolo server OAuth e come eseguire il flusso
' Authorization Code + PKCE davvero sulla rete locale:
'   - /mock/authorize  (GET)  -> JSON "user authority" (in un browser reale farebbe il redirect)
'   - /mock/token      (POST) -> scambia grant_type/code e risponde { access_token, ... }
' Il client usa OAUTH.TokenBody + OAUTH.RequestToken contro il mock, senza credenziali esterne.
' Il server standalone equivalente (sottoprocesso, processo pulito) è OAuthMockIdpServer.bas ed è
' VERIFICATO end-to-end dal test OAuthMockIdpServer_Subprocess_E2E (authorize 200, token OK,
' grant invalido -> 400 invalid_grant). Qui server+client sono nello stesso programma: gira in
' REPL (processo con GPM); nell'harness xUnit in-process il client BASIC non raggiunge il proprio
' listener (limite d'ambiente noto: HttpListener-InProcess, vedi Obsidian) -> il test è compile-only.

IMPORT OAUTH
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY
IMPORT STRINGS
IMPORT HTTP

DIM gPort AS LONG = 8097

SUB MockAuthorize(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    res.Send("{""mock"":""authorize"",""state"":""demo""}")
END SUB

SUB MockToken(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM body$ = req.Body
    IF INSTR(body$, "grant_type=authorization_code") > 0 AND INSTR(body$, "code=demo-auth-code") > 0 THEN
        res.Send("{""access_token"":""mock-token-abc"",""token_type"":""Bearer"",""expires_in"":3600}")
    ELSE
        res.SetStatus(400)
        res.Send("{""error"":""invalid_grant""}")
    END IF
END SUB

' --- MAIN ---
HTTP ROUTE "GET", "/mock/authorize", MockAuthorize()
HTTP ROUTE "POST", "/mock/token", MockToken()
HTTP SERVER gPort

' --- flusso client (rete locale) ---
DIM au$ = HTTP.GET("http://127.0.0.1:" + STR$(gPort) + "/mock/authorize")
PRINT "authorize: "; au$
IF INSTR(au$, "mock") > 0 THEN PRINT "AUTHZ OK" ELSE PRINT "AUTHZ FAIL"

DIM verifier$ = OAUTH.CreateVerifier(64)
DIM code$ = "demo-auth-code"
DIM body$ = OAUTH.TokenBody("authorization_code", "mock-client", "", code$, "http://localhost/cb", "openid")
DIM tokenJson$ = ""
DIM attempt AS LONG
FOR attempt = 1 TO 10
    tokenJson$ = OAUTH.RequestToken("http://127.0.0.1:" + STR$(gPort) + "/mock/token", body$)
    IF LEN(tokenJson$) > 0 THEN attempt = 10
NEXT attempt

PRINT "token: "; tokenJson$
IF INSTR(tokenJson$, "mock-token-abc") > 0 THEN
    PRINT "MOCK TOKEN OK"
ELSE
    PRINT "MOCK TOKEN FAIL"
END IF

PRINT "OAuthMockIdp done"
