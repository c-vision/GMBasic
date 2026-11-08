' FILE: Crypto/CryptoBlockchain.bas
' =================================================================================================
'       GUIDA TEORICA: PRIMITIVE CRITTOGRAFICHE PER BLOCKCHAIN (pacchetto `blockchain`)
' =================================================================================================
'
' Il dominio `BLOCKCHAIN` non e' piu' un builtin C#: e' una LIBRERIA C (sorgenti in
' gmbsuite/gmbblockchain) distribuita come pacchetto del package manager, perche' richiede
' OpenSSL (`libcrypto`) per secp256k1 e i BIGNUM della difficolta'.
'
'   REQUIRE "blockchain"        ' risolve il pacchetto e include blockchain.inc
'   IMPORT BLOCKCHAIN           ' porta il dominio in scope
'
' ABI: i dati binari sono ARRAY DI BYTE.
'   - il bridge FFI passa ogni elemento come intero a 8 byte (valori 0..255) nello scratch;
'   - i buffer di uscita li dimensiona il CHIAMANTE (`DIM h(31) AS BYTE`) e i wrapper
'     deducono le lunghezze con UBOUND (capienza) e dal valore di ritorno (scritti).
'
' 1. HASHING SPECIALIZZATO
'    - KECCAK256: l'algoritmo nativo di Ethereum (precursore di SHA-3 ma diverso).
'    - RIPEMD160: usato da Bitcoin per gli indirizzi corti (HASH160).
'    - BLAKE2B256: hash ad alte prestazioni (Zcash e altre chain).
' 2. ENCODING (BASE58 / BASE32)
' 3. INDIRIZZI ETH (EIP-55) E BTC (P2PKH mainnet/testnet)
' 4. SERIALIZZAZIONE: RLP, VARINT, ABI EVM, firma ECDSA secp256k1
' =================================================================================================

REQUIRE "blockchain"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY

SUB Main()
    CLS
    PRINT "=================================================================="
    PRINT " CryptoBlockchain - primitive blockchain via pacchetto FFI"
    PRINT "=================================================================="
    PRINT

    ' "gm-basic blockchain" in byte
    DIM data() AS BYTE = FROMHEX("676d2d626173696320626c6f636b636861696e")

    PRINT "--- 1. Hashing ---"
    DIM k(31) AS BYTE
    KECCAK256(data, k)
    PRINT "Keccak-256:  "; TOHEX(k)
    DIM r(19) AS BYTE
    RIPEMD160(data, r)
    PRINT "RIPEMD-160:  "; TOHEX(r)
    DIM b(31) AS BYTE
    BLAKE2B256(data, b)
    PRINT "BLAKE2b-256: "; TOHEX(b)
    PRINT

    PRINT "--- 2. Encoding ---"
    PRINT "Base32: "; BASE32ENCODE(data)
    PRINT "Base58: "; BASE58ENCODE(data)
    DIM back(63) AS BYTE
    DIM n AS LONG
    n = BASE32DECODE(BASE32ENCODE(data), back)
    PRINT "Base32 round-trip: "; LEFT(TOHEX(back), n * 2)
    n = BASE58DECODE(BASE58ENCODE(data), back)
    PRINT "Base58 round-trip: "; LEFT(TOHEX(back), n * 2)
    PRINT

    PRINT "--- 3. Indirizzi ---"
    ' chiave pubblica NON compressa (65 byte, inizio 0x04): e' quella che hasha ETHADDRFROMPUB
    DIM pubFull() AS BYTE = FROMHEX("0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8")
    DIM eth$ AS STRING
    eth$ = ETHADDRFROMPUB(pubFull)
    PRINT "Ethereum:        "; eth$
    PRINT "Ethereum (EIP55): "; ETHADDRCHECKSUM(eth$)
    ' chiave pubblica COMPRESSA (33 byte) per l'indirizzo Bitcoin
    DIM pubCmp() AS BYTE = FROMHEX("0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798")
    PRINT "Bitcoin mainnet: "; BTCADDRFROMPUB(pubCmp, FALSE)
    PRINT "Bitcoin testnet: "; BTCADDRFROMPUB(pubCmp, TRUE)
    PRINT

    PRINT "--- 4. Serializzazione e utilità ---"
    DIM o(63) AS BYTE
    DIM dog() AS BYTE = FROMHEX("646f67")
    n = RLPENCODE(dog, o)
    PRINT "RLP(dog):        "; LEFT(TOHEX(o), n * 2)
    n = VARINTENCODE(300, o)
    PRINT "VarInt(300):     "; LEFT(TOHEX(o), n * 2)
    DIM vi() AS BYTE = FROMHEX("ac02")
    PRINT "VarInt decode:   "; VARINTDECODE(vi)
    DIM hdr() AS BYTE = FROMHEX("0000000000000000000000000000000000000000000000000000000000000000")
    PRINT "Difficoltà:      "; BLOCKDIFFICULTY(hdr, 553648127)
    PRINT

    PRINT "--- 5. Chiavi e firma (secp256k1) ---"
    DIM priv(31) AS BYTE
    priv(31) = 1
    DIM pub(63) AS BYTE
    n = GETPUBLICKEY(priv, pub)
    PRINT "Pubkey(1):       "; LEFT(TOHEX(pub), n * 2)
    DIM msg() AS BYTE = FROMHEX("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff")
    DIM sig(79) AS BYTE
    n = SECP256K1SIGN(msg, priv, sig)
    PRINT "Firma DER (byte):"; n
    PRINT "Firma DER (hex): "; LEFT(TOHEX(sig), n * 2)
    PRINT
    PRINT "--- TEST COMPLETATO ---"
END SUB
