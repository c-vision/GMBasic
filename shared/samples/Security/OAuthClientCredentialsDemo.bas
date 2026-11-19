' FILE: Security/OAuthClientCredentialsDemo.bas
' =================================================================================================
'       OAUTH — Client Credentials (uso corretto della libreria OAUTH)
' =================================================================================================
' Machine-to-machine: nessun browser. Si costruisce il body con OAUTH.TokenBody e si chiama
' OAUTH.RequestToken(endpoint, body). In questo demo il body e l'endpoint vengono verificati
' senza rete (il token request reale è un commento).

IMPORT OAUTH
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY
IMPORT STRINGS
IMPORT HTTP

PRINT "=== OAuth Client Credentials ==="

DIM ep$ = OAUTH.TokenEndpoint("google")
DIM body$ = OAUTH.TokenBody("client_credentials", "svc-client-id", "svc-secret", "", "", "https://www.googleapis.com/auth/cloud-platform")

PRINT "endpoint: "; ep$
PRINT "body:     "; body$

IF LEN(ep$) > 0 THEN
    PRINT "ENDPOINT OK"
ELSE
    PRINT "ENDPOINT FAIL"
END IF
IF INSTR(body$, "grant_type=client_credentials") > 0 AND INSTR(body$, "client_secret=svc-secret") > 0 THEN
    PRINT "CC BODY OK"
ELSE
    PRINT "CC BODY FAIL"
END IF

' il request reale (rete) sarebbe:
'   DIM json$ = OAUTH.RequestToken(ep$, body$)   ' poi JSON.PARSE(json$) per access_token/expires_in

PRINT "OAuthClientCredentials done"
