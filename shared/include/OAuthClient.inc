' FILE: include/OAuthClient.inc
' =================================================================================================
'       OAUTH — libreria client OAuth 2.0 / OpenID Connect (di sistema, in include/)
' =================================================================================================
' Libreria BASIC di sistema (importabile con `IMPORT OAUTH`; la root include/ e' sempre nei
' percorsi di ricerca IMPORT). Pattern core:
'   - Authorization Code + PKCE (S256)
'   - Client Credentials / Refresh (request token)
' Usa i namespace builtin CRYPTOGRAPHY, STRINGS e HTTP (da importare nel programma che la usa).
' Discovery .well-known e presets IdP (Azure/Auth0/Okta/Google) sono configurazione: qui si
' passano gli endpoint, non si indovina nulla.

NAMESPACE OAUTH

' --- PKCE: verifier casuale (RFC 7636: 43-128 char, charset base64url) ---
PUBLIC FUNCTION CreateVerifier(length AS LONG) AS STRING
    IF length < 43 OR length > 128 THEN length = 43
    DIM charset$ = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~"
    ' API del pacchetto `crypto`: il generatore riempie un array BYTE out-param e ritorna n.
    DIM bytes(127) AS BYTE
    DIM nBytes AS LONG = CRYPTOGRAPHY.RANDOMCRYPTOBYTES(length, bytes)
    DIM out$ = ""
    DIM i AS LONG
    FOR i = 0 TO length - 1
        out$ = out$ + MID(charset$, (bytes(i) MOD 64) + 1, 1)
    NEXT i
    RETURN out$
END FUNCTION

' --- PKCE: code_challenge S256 (base64url(SHA256(verifier))) ---
PUBLIC FUNCTION CreateChallenge(verifier AS STRING) AS STRING
    RETURN CRYPTOGRAPHY.TOBASE64URL(CRYPTOGRAPHY.SHA256(verifier))
END FUNCTION

' --- Authorization URL (Authorization Code + PKCE) ---
PUBLIC FUNCTION AuthUrl(authorizeEndpoint AS STRING, clientId AS STRING, redirectUri AS STRING, scope AS STRING, state AS STRING, verifier AS STRING) AS STRING
    DIM url$ = authorizeEndpoint + "?response_type=" + STRINGS.URLENCODE("code")
    url$ = url$ + "&client_id=" + STRINGS.URLENCODE(clientId)
    IF LEN(redirectUri) > 0 THEN url$ = url$ + "&redirect_uri=" + STRINGS.URLENCODE(redirectUri)
    IF LEN(scope) > 0 THEN url$ = url$ + "&scope=" + STRINGS.URLENCODE(scope)
    url$ = url$ + "&state=" + STRINGS.URLENCODE(state)
    url$ = url$ + "&code_challenge=" + STRINGS.URLENCODE(CreateChallenge(verifier))
    url$ = url$ + "&code_challenge_method=S256"
    RETURN url$
END FUNCTION

' --- Token body form-urlencoded (client_credentials | refresh_token | authorization_code) ---
PUBLIC FUNCTION TokenBody(grantType AS STRING, clientId AS STRING, clientSecret AS STRING, codeOrRefresh AS STRING, redirectUri AS STRING, scope AS STRING) AS STRING
    DIM body$ = "grant_type=" + STRINGS.URLENCODE(grantType)
    body$ = body$ + "&client_id=" + STRINGS.URLENCODE(clientId)
    IF LEN(clientSecret) > 0 THEN body$ = body$ + "&client_secret=" + STRINGS.URLENCODE(clientSecret)
    IF grantType = "refresh_token" AND LEN(codeOrRefresh) > 0 THEN body$ = body$ + "&refresh_token=" + STRINGS.URLENCODE(codeOrRefresh)
    IF grantType = "authorization_code" THEN
        IF LEN(codeOrRefresh) > 0 THEN body$ = body$ + "&code=" + STRINGS.URLENCODE(codeOrRefresh)
        IF LEN(redirectUri) > 0 THEN body$ = body$ + "&redirect_uri=" + STRINGS.URLENCODE(redirectUri)
    END IF
    IF LEN(scope) > 0 THEN body$ = body$ + "&scope=" + STRINGS.URLENCODE(scope)
    RETURN body$
END FUNCTION

' --- Token request: ritorna il corpo JSON della risposta (parsalo con JSON.PARSE) ---
PUBLIC FUNCTION RequestToken(tokenEndpoint AS STRING, body AS STRING) AS STRING
    RETURN HTTP.POST(tokenEndpoint, body, "application/x-www-form-urlencoded")
END FUNCTION

' --- Presets IdP: issuer pubblico ("" se il provider richiede un dominio custom) ---
PUBLIC FUNCTION Issuer(provider AS STRING) AS STRING
    IF provider = "azure" THEN RETURN "https://login.microsoftonline.com/common/v2.0"
    IF provider = "google" THEN RETURN "https://accounts.google.com"
    RETURN ""   ' auth0/okta richiedono il dominio del tenant: passare l'issuer esplicito
END FUNCTION

' --- Endpoint precompilati dai presets ("" = usare la discovery WellKnown) ---
PUBLIC FUNCTION AuthorizeEndpoint(provider AS STRING) AS STRING
    IF provider = "google" THEN RETURN "https://accounts.google.com/o/oauth2/v2/auth"
    RETURN ""   ' azure/custom: derivare da Issuer + WellKnown
END FUNCTION

PUBLIC FUNCTION TokenEndpoint(provider AS STRING) AS STRING
    IF provider = "google" THEN RETURN "https://oauth2.googleapis.com/token"
    RETURN ""
END FUNCTION

' --- OpenID Connect discovery: scarica .well-known/openid-configuration dell'issuer ---
' (ritorna il JSON grezzo; parsalo con JSON.PARSE per estrarre authorization_endpoint/token_endpoint)
PUBLIC FUNCTION WellKnown(issuer AS STRING) AS STRING
    DIM url$ = issuer
    IF RIGHT(url$, 1) <> "/" THEN url$ = url$ + "/"
    RETURN HTTP.GET(url$ + ".well-known/openid-configuration")
END FUNCTION


