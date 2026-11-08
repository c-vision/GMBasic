# crypto (0.1.0)

Dominio **CRYPTOGRAPHY** come pacchetto FFI: le primitive stanno in una dylib
(`libgmbcrypto`) invece che nel compilatore.

| gruppo | funzioni |
|---|---|
| hash | SHA-1, SHA-256, SHA-384, SHA-512, SHA3-256, SHA3-512, MD5, RIPEMD-160 (diretto e `*HEX`), hash di file |
| MAC | HMAC-SHA256/384/512 |
| KDF | PBKDF2-SHA256/512, HKDF |
| AEAD | AES-128/256-GCM, ChaCha20-Poly1305 |
| password | `PASSWORDHASH`/`PASSWORDVERIFY` col **formato identico al builtin**: `Base64(salt16).Base64(PBKDF2-SHA256 100000 iter, 32 byte)` |
| checksum | CRC32 (ISO-HDLC, zlib-compatibile), CRC32C (Castagnoli), CRC16 (ARC/CCITT-FALSE/XMODEM/KERMIT), CRC64 (ECMA inizializzato a 0 e XZ), Adler-32, Fletcher-32 |
| encoding | hex, base64, base64url (encode/decode) |
| altro | random crittografico, confronto a tempo costante |

## Dipendenza esterna

OpenSSL 3 (`libcrypto`), usata per hash, HMAC, KDF, AEAD, random: è il motivo per
cui il dominio è un **pacchetto** e non una lib di `shared/libs` (le lib interne
sono self-contained, senza dipendenze esterne). Build e runtime richiedono
`brew install openssl@3`.

## Copia canonica

I sorgenti canonici vivono in **`gmbsuite/gmbcrypto`**; qui c'è la copia
self-contained (`src/`, `include/`) usata dal pacchetto. Le due copie devono
restare **identiche** (confronto byte per byte): il canonico è uno solo.

## Build

```bash
bash build.sh          # produce lib/osx-arm64/libgmbcrypto.dylib
```

Test C dei vettori noti (nel canonico): `cd ~/Documents/Projects/gmbsuite/gmbcrypto && make test`.

## Stato: `crypto.inc` pronto, attivazione bloccata dal builtin

`crypto.inc` esporta il namespace **CRYPTOGRAPHY** con wrapper che ritornano
`ARRAY OF BYTE` (stessa forma del builtin: `PRINT CRYPTOGRAPHY.MD5("hello")`
stampa una lista di interi). Il file viene letto e parsato correttamente; il
fallimento di attivazione e' la regola del linker:

```
REQUIRE: crypto -> 0.1.0
emissione: riga 38: la procedura 'MD5' nel namespace 'CRYPTOGRAPHY' ridefinisce il
builtin esistente con firma contestata (stessa arita' effettiva)
```

Cioe': finche' le 40 procedure `CRYPTOGRAPHY.*` restano builtin, una `DECLARE`
con lo stesso nome e la stessa arita' non puo' esistere. L'attivazione richiede
la rimozione della superficie builtin (TokenKind + `BuiltInFunction` + registry
dei 4 backend + `StdLib.Cryptography.cs` + `StdLib.Runtime.Cryptography.cs`),
cioe' una modifica a `GMBasicSharp/` e `GMBasic/native/` che va autorizzata.

Superficie 0.1.0 verificata a livello C: 37 vettori noti (`make test` nel
canonico). Il sample `samples/crypto_smoke.bas` e' pronto per la prima esecuzione
dopo l'attivazione.

## Non implementate (per decisione)

`SCRYPT`, `ARGON2ID`, `BLAKE3`, `X25519` — richiederebbero una libreria in più e
sono state escluse esplicitamente.

## Ancora da migrare

`KEYGEN`/`KEYIMPORT`/`KEYEXPORT`, `SIGN`/`VERIFY`/`VERIFYRAW`,
`ENCRYPTASYM`/`DECRYPTASYM` (handle di chiave + PEM/DER: serve il modello handle
alla `GEO`/`GRIDNEW`), `AESENCRYPT`/`AESDECRYPT` (forma password-based storica),
`CHACHA20ENCRYPT`/`CHACHA20DECRYPT` (stream grezzo), `RANDOMINT`/`RANDOMDOUBLE`/
`RANDOMNORMAL` (RNG generale, non crittografico).
