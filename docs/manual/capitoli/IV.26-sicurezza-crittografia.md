# IV.26 — Sicurezza e crittografia

## Cosa ottieni da questo capitolo

Le quattro primitive che servono davvero — hash, HMAC, Base64, token firmati — con i vettori di
test ufficiali che ne provano la correttezza, e le regole che nessuna libreria può applicare al
posto tuo.

## 1. Hash: identifica, non nascondere

```basic
md5:     5d41402abc4b2a76b9719d911017c592
sha1:    aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d
sha256:  2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824
```

Eseguito (`shared/samples/Crypto/CryptoHashing.bas`, backend C99Native) sull'input `hello`.
Un hash è **unidirezionale**: non si torna indietro, e questo è il punto. `MD5` e `SHA-1` restano
disponibili per compatibilità (checksum, identificatori), non per la sicurezza: per firmare o
verificare usa `SHA-256` o superiore.

## 2. HMAC: hash con una chiave

Un hash nudo non prova **chi** ha prodotto il dato: chiunque può rifarlo. HMAC lega il dato a una
chiave segreta:

```
HMAC-SHA256: f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8
HMAC256 VECTOR OK
```

Sono i vettori della **RFC 4231** (`shared/samples/Security/HmacAndBase64Url.bas`): se il tuo
ambiente produce questi valori, l'implementazione è corretta — non "sembra corretta". Un HMAC che
non combacia con il vettore è un difetto, non una differenza di versione.

## 3. Base64 e Base64URL

```
TOBASE64URL(hello): aGVsbG8
TOBASE64URL(f):     Zg
FROMBASE64URL roundtrip hex: 68656c6c6f
```

Base64 **non cifra**: è una codifica, chiunque la decodifica. Serve a trasportare byte in un
contesto testuale. La variante **URL-safe** (RFC 4648 §5, senza padding) è quella che si usa in
un token o in un URL, dove `+`, `/` e `=` sono scomodi o vietati.

## 4. Token firmati: JWT

```
=== JWT HS256 ===
VERIFY OK
TAMPER DETECTED
TOKEN: eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjMiLCJyb2xlIjoiYWRtaW4ifQ.H6ttZyZLaKFeuIFWSGcp1tCplV54dnh5WCgqGyvqfEk
PAYLOAD: [{"sub":"123","role":"admin"}]
PAYLOAD MATCH
```

Eseguito (`shared/samples/Security/JwtDemo.bas`). Le tre righe che contano: la verifica passa, la
**manomissione viene rilevata** (un carattere cambiato nella firma invalida il token) e il payload
si rilegge come JSON.

Il payload di un JWT è **leggibile da chiunque**: `eyJzdWIiOiIxMjMi…` è Base64, non cifratura.
Un JWT protegge l'integrità, non la riservatezza. Non metterci dentro segreti.

## 5. Password: mai un hash nudo

```
password: ok
random:  ok
```

Una password non si salva con `SHA-256`: si salva con una funzione lenta e salata (bcrypt, scrypt,
Argon2), altrimenti una tabella precalcolata la ritrova in un secondo. Il sample
`Crypto/CryptoFunctions.bas` copre hash, confronto e generazione casuale; la regola resta: la
verifica della password si fa **solo** con la funzione dedicata, mai confrontando due hash a mano.

## 6. Il resto del catalogo

| Ambito | Sample | Nota |
|---|---|---|
| simmetrico | `Crypto/CryptoSymmetric.bas` | AES: stessa chiave per cifrare e decifrare |
| asimmetrico | `Crypto/CryptoAsymmetric.bas` | chiave pubblica/privata, firma |
| hashing e file | `Crypto/CryptoStrHashFile.bas` | hash di file grandi |
| autenticazione | `Security/AuthFlowDemo.bas` | login, sessione, logout |
| sessioni | `Security/HttpSessionsDemo.bas` | sessione su HTTP |
| header | `Security/HeadersProtectionDemo.bas` | header di sicurezza |
| OAuth | `Security/OAuthClientCredentialsDemo.bas` | flusso client credentials |
| IdP di prova | `Security/OAuthMockIdpDemo.bas` | provider finto per i test |

## 7. Le regole che il codice non può dare

1. **Nessun segreto nel codice** — chiavi, password, token non stanno nel sorgente né nel
   repository: si leggono dalla configurazione dell'ambiente.
2. **TLS per tutto ciò che attraversa la rete** — HTTP in chiaro su `localhost` va bene; fuori, no.
3. **Privilegio minimo** — un processo che legge un file non ha bisogno di scrivere su tutta la
   cartella.
4. **Non inventare crittografia** — usa le primitive della libreria; una "cifratura personale" è
   quasi sempre rotta.
5. **Verifica, non fidarti** — ogni dato che arriva dall'esterno (un URL, un JSON, un token) va
   validato prima dell'uso.

