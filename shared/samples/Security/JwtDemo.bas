' FILE: Security/JwtDemo.bas
' =================================================================================================
' SECURITY — JWT HS256 (dominio AUTH, lib `auth` via FFI)
' =================================================================================================
' Scenario educativo: un servizio emette un token per un utente, lo verifica a ogni richiesta e
' ne legge il payload. Usa TUTTE le funzioni pubbliche del namespace AUTH:
'
'   AUTH.JWTCREATE(claims$, secret$)  → token "header.payload.firma" (HS256, base64url)
'   AUTH.JWTVERIFY(token$, secret$)  → BIT: -1 firma valida, 0 firma errata/manomessa
'   AUTH.JWTPAYLOAD(token$)          → payload JSON decodificato
'
' Punti didattici:
'   - claims$ è il payload JSON: qui lo costruiamo a mano, in un'app reale si usa JSON.
'   - il segreto NON viaggia nel token: senza il segreto giusto la firma non torna.
'   - il payload è leggibile da chiunque (base64url): il token garantisce l'INTEGRITÀ,
'     non la riservatezza.
'
' Esecuzione: `gmb run shared/samples/Security/JwtDemo.bas`
' Output deterministico: claims e segreto sono fissi, quindi anche il token lo è.
' =================================================================================================

IMPORT AUTH

SUB Main()
    DIM q AS STRING
    q = CHR$(34)

    ' ── 1. Payload e token ─────────────────────────────────────────────────────────────────────
    ' Il payload è JSON: sub = utente, role = ruolo applicativo.
    DIM claims AS STRING
    claims = "{" + q + "sub" + q + ":" + q + "123" + q + "," + q + "role" + q + ":" + q + "admin" + q + "}"
    DIM secret AS STRING
    secret = "s3cret"

    DIM token AS STRING
    token = AUTH.JWTCREATE(claims, secret)
    PRINT "== token =="
    PRINT "claims  = "; claims
    PRINT "token   = "; token

    ' ── 2. Verifica della firma ────────────────────────────────────────────────────────────────
    ' Con il segreto giusto JWTVERIFY vale −1 (BIT vero); con uno sbagliato vale 0.
    PRINT "== verifica =="
    PRINT "secret ok  = "; AUTH.JWTVERIFY(token, secret)
    PRINT "secret bad = "; AUTH.JWTVERIFY(token, "wrong-key")

    ' ── 3. Lettura del payload ─────────────────────────────────────────────────────────────────
    ' JWTPAYLOAD non verifica la firma: decodifica soltanto. Va usata DOPO JWTVERIFY.
    PRINT "== payload =="
    DIM payload AS STRING
    payload = AUTH.JWTPAYLOAD(token)
    PRINT "decoded    = "; payload
    PRINT "originale  = "; (payload = claims)

    ' ── 4. Struttura del token ─────────────────────────────────────────────────────────────────
    ' Tre segmenti separati da '.'; il primo è l'header fisso HS256/JWT.
    PRINT "== struttura =="
    PRINT "header     = "; LEFT$(token, 10)
    PRINT "punti      = "; (INSTR(token, ".") > 0)
END SUB
