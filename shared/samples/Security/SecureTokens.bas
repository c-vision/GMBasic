' FILE: Security/SecureTokens.bas
' =================================================================================================
'       SECURITY — token sicuri: PASSWORDHASH/VERIFY + JWT HS256 + HMAC
' =================================================================================================
' Verifica end-to-end del pattern "credential store sicuro + bearer token":
'   - CRYPTOGRAPHY.PASSWORDHASH/VERIFY (PBKDF2-HMAC-SHA256, formato b64(salt).b64(hash))
'   - AUTH.JWTCREATE/JWTVERIFY/JWTPAYLOAD (HS256)
'   - CRYPTOGRAPHY.HMACSHA256 (firma)
' Eseguibile e identico su IL, PCodeVM, VM/C e C99Native (AUTH e CRYPTOGRAPHY via FFI).

REQUIRE "crypto"
IMPORT CRYPTOGRAPHY
IMPORT AUTH

PRINT "=== Credenziali: PASSWORDHASH/VERIFY ==="
DIM pw$ = "P@ssw0rd-Sicura!"
DIM hash$ = CRYPTOGRAPHY.PASSWORDHASH(pw$)
IF CRYPTOGRAPHY.PASSWORDVERIFY(pw$, hash$) THEN
    PRINT "PWHASH OK"
ELSE
    PRINT "PWHASH FAIL"
END IF
IF CRYPTOGRAPHY.PASSWORDVERIFY("password-sbagliata", hash$) THEN
    PRINT "PWV NEG FAIL"
ELSE
    PRINT "PWV NEG OK"
END IF

PRINT "=== Bearer token: JWT HS256 sui claims ==="
DIM q$ = CHR$(34)
DIM claims$ = "{" + q$ + "sub" + q$ + ":" + q$ + "user-42" + q$ + "," + q$ + "role" + q$ + ":" + q$ + "admin" + q$ + "}"
DIM tok$ = AUTH.JWTCREATE(claims$, "hs256-secret")
PRINT "TOKEN ok:  "; AUTH.JWTVERIFY(tok$, "hs256-secret")
PRINT "TOKEN bad: "; AUTH.JWTVERIFY(tok$, "altro-secret")

PRINT "=== Firma messaggio: HMAC-SHA256 ==="
PRINT "SIG: "; CRYPTOGRAPHY.HMACSHA256("messaggio da firmare", "chiave-condivisa")