## 8. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| il token "scade" subito | orologio del server sbagliato | sincronizza l'orologio |
| la firma non torna | si firma il JSON riserializzato | firma i byte originali |
| Base64 con `+` e `/` in un URL | codifica standard | usa Base64URL |
| l'hash della password non combacia | sale diverso a ogni verifica | usa la funzione di verifica |
| HMAC diverso fra due linguaggi | chiave in Base64 contro chiave grezza | confronta i byte della chiave |

## 9. Dove andare adesso

- **IV.24** — il canale che queste primitive proteggono.
- **IV.23** — dove finiscono i dati sensibili.
- **V.35** — i modelli locali e la riservatezza dei dati che non escono.














<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Security-Best-Practices

*Fonte: `Security-Best-Practices`*


## 1. Password — NON usare SHA/MD5 in chiaro

IMPORT CRYPTOGRAPHY
DIM hash AS STRING = CRYPTOGRAPHY.PASSWORDHASH("P@ssw0rd")   ' PBKDF2-HMAC-SHA256, salt+hash
DIM ok AS BIT = CRYPTOGRAPHY.PASSWORDVERIFY("P@ssw0rd", hash)


## 2. Firma e integrità — HMAC (HS256-ready)

IMPORT CRYPTOGRAPHY
' HMACSHA256/384/512(data, key) → hex STRING (vettori RFC 4231)
DIM sig$ = CRYPTOGRAPHY.HMACSHA256("messaggio", "chiave-condivisa")


## 3. Base64URL per token

IMPORT CRYPTOGRAPHY
DIM t$ = CRYPTOGRAPHY.TOBASE64URL("payload")     ' RFC 4648 §5, niente padding
DIM bytes() = CRYPTOGRAPHY.FROMBASE64URL(t$)


## 4. Output web — anti-XSS

IMPORT STRINGS
DIM s$ = STRINGS.HTMLENCODE(userInput$)          ' &amp; &lt; &gt; &quot; &#39;
DIM u$ = STRINGS.URLENCODE(query$)               ' percent RFC 3986
DIM j$ = STRINGS.JSENCODE(userInput$)            ' per embed sicuro in literal JS


<p><%= utente.nome %></p>              ' escaped automaticamente (commit 41558638)
<p><%= trustedsHtml$ | raw %></p>      ' SOLO HTML fidato
<p>@Html.Encode(Model.Descrizione)</p> ' encode esplicito
<p>@Html.Raw(htmlFidato)</p>           ' raw esplicito


## 5. Server HTTP — header di sicurezza e IP allow/block

IMPORT HTTP
DIM config AS HTTPCONFIG = NEW HTTPCONFIG
config.Port = 8080
config.SecurityHeaders = TRUE                      ' HSTS/CSP/X-Frame-Options/...
config.AllowedIPs = "127.0.0.1, 10.0.0.0/8"       ' allow-list (vuota = tutti)
config.BlockedIPs = "203.0.113.7"                 ' block-list CIDR v4/IP
HTTP CONFIG config


## 6. Sessioni e cookie

## 7. Database — niente SQL per concatenazione

## 8. Checklist rapida

## Gap dichiarati (2026-09-02)

### Cryptography-Guida-Utente

*Fonte: `Cryptography-Guida-Utente`*


## 1. Panoramica

IMPORT CRYPTOGRAPHY
SUB Main()
    PRINT STRMD5("abc")
END SUB


## 2. Hash di una stringa
| Funzione | Algoritmo |
|---|---|
| `STRMD5(str)` | MD5 |
| `STRSHA256(str)` | SHA-256 |

IMPORT CRYPTOGRAPHY
SUB Main()
    PRINT STRMD5("abc")
    PRINT STRSHA256("abc")
END SUB


 900150983cd24fb0d6963f7d28e17f72
 ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad


## 3. Hex e Base64

### 3.1 FROMHEX(hexString) → byte[]

DIM dati() = FROMHEX("474d2d4241534943")


### 3.2 TOHEX(bytes) → stringa

PRINT TOHEX(FROMHEX("DEADBEEF"))   ' "DEADBEEF"


### 3.3 Base64

b64 = ...  ' FROMBASE64(base64String) → byte[]
str = ...  ' FROMBASE64URL(base64urlString) → byte[]


## 4. Hash di array di byte

IMPORT CRYPTOGRAPHY
SUB Main()
    DIM testo() = FROMHEX("474d2d4241534943")
    PRINT TOHEX(MD5(testo()))
    PRINT TOHEX(SHA256(testo()))
END SUB


## 5. Hashing file e HMAC

h = HASHFILE(filePath, algorithm)      ' hash di un file
h = HMACSHA256(data, key)              ' HMAC-SHA-256 (data PRIMO)
h = HMACSHA384(data, key)
h = HMACSHA512(data, key)


