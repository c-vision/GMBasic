' FILE: Security/OAuthPKCEFlowDemo.bas
' =================================================================================================
'       OAUTH — flusso Authorization Code + PKCE (uso corretto della libreria OAUTH)
' =================================================================================================
' Mostra l'uso corretto di include/OAuthClient.inc per un'app server GMBasic:
'   1) /login  -> genera verifier+state, costruisce l'URL di autorizzazione (PKCE S256) e
'                 redirige l'utente all'IdP (preset google in questo demo).
'   2) callback -> l'IdP torna con ?code=...&state=...; si scambia il code con TokenBody +
'                 RequestToken (in questo demo il body viene costruito e stampato, senza rete).
' Eseguibile su IL, PCodeVM e VM/C.

IMPORT OAUTH
IMPORT CRYPTOGRAPHY
IMPORT STRINGS
IMPORT HTTP

PRINT "=== OAuth Authorization Code + PKCE ==="

' --- 1. avvio del login ---
DIM verifier$ = OAUTH.CreateVerifier(64)   ' salvato in sessione per la verifica al callback
DIM state$ = "csrf-state-123"
DIM authUrl$ = OAUTH.AuthUrl(OAUTH.AuthorizeEndpoint("google"), "google-client-id",
    "http://localhost:8095/cb", "openid profile email", state$, verifier$)

PRINT "authUrl len: "; LEN(authUrl$)
IF INSTR(authUrl$, "accounts.google.com") > 0 AND INSTR(authUrl$, "code_challenge=") > 0 THEN
    PRINT "AUTH URL OK"
ELSE
    PRINT "AUTH URL FAIL"
END IF
IF INSTR(authUrl$, "state=csrf-state-123") > 0 THEN
    PRINT "STATE OK"
ELSE
    PRINT "STATE FAIL"
END IF

' --- 2. callback: il provider ci rimanda ?code=...&state=... ---
DIM code$ = "auth-code-4f3a"    ' nella realtà arriva dalla query string del callback
DIM challenge$ = OAUTH.CreateChallenge(verifier$)
DIM tokenBody$ = OAUTH.TokenBody("authorization_code", "google-client-id", "",
    code$, "http://localhost:8095/cb", "openid profile email")

PRINT "challenge len: "; LEN(challenge$)
IF INSTR(tokenBody$, "grant_type=authorization_code") > 0 AND INSTR(tokenBody$, "code=auth-code-4f3a") > 0 THEN
    PRINT "CALLBACK TOKEN BODY OK"
ELSE
    PRINT "CALLBACK TOKEN BODY FAIL"
END IF

' nota: lo scambio vero è OAUTH.RequestToken(OAUTH.TokenEndpoint("google"), tokenBody$)
' (richiede rete); il body qui sopra è pronto per JSON.PARSE della risposta.

PRINT "OAuthPKCEFlow done"
