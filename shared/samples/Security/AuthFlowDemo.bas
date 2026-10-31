' FILE: Security/AuthFlowDemo.bas
' =================================================================================================
'       SECURITY — flusso auth completo (demo, niente server): credenziali + JWT + ruolo
' =================================================================================================
' Simula il cuore di un login/autorizzazione:
'   1) REGISTRAZIONE: memorizza PASSWORDHASH della password (mai la password in chiaro)
'   2) LOGIN: PASSWORDVERIFY -> se ok emette JWT HS256 con claims {sub, role}
'   3) AUTORIZZAZIONE: JWTVERIFY (integrita'/firma) + estrazione ruolo dal payload
' Serve da modello per i controller MVC/BSP (route protette, AUTHORIZE) senza server.
' Eseguibile su IL, PCodeVM e VM/C (C99Native: gap dichiarato 2026-09-02).

IMPORT CRYPTOGRAPHY
IMPORT AUTH

' --- helper: estrae il valore di un claim JSON semplice:  "role":"admin"
FUNCTION RoleOf(payload AS STRING) AS STRING
    DIM key$ = CHR$(34) + "role" + CHR$(34) + ":" + CHR$(34)
    DIM p AS LONG = INSTR(payload, key$)
    IF p <= 0 THEN RETURN ""
    DIM startAt AS LONG = p + LEN(key$)
    DIM endAt AS LONG = INSTR(payload, CHR$(34), startAt)
    IF endAt <= 0 THEN RETURN ""
    RETURN MID(payload, startAt, endAt - startAt)
END FUNCTION

SUB LoginFlow(login$ AS STRING, password$ AS STRING, roleExpected$ AS STRING)
    DIM storedHash$ = ""
    IF login$ = "admin" THEN storedHash$ = adminHash$
    IF login$ = "user" THEN storedHash$ = userHash$

    IF storedHash$ = "" THEN
        PRINT "login "; login$; ": UNKNOWN USER"
        RETURN
    END IF
    IF CRYPTOGRAPHY.PASSWORDVERIFY(password$, storedHash$) = FALSE THEN
        PRINT "login "; login$; ": WRONG PASSWORD"
        RETURN
    END IF

    ' login ok -> token con claims {sub, role}
    DIM q$ = CHR$(34)
    DIM claims$ = "{" + q$ + "sub" + q$ + ":" + q$ + login$ + q$ + "," + q$ + "role" + q$ + ":" + q$ + roleExpected$ + q$ + "}"
    DIM token$ = AUTH.JWTCREATE(claims$, secret$)

    ' autorizzazione: firma valida?
    IF AUTH.JWTVERIFY(token$, secret$) = FALSE THEN
        PRINT "login "; login$; ": TOKEN INVALID"
        RETURN
    END IF

    DIM role$ = RoleOf(AUTH.JWTPAYLOAD(token$))
    PRINT "login "; login$; ": token ok, role="; role$
    IF role$ = roleExpected$ THEN
        PRINT "AUTH OK ("; role$; ")"
    ELSE
        PRINT "AUTH ROLE MISMATCH"
    END IF
END SUB

' MAIN
DIM secret$ = "demo-secret"
DIM adminHash$ = CRYPTOGRAPHY.PASSWORDHASH("admin-pw")
DIM userHash$ = CRYPTOGRAPHY.PASSWORDHASH("user-pw")

PRINT "=== AuthFlow demo ==="
LoginFlow("admin", "admin-pw", "admin")
LoginFlow("user", "user-pw", "user")
LoginFlow("admin", "sbagliata", "admin")
LoginFlow("ghost", "x", "user")
