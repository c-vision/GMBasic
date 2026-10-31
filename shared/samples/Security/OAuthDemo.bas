' FILE: Security/OAuthDemo.bas
' =================================================================================================
'       OAUTH — client Authorization Code + PKCE / Client Credentials (libreria di sistema)
' =================================================================================================
' Demo della libreria include/OAuthClient.inc (IMPORT OAUTH). Genera l'URL di autorizzazione
' con PKCE S256 e il body del token request. Eseguibile su IL, PCodeVM e VM/C.

IMPORT OAUTH
IMPORT CRYPTOGRAPHY
IMPORT STRINGS
IMPORT HTTP

PRINT "=== OAuth client ==="

' Verifier deterministico (>=43 char) per un test riproducibile
DIM v$ = "verifier-test-0123456789-0123456789-0123456789-0123456789"
DIM challenge$ = OAUTH.CreateChallenge(v$)
PRINT "challenge len: "; LEN(challenge$)

DIM url$ = OAUTH.AuthUrl("https://login.example.com/authorize", "client-123", "http://localhost:8080/cb", "openid profile", "abc123", v$)
PRINT "url len: "; LEN(url$)
IF INSTR(url$, "response_type=code") > 0 AND INSTR(url$, "client_id=client-123") > 0 THEN
    PRINT "AUTHURL OK"
ELSE
    PRINT "AUTHURL FAIL"
END IF
IF INSTR(url$, "code_challenge=") > 0 AND INSTR(url$, "code_challenge_method=S256") > 0 THEN
    PRINT "PKCE OK"
ELSE
    PRINT "PKCE FAIL"
END IF

DIM body$ = OAUTH.TokenBody("client_credentials", "client-123", "s3cret", "", "", "api")
PRINT "cc body: "; body$
IF INSTR(body$, "grant_type=client_credentials") > 0 THEN
    PRINT "CC OK"
ELSE
    PRINT "CC FAIL"
END IF

' Verifier random (range RFC 7636 43-128)
DIM rv$ = OAUTH.CreateVerifier(43)
IF LEN(rv$) = 43 THEN
    PRINT "VERIFIER OK"
ELSE
    PRINT "VERIFIER FAIL"
END IF

' Presets IdP
IF OAUTH.Issuer("azure") = "https://login.microsoftonline.com/common/v2.0" THEN
    PRINT "ISSUER AZURE OK"
ELSE
    PRINT "ISSUER AZURE FAIL"
END IF
IF OAUTH.AuthorizeEndpoint("google") = "https://accounts.google.com/o/oauth2/v2/auth" THEN
    PRINT "EP GOOGLE OK"
ELSE
    PRINT "EP GOOGLE FAIL"
END IF

PRINT "OAuthDemo done"
