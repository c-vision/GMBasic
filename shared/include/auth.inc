' auth.inc — namespace AUTH via FFI (lib `auth`, libgmbauth). JWT HS256.
'
' Tre funzioni pure di composizione: JWTCREATE(claims$, secret$) -> token,
' JWTVERIFY(token$, secret$) -> BIT, JWTPAYLOAD(token$) -> JSON decodificato.
' Header fisso {"alg":"HS256","typ":"JWT"}; firma HMAC-SHA256; base64url.
'
' La lib è SELF-CONTAINED (SHA-256/HMAC/base64url in C): nessuna dipendenza esterna.
'
' Uso: REQUIRE "auth"  →  IMPORT AUTH

NAMESPACE AUTH

DECLARE NATIVE FUNCTION AUTHJWTCREATE LIB "gmbauth" ALIAS "ofx_auth_jwt_create" (claims AS STRING, secret AS STRING) AS STRING
DECLARE NATIVE FUNCTION AUTHJWTVERIFY LIB "gmbauth" ALIAS "ofx_auth_jwt_verify" (token AS STRING, secret AS STRING) AS LONG
DECLARE NATIVE FUNCTION AUTHJWTPAYLOAD LIB "gmbauth" ALIAS "ofx_auth_jwt_payload" (token AS STRING) AS STRING

' Crea un token JWT HS256 da un payload JSON (es. JSON.STRINGIFY(...)).
PUBLIC FUNCTION JWTCREATE(claims AS STRING, secret AS STRING) AS STRING
    RETURN AUTHJWTCREATE(claims, secret)
END FUNCTION

' BIT: -1 se la firma è valida con `secret`, 0 altrimenti.
PUBLIC FUNCTION JWTVERIFY(token AS STRING, secret AS STRING) AS LONG
    RETURN (AUTHJWTVERIFY(token, secret) <> 0)
END FUNCTION

' Payload JSON decodificato (stringa vuota su token non valido).
PUBLIC FUNCTION JWTPAYLOAD(token AS STRING) AS STRING
    RETURN AUTHJWTPAYLOAD(token)
END FUNCTION
