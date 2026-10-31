' FILE: Security/JwtDemo.bas
' =================================================================================================
'       SECURITY — JWT HS256 (dominio AUTH)
' =================================================================================================
' Verifica end-to-end di AUTH.JWTCREATE / JWTVERIFY / JWTPAYLOAD (2026-09-02).
' Eseguibile su IL, PCodeVM e VM/C (C99Native: gap dichiarato 2026-09-02).

IMPORT AUTH

PRINT "=== JWT HS256 ==="

DIM q$ = CHR$(34)
DIM claims$ = "{" + q$ + "sub" + q$ + ":" + q$ + "123" + q$ + "," + q$ + "role" + q$ + ":" + q$ + "admin" + q$ + "}"

DIM token$ = AUTH.JWTCREATE(claims$, "s3cret")

IF AUTH.JWTVERIFY(token$, "s3cret") THEN
    PRINT "VERIFY OK"
ELSE
    PRINT "VERIFY FAIL"
END IF

IF AUTH.JWTVERIFY(token$, "wrong-key") THEN
    PRINT "TAMPER NOT DETECTED"
ELSE
    PRINT "TAMPER DETECTED"
END IF

PRINT "TOKEN: "; token$

DIM p$ = AUTH.JWTPAYLOAD(token$)
PRINT "PAYLOAD: ["; p$; "]"
IF p$ = claims$ THEN
    PRINT "PAYLOAD MATCH"
ELSE
    PRINT "PAYLOAD MISMATCH"
END IF
