' FILE: Security/OAuthMockIdpServer.bas
' =================================================================================================
'       OAUTH — mock IdP: server OAuth standalone (implementazione BASIC di un IdP)
' =================================================================================================
' Mini authorization server OAuth implementato IN BASIC:
'   - /mock/authorize  (GET)  -> risposta "user authority" (qui JSON dimostrativo; in un IdP
'                                reale: 302 verso il redirect_uri con ?code=...&state=...)
'   - /mock/token      (POST) -> valida grant_type=authorization_code + code e risponde
'                                { access_token, token_type, expires_in } (RFC 6749 §4.1.3);
'                                grant non valido -> 400 { "error": "invalid_grant" }
' Resta attivo 20 s poi si ferma (SLEEP + HTTP SERVER STOP); utile per test e2e e per il
' client BASIC (OAuthMockIdpDemo.bas in REPL). Porta: 8097.

IMPORT HTTP
IMPORT STRINGS

PRINT "MOCK IDP READY"

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

HTTP ROUTE "GET", "/mock/authorize", MockAuthorize()
HTTP ROUTE "POST", "/mock/token", MockToken()
HTTP SERVER 8097

SLEEP 20000
HTTP SERVER STOP
PRINT "MOCK IDP STOPPED"
