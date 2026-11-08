# auth — lib JWT HS256 (FFI)

Namespace `AUTH`: composizione di token JWT HS256, senza dipendenze esterne
(SHA-256, HMAC-SHA256 e base64url implementati in `src/auth.c`).

## Superficie

| funzione | parametri | ritorno |
|---|---|---|
| `AUTH.JWTCREATE(claims$, secret$)` | payload JSON, segreto | STRING (token `h.p.s`) |
| `AUTH.JWTVERIFY(token$, secret$)` | token, segreto | BIT (-1 valido / 0 no) |
| `AUTH.JWTPAYLOAD(token$)` | token | STRING (payload JSON decodificato) |

Header fisso `{"alg":"HS256","typ":"JWT"}`; firma `HMAC-SHA256`; base64url senza
padding. `JWTVERIFY` confronta la firma in tempo costante sulla lunghezza.

## Uso

```basic
REQUIRE "auth"
IMPORT AUTH

t$ = JWTCREATE("{""sub"":""x""}", "secret")
PRINT JWTVERIFY(t$, "secret")   ' -1
PRINT JWTPAYLOAD(t$)            ' {"sub":"x"}
```

## Build

```bash
bash build.sh       # → lib/<platform>/libgmbauth.dylib
```