## 6. Crittografia (cenni)

## 7. Note

## 8. Verifiche sui quattro backend (2026-09)

## 9. Suite MiniTests `shared/minitests/cryptography/` (2026-09-11)
| MiniTest | Copre |
|---|---|
| `hashes.bas` | `MD5`/`SHA1`/`SHA256`/`SHA512` (byte[]) |
| `strhash.bas` | `STRMD5`/`STRSHA1`/`STRSHA256`/`STRSHA512` (hex) |
| `hmac.bas` | `HMACSHA256`/`384`/`512` |
| `encoding.bas` | `TOBASE64`/`FROMBASE64`, `TOBASE64URL`/`FROMBASE64URL`, `TOHEX`/`FROMHEX` |
| `password.bas` | `PASSWORDHASH`/`PASSWORDVERIFY` |
| `random.bas` | `RANDOMINT`/`RANDOMDOUBLE`/`RANDOMNORMAL`/`RANDOMBYTES`/`RANDOMCRYPTOBYTES` |
| `aes.bas` | `AESENCRYPT`/`AESDECRYPT` (round-trip) |
| `chacha20.bas` | `CHACHA20ENCRYPT`/`CHACHA20DECRYPT` (round-trip) |
| `chacha20poly1305.bas` | `CHACHA20POLY1305ENCRYPT`/`DECRYPT` (AEAD, dict ciphertext/tag) |
| `hashfile.bas` | `HASHFILE`/`STRHASHFILE` |
| `asym_sign.bas` | `KEYGEN`/`SIGN`/`VERIFY`/`KEYEXPORT` (asimmetrico ECDSA) |
| `keyimport.bas` | `KEYIMPORT` (PEM → handle) |
| `verifyraw.bas` | `VERIFYRAW` (Ed25519, RFC 8032 test vector) |
| `encryptasym.bas` | `ENCRYPTASYM`/`DECRYPTASYM` (RSA) |

### Security-Middleware-Reference

*Fonte: `Security-Middleware-Reference`*


## 1. Dove si applica la sicurezza

## 2. Riferimento rapido delle opzioni `HTTP CONFIG`
| Campo | Tipo | Effetto | Codice di risposta |
|---|---|---|---|
| `SecurityHeaders` | BIT | header di sicurezza su ogni risposta | — |
| `AllowedIPs` / `BlockedIPs` | STRING | IP singoli/CIDR IPv4, virgola | 403 |
| `MaxRequestBodySize` | LONG | limite body richiesta | 413 |
| `RateLimitRequestsPerMinute` / `RateLimitBurstSize` | INTEGER | token bucket | 429 |
| `RequestTimeoutMs` / `DefaultRequestTimeout` | INTEGER | timeout handler | 503 |
| `EnableKeepAlive` | BIT | keep-alive | — |
| `CertificatePath` / `CertificatePassword` | STRING | HTTPS (.pfx) — va impostato PRIMA di `HTTP SERVER` | — |
| `Sessions` | BIT | sessioni server-side route HTTP | cookie alla 1ª richiesta |
| `SessionFile` | STRING | file SQLite persistenza sessioni ("" = in-memory) | — |

## 3. Header di sicurezza automatici

## 4. Componenti (middleware/handler) e difesa in profondità

## 5. Esempio "tutto insieme"

IMPORT HTTP
DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
cfg.Port = 8443
cfg.SecurityHeaders = TRUE
cfg.AllowedIPs = "127.0.0.1, 192.168.0.0/16"
cfg.BlockedIPs = "203.0.113.7"
cfg.MaxRequestBodySize = 1 * 1024 * 1024
cfg.RateLimitRequestsPerMinute = 600
cfg.RateLimitBurstSize = 1000
cfg.Sessions = TRUE
cfg.SessionFile = "work/sessions.db"
' cfg.CertificatePath = "server.pfx" : cfg.CertificatePassword = "pw"   ' HTTPS
HTTP CONFIG cfg

SUB Login(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    ' ... valida credenziali ...


## 6. Gap noti

<!-- END SURFACE -->



<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **AUTH.JWTCREATE(x1, x2)** — `DIM token$ = AUTH.JWTCREATE(claims$, "s3cret")` (shared/samples/Security/JwtDemo.bas:15)
- **AUTH.JWTPAYLOAD(x1)** — `DIM p$ = AUTH.JWTPAYLOAD(token$)` (shared/samples/Security/JwtDemo.bas:31)
- **AUTH.JWTVERIFY(x1, x2)** — `IF AUTH.JWTVERIFY(token$, "s3cret") THEN` (shared/samples/Security/JwtDemo.bas:17)
<!-- END DOMAIN TABLE -->
