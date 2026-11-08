# blockchain (0.1.0)

Dominio **BLOCKCHAIN** esposto via FFI: hashing (Keccak-256, RIPEMD-160, BLAKE2b-256),
encoding (base58, base32, RLP, varint), indirizzi Bitcoin/Ethereum, BIP39/BIP32,
firma ECDSA secp256k1, difficoltà di blocco e ABI EVM.

Sostituisce il namespace `BLOCKCHAIN` del builtin stdlib (C#): le chiamate passano ora
dalla dylib `libgmbblockchain`.

## FFI: firma C fissa (`DECLARE NATIVE`)

Le funzioni di questa libreria hanno un'ABI fissa: tutte le `DECLARE` con array sono
`DECLARE NATIVE`, quindi il compilatore passa l'array come **solo puntatore** e le
dimensioni restano parametri espliciti (`n`, `rank`, `dims`). Non aggiungete le
auto-extents (`d0..dN`): il C della libreria non le riceve. Regola completa nella
guida FFI e nel manuale III.16.

## Contenuto

- `src/` — copia **self-contained** dei sorgenti (non referenzia gmbsuite né GMBasic);
  la copia canonica vive in `gmbsuite/gmbblockchain`.
- `include/gmbblockchain.h` — header pubblico.
- `blockchain.inc` — export BASIC (`NAMESPACE BLOCKCHAIN`).
- `lib/osx-arm64/libgmbblockchain.dylib` — binario prodotto da `build.sh`.
- `samples/blockchain_demo.bas` — esempio completo.

## Dipendenza esterna

OpenSSL 3 (`libcrypto`), usata per secp256k1 e per i BIGNUM della difficoltà di blocco:
è il motivo per cui il dominio è un **pacchetto** e non una lib di `shared/libs`
(le lib interne sono self-contained, senza dipendenze esterne). Build e runtime
richiedono `brew install openssl@3`.

## Uso

```basic
REQUIRE "blockchain"
IMPORT BLOCKCHAIN

DIM d(2) AS BYTE
d(0) = 97 : d(1) = 98 : d(2) = 99
DIM h(31) AS BYTE
BLOCKCHAIN.KECCAK256(d, h)
PRINT CRYPTOGRAPHY.TOHEX(h)
```

## Indirizzi: chiave COMPRESSA vs NON compressa

Le due funzioni di indirizzo vogliono **forme diverse** della chiave pubblica e la lib
**non converte** tra loro: passare la forma sbagliata non da' un errore, da' un indirizzo
diverso e sbagliato.

| funzione | chiave attesa | nota |
|---|---|---|
| `BTCADDRFROMPUB(pub, testnet)` | **compressa**, 33 byte (`0x02`/`0x03` || `X`) | e' quella che `GETPUBLICKEY` restituisce |
| `ETHADDRFROMPUB(pub)` | **non compressa**, 65 byte (`0x04` || `X` || `Y`) | il byte `0x04` va incluso: si hasha `X||Y` |

Verificato con `priv = 1` (la chiave pubblica di `1` e' il vettore noto di secp256k1):

| chiamata | risultato | |
|---|---|---|
| `GETPUBLICKEY(priv)` | `0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798` (33 byte) | ok |
| `BTCADDRFROMPUB(pub33, FALSE)` | `1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH` | corretto |
| `ETHADDRFROMPUB(pub33)` | `0x7034adcca06964c0dffdb3eae70e8666faa5a63d` | **SBAGLIATO** |
| `ETHADDRFROMPUB(pub65)` | `0x7e5f4552091a69125d5dfcb7b8c2659029395bdf` | corretto |

`DERIVEKEY` e `GETPUBLICKEY` espongono la forma **compressa** (33 byte). Per l'indirizzo
Ethereum serve la **non compressa**: `GETFULLPUBLICKEY(priv, out)` la scrive nel buffer del
chiamante (`DIM out(64) AS BYTE`) e ritorna i byte scritti — 65, oppure 0 se la lib e' stata
costruita senza OpenSSL, -1 se il buffer e' troppo piccolo.

```basic
REQUIRE "blockchain"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY

DIM priv(31) AS BYTE
priv(31) = 1
DIM full(64) AS BYTE
IF GETFULLPUBLICKEY(priv, full) = 65 THEN
    PRINT TOHEX(full)          ' 0479be667e...d4b8   (0x04 || X || Y)
    PRINT ETHADDRFROMPUB(full) ' 0x7e5f4552091a69125d5dfcb7b8c2659029395bdf
END IF
```

Vale sia per `ETHADDRFROMPUB` sia per `BTCADDRFROMPUB`: la **forma** si deduce dal primo byte,
non da `LEN(pub)`, quindi un array piu' grande del necessario (per esempio `DIM p(63)` che
contiene una chiave da 33 byte seguita da zeri) da' lo stesso indirizzo del buffer esatto,
perche' i byte in coda oltre la chiave non entrano nell'hash. Resta consigliato dimensionare
esattamente (`DIM p(32)` per 33 byte, `DIM p(64)` per 65): con un primo byte non riconosciuto
la funzione non fa ipotesi e hasha `LEN(pub)` byte come sono.

In nessun caso si passa a `ETHADDRFROMPUB` una chiave compressa: meglio nessun indirizzo che
uno sbagliato.

## ABI: array di BYTE

Gli array `AS BYTE` sono ammessi come elementi-array FFI: il bridge **non pinna** il
`byte[]` (elemento largo 1 byte) ma costruisce uno **scratch con elementi a 8 byte**,
ci fa copy-in con i valori 0..255 e copy-back dopo la chiamata. La lib riceve quindi
`int64_t*` e riduce ogni elemento a `uint8_t`.

Conseguenze per il chiamante:

- i buffer di uscita li dimensiona il chiamante (`DIM out(31) AS BYTE`);
- i wrapper `PUBLIC` dell'export deducono lunghezze e capacità con `UBOUND`;
- i testi (indirizzi, mnemonic, base58) restano `STRING`.
