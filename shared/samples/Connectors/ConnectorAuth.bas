' FILE: Connectors/ConnectorAuth.bas
' OAuth2 + PKCE per connettore (MCP spec 2026-07-28, SYSTEM.txt §28-29)
' Flusso: CreateVerifier -> CreateChallenge -> BuildAuthUrl -> RequestToken -> Stored Bearer

IMPORT AI
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY
IMPORT STRINGS
IMPORT HTTP
IMPORT OAUTH

SUB Main()
    PRINT "=== OAuth per Connettore ==="

    ' 1. Registra connettori con auth oauth2 (dal marketplace)
    AI CONNECTOR LOAD "gmail"
    AI CONNECTOR LOAD "github"
    PRINT "LOAD gmail+github OK"

    ' 2. PKCE: verifier 64 char + challenge S256
    DIM verifier$ AS STRING = OAUTH.CreateVerifier(64)
    PRINT "verifier len="; LEN(verifier$)
    DIM challenge$ AS STRING = OAUTH.CreateChallenge(verifier$)
    PRINT "challenge="; LEFT(challenge$, 60)

    ' 3. Auth URL (Authorization Code + PKCE)
    DIM authUrl$ AS STRING = OAUTH.AuthUrl("https://accounts.google.com/o/oauth2/v2/auth", "client123", "http://localhost:8765/callback", "https://www.googleapis.com/auth/gmail.readonly", "stateXYZ", verifier$)
    PRINT "authUrl: "; LEFT(authUrl$, 300)

    ' 4. Salva secret OAuth sul connettore (simula callback con code -> token)
    ' In produzione: code da redirect + OAUTH.RequestToken(tokenEndpoint, body)
    AI CONNECTOR AUTH "gmail", "{""access_token"":""ya29.test_gmail"",""refresh_token"":""1//refresh_gmail"",""token_type"":""Bearer"",""expires_in"":3600,""scope"":""gmail.readonly""}"
    PRINT "AUTH gmail Bearer stored"

    AI CONNECTOR SECRET SET "gmail", "client_id", "client123"
    AI CONNECTOR SECRET SET "gmail", "client_secret", "secretXYZ"
    PRINT "SECRET client_id/secret stored"

    ' 5. Verifica header Authorization (Bearer per MCP client)
    DIM tok$ AS STRING = AI.CONNECTORSECRETGET("gmail", "access_token")
    PRINT "access_token="; LEFT(tok$, 30)
    DIM sec$ AS STRING = AI.CONNECTORSECRETGET("gmail", "client_id")
    PRINT "client_id="; sec$

    ' 6. GitHub con api_key (alternativa a OAuth)
    AI CONNECTOR SECRET SET "github", "api_key", "ghp_demo123"
    DIM key$ AS STRING = AI.CONNECTORSECRETGET("github", "api_key")
    PRINT "github api_key="; LEFT(key$, 20)

    ' 7. Chiamata con OAuth (McpClient.PostJsonRpcRaw usa Bearer da ConnectorAuth)
    ' Se il connettore ha backing MCP server, il Bearer viene iniettato automaticamente
    DIM res$ AS STRING = AI.CONNECTORCALL("gmail", "search", "{""query"":""from:boss""}")
    PRINT "CALL gmail.search: "; LEFT(res$, 300)

    ' 8. Discovery .well-known (OIDC)
    DIM wk$ AS STRING = OAUTH.WellKnown("https://accounts.google.com")
    PRINT "well-known len="; LEN(wk$)

    PRINT "=== OAuth OK ==="
END SUB
