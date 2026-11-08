' FILE: Crypto/CryptoWalletHD.bas
' =================================================================================================
'       GUIDA TEORICA: HD WALLET (BIP39 / BIP32 / BIP44) col pacchetto `blockchain`
' =================================================================================================
'
'   Frase Mnemonic (BIP39) -> Seed Master (64 byte) -> Chiave master (priv||pub||chainCode)
'
' Con la superficie FFI cambiano DUE cose rispetto al builtin dismesso:
'   - il seed e la chiave non tornano come valore: si passano BUFFER di BYTE da riempire;
'   - DERIVEKEY scrive 97 byte in un buffer piatto (priv 32 || pub 33 || chainCode 32)
'     invece di un DICT.
' Il PATH non viene applicato: si ottiene la chiave MASTER (stessa semantica del builtin).
' =================================================================================================

REQUIRE "blockchain"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY

SUB Show(label AS STRING, a() AS BYTE, n AS LONG)
    PRINT label; LEFT(TOHEX(a), n * 2)
END SUB

SUB Main()
    CLS
    PRINT "=================================================================="
    PRINT " CryptoWalletHD - HD wallet Ethereum (BIP39/BIP44)"
    PRINT "=================================================================="
    PRINT

    PRINT "--- 1. Frase mnemonic (12 parole) ---"
    DIM mnemonic$ AS STRING
    mnemonic$ = GENERATEMNEMONIC(12)
    PRINT mnemonic$
    PRINT

    PRINT "--- 2. Seed master (PBKDF2-HMAC-SHA512, 2048 iterazioni) ---"
    DIM passphrase$ AS STRING
    passphrase$ = "my-secret-password-123"
    DIM seed(63) AS BYTE
    DIM n AS LONG
    n = MNEMONICTOSEED(mnemonic$, passphrase$, seed)
    Show("seed (64B): ", seed, 64)
    PRINT

    PRINT "--- 3. Chiave master (m/44'/60'/0'/0/0) ---"
    PRINT "path: m/44'/60'/0'/0/0  (non applicato: chiave master)"
    DIM dk(96) AS BYTE
    n = DERIVEKEY(seed, "m/44'/60'/0'/0/0", dk)
    DIM priv(31) AS BYTE
    DIM pub(32) AS BYTE
    DIM chain(31) AS BYTE
    DIM i AS LONG
    FOR i = 0 TO 31
        priv(i) = dk(i)
        chain(i) = dk(65 + i)
    NEXT i
    FOR i = 0 TO 32
        pub(i) = dk(32 + i)
    NEXT i
    Show("priv:  ", priv, 32)
    Show("pub:   ", pub, 33)
    Show("chain: ", chain, 32)
    PRINT

    PRINT "--- 4. Indirizzi pubblici ---"
    ' ETHADDRFROMPUB vuole la chiave NON compressa (65 byte, 0x04 || X || Y) e NON
    ' decomprime un punto compresso: con i 33 byte di DERIVEKEY darebbe un indirizzo
    ' DIVERSO e sbagliato. GETFULLPUBLICKEY scrive la non compressa della stessa privata.
    DIM full(64) AS BYTE
    DIM nfull AS LONG
    nfull = GETFULLPUBLICKEY(priv, full)
    DIM eth$ AS STRING
    eth$ = ETHADDRFROMPUB(full)
    PRINT "ETH:         "; eth$
    PRINT "ETH EIP-55:  "; ETHADDRCHECKSUM(eth$)
    ' BTC invece usa proprio la compressa (33 byte): quella scritta da DERIVEKEY.
    PRINT "BTC mainnet: "; BTCADDRFROMPUB(pub, FALSE)
    PRINT "BTC testnet: "; BTCADDRFROMPUB(pub, TRUE)
    PRINT

    PRINT "--- 5. Verifica: GETPUBLICKEY(priv) == pub ---"
    DIM re(63) AS BYTE
    n = GETPUBLICKEY(priv, re)
    IF LEFT(TOHEX(re), n * 2) = TOHEX(pub) THEN
        PRINT "VERIFICA OK"
    ELSE
        PRINT "ERRORE: chiavi pubbliche diverse"
    END IF
END SUB
