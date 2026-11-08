' blockchain_suite.bas — pacchetto `blockchain` (FFI su libgmbblockchain, OpenSSL 3).
'
' Esempio completo del dominio: usa TUTTE le funzioni pubbliche del namespace BLOCKCHAIN,
' un blocco per area (hashing, encoding, RLP/varint, chiavi e indirizzi, HD wallet,
' firma ECDSA, difficoltà, ABI EVM). Ogni sezione spiega input, tipo e significato
' dell'output; i valori stampati sono deterministici salvo dove indicato (firma e
' mnemoniche sono casuali per definizione).
'
' Esecuzione:
'   gmb pkg install --local packages/blockchain/0.1.0
'   gmb run shared/samples/packages/blockchain@0.1.0/blockchain_suite.bas
'
' Requisiti: pacchetto `blockchain` installato, OpenSSL 3 sulla macchina.

REQUIRE "blockchain"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY
IMPORT STRINGS

SUB Main()
    ' ── 1. HASHING ─────────────────────────────────────────────────────────────
    ' Tutti gli hash prendono un array di BYTE e scrivono in un buffer di uscita
    ' dimensionato dal chiamante (32 byte Keccak/BLAKE2b, 20 byte RIPEMD-160).
    ' Input "hello" = 0x68656c6c6f.
    PRINT "== hashing =="
    DIM msg() AS BYTE = FROMHEX("68656c6c6f")
    DIM hk(31) AS BYTE
    DIM hr(19) AS BYTE
    DIM hb(31) AS BYTE
    KECCAK256(msg, hk)          ' hash Ethereum/ABI (32 byte)
    RIPEMD160(msg, hr)          ' 20 byte, storicamente usato da Bitcoin
    BLAKE2B256(msg, hb)         ' 32 byte, alternativo veloce
    PRINT "keccak256   = "; TOHEX(hk)
    PRINT "ripemd160   = "; TOHEX(hr)
    PRINT "blake2b256  = "; TOHEX(hb)

    ' ── 2. ENCODING ────────────────────────────────────────────────────────────
    ' Base58 (Bitcoin) e Base32: encode ritorna STRING, decode riempie un buffer
    ' BYTE e ritorna il numero di byte scritti (-1 = testo non valido).
    PRINT "== encoding =="
    PRINT "base58      = "; BASE58ENCODE(msg)
    DIM dec(63) AS BYTE
    PRINT "base58 dec  = "; TOHEX(dec); " -> "; BASE58DECODE(BASE58ENCODE(msg), dec); TOHEX(dec)
    PRINT "base32      = "; BASE32ENCODE(msg)
    PRINT "base32 dec  = "; BASE32DECODE(BASE32ENCODE(msg), dec); TOHEX(dec)

    ' RLP (Ethereum): codifica una sequenza di byte con la lunghezza in prefisso.
    DIM rlp(63) AS BYTE
    PRINT "rlp         = "; RLPENCODE(msg, rlp); TOHEX(rlp)

    ' VARINT (Bitcoin): interi a lunghezza variabile; decode ritorna il valore LONG.
    DIM vi(15) AS BYTE
    PRINT "varint 150  = "; VARINTENCODE(150, vi); TOHEX(vi); " -> "; VARINTDECODE(vi)

    ' ── 3. CHIAVI E INDIRIZZI ──────────────────────────────────────────────────
    ' Chiave privata = 32 byte; 1 è la chiave del generatore G (facile da verificare).
    ' GETPUBLICKEY produce la compressa (33 byte), GETFULLPUBLICKEY la non compressa
    ' (65 byte: 0x04 || X || Y), che è la forma richiesta da ETHADDRFROMPUB.
    PRINT "== chiavi e indirizzi =="
    DIM priv(31) AS BYTE
    priv(31) = 1
    DIM pub(32) AS BYTE
    DIM pubfull(64) AS BYTE
    PRINT "pub len     = "; GETPUBLICKEY(priv, pub)
    PRINT "pub full len= "; GETFULLPUBLICKEY(priv, pubfull)
    DIM eth AS STRING
    eth = ETHADDRFROMPUB(pubfull)
    PRINT "eth         = "; eth
    PRINT "eth checksum= "; ETHADDRCHECKSUM(eth)
    PRINT "btc mainnet = "; BTCADDRFROMPUB(pub, 0)
    PRINT "btc testnet = "; BTCADDRFROMPUB(pub, -1)

    ' ── 4. HD WALLET (BIP39 / BIP32) ──────────────────────────────────────────
    ' GENERATEMNEMONIC produce una frase casuale di N parole; MNEMONICTOSEED la
    ' trasforma nel seed BIP39 (64 byte) e DERIVEKEY ne ricava la chiave master
    ' (priv || pub || chainCode = 97 byte). Usiamo il vettore di test BIP39
    ' 11x "abandon" + "about" con passphrase "TREZOR": output deterministico.
    PRINT "== HD wallet =="
    DIM seed(63) AS BYTE
    PRINT "seed bytes  = "; MNEMONICTOSEED("abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about", "TREZOR", seed)
    PRINT "seed sha    = "; TOHEX(seed)
    DIM hd(96) AS BYTE
    PRINT "hd bytes    = "; DERIVEKEY(seed, "", hd)
    PRINT "master priv = "; TOHEX(hd)

    ' GENERATEMNEMONIC è casuale: verifichiamo la FORMA (12 parole), non il testo.
    DIM m$ AS STRING
    m$ = GENERATEMNEMONIC(12)
    DIM ws AS LONG
    ws = 1
    DIM i AS LONG
    FOR i = 1 TO LEN(m$)
        IF MID$(m$, i, 1) = " " THEN ws = ws + 1
    NEXT i
    PRINT "mnemonic    = " + STR(ws) + " parole"

    ' ── 5. FIRMA ECDSA (secp256k1) ────────────────────────────────────────────
    ' Firma DER di un hash SHA-256 già calcolato. ECDSA è randomizzata: sono
    ' deterministici solo il prefisso DER 0x30 e la lunghezza (68..72 byte).
    PRINT "== firma =="
    DIM hash() AS BYTE = FROMHEX("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff")
    DIM sig(79) AS BYTE
    DIM siglen AS LONG
    siglen = SECP256K1SIGN(hash, priv, sig)
    PRINT "sig prefix  = 0x"; LEFT$(TOHEX(sig), 2)
    PRINT "sig len >=68= "; (siglen >= 68)

    ' ── 6. DIFFICOLTÀ DI BLOCCO ───────────────────────────────────────────────
    ' BLOCKDIFFICULTY(header, bits) = target / hash dell'header, dove `bits` è
    ' l'intero compatto nBits (0x20FFFFFF = 553648127). Header tutto a zero.
    PRINT "== difficoltà =="
    DIM hdr() AS BYTE = FROMHEX("0000000000000000000000000000000000000000000000000000000000000000")
    PRINT "difficulty  = "; BLOCKDIFFICULTY(hdr, 553648127)

    ' ── 7. ABI EVM ────────────────────────────────────────────────────────────
    ' ABIENCODE(signature, nums) = selettore (4 byte) + argomenti a 32 byte.
    ' ABISELECTOR ritorna solo i primi 4 byte del Keccak della firma.
    ' ABIDECODE converte una parola hex di 32 byte in DOUBLE (buffer del chiamante).
    PRINT "== ABI =="
    DIM nums(0) AS DOUBLE
    nums(0) = 1
    PRINT "abi encode  = "; ABIENCODE("balanceOf(address)", nums)
    PRINT "abi selector= "; ABISELECTOR("balanceOf(address)")
    DIM words(9) AS DOUBLE
    DIM wordsn AS LONG
    wordsn = ABIDECODE("0x0000000000000000000000000000000000000000000000000000000000000001", words)
    PRINT "abi decode  = "; words(0); " (parole: "; wordsn; ")"
END SUB
