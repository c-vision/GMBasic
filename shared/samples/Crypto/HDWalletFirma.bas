' FILE: Crypto/HDWalletFirma.bas
' =================================================================================================
'       SCENARIO DIDATTICO: dal mnemonico alla firma — HD wallet + ECDSA (pacchetto blockchain)
' =================================================================================================
'
' Un software di firma documentale riceve una frase mnemonic (BIP39) e deve, in ordine:
'   1. ricavare il seed master        MNEMONICTOSEED  (PBKDF2-HMAC-SHA512, 2048 iterazioni)
'   2. derivare la chiave master      DERIVEKEY       (97 byte: priv 32 || pub 33 || chainCode 32)
'   3. verificare l'identita'         GETPUBLICKEY    (la pubblica ricavata dalla privata deve
'                                                    coincidere con quella scritta da DERIVEKEY:
'                                                    un wallet che salta questo passo firma con
'                                                    una chiave e pubblica un'altra identita')
'   4. firmare i digest               SECP256K1SIGN   (ECDSA secp256k1, DER)
'
' L'ordine non e' arbitrario: la firma dipende dalla chiave derivata al punto 2, e la chiave
' ha senso solo se l'identita' del punto 3 e' verificata. E' la catena di un wallet reale.
'
' ABI del pacchetto FFI: i dati binari sono ARRAY DI BYTE, i buffer di uscita li dimensiona il
' CHIAMANTE e le lunghezze si deducono con LEN(). Qui seed/dk/priv/pub/digest/sig attraversano
' il confine come PARAMETRI di SUB e FUNCTION, non come variabili locali di chi chiama l'FFI.
'
' ATTENZIONE (trappola dell'ABI): BTCADDRFROMPUB vuole la chiave COMPRESSA (33 byte), ed e'
' quella che DERIVEKEY/GETPUBLICKEY restituiscono. ETHADDRFROMPUB vuole invece la chiave NON
' compressa (65 byte, 0x04 || X || Y): dandole la compressa la lib NON decomprime e produce un
' indirizzo diverso e sbagliato (verificato: per priv=1 da' 0x7034adcc... invece di 0x7e5f4552...).
' Per questo la sezione 4 ricava la non compressa con GETFULLPUBLICKEY.
'
' DETERMINISMO: mnemonic, passphrase e digest sono vettori FISSI, quindi seed, chiavi, indirizzi
' e payload sono riproducibili. Della firma si verifica la STRUTTURA (tag DER 0x30, lunghezza
' 68..72 byte), non i byte: la nonce ECDSA e' casuale. L'output e' quindi identico sulle 4 gambe
' (IL, PCodeVM, VM/C, C99Native).
' =================================================================================================
REQUIRE "blockchain"
REQUIRE "crypto"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY
IMPORT STRINGS

SUB Mostra(etichetta AS STRING, a() AS BYTE, quanti AS LONG)
    PRINT etichetta; LEFT(BYTESHEX(a), quanti * 2)
END SUB

SUB SeedDaMnemonic(mnemonic AS STRING, passphrase AS STRING, seed() AS BYTE)
    DIM n AS LONG
    n = MNEMONICTOSEED(mnemonic, passphrase, seed)
END SUB

SUB ChiaveMaster(seed() AS BYTE, dk() AS BYTE)
    DIM n AS LONG
    n = DERIVEKEY(seed, "m/44'/60'/0'/0/0", dk)
END SUB

' 1 = firma DER strutturalmente valida (tag 0x30, 68..72 byte), 0 = no.
FUNCTION FirmaValida(digest() AS BYTE, priv() AS BYTE) AS LONG
    DIM sig(79) AS BYTE
    DIM n AS LONG
    n = SECP256K1SIGN(digest, priv, sig)
    IF LEFT(BYTESHEX(sig), 2) = "30" AND n >= 68 AND n <= 72 THEN
        RETURN 1
    END IF
    RETURN 0
END FUNCTION

SUB Main()
    PRINT "=================================================================="
    PRINT " HDWalletFirma - dal mnemonico alla firma (pacchetto blockchain)"
    PRINT "=================================================================="
    PRINT

    PRINT "--- 1. mnemonic -> seed master ---"
    DIM mnemonic$ AS STRING
    mnemonic$ = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about"
    DIM passphrase$ AS STRING
    passphrase$ = "TREZOR"
    DIM seed(63) AS BYTE
    SeedDaMnemonic(mnemonic$, passphrase$, seed)
    Mostra("seed (64B): ", seed, 64)
    PRINT

    PRINT "--- 2. seed -> chiave master (priv 32 || pub 33 || chainCode 32) ---"
    DIM dk(96) AS BYTE
    ChiaveMaster(seed, dk)
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
    Mostra("priv:  ", priv, 32)
    Mostra("pub:   ", pub, 33)
    Mostra("chain: ", chain, 32)
    PRINT

    PRINT "--- 3. identita': GETPUBLICKEY(priv) deve coincidere con pub ---"
    DIM re(63) AS BYTE
    DIM n AS LONG
    n = GETPUBLICKEY(priv, re)
    PRINT "pubblica ricavata: "; n; " byte"
    IF LEFT(BYTESHEX(re), n * 2) = BYTESHEX(pub) THEN
        PRINT "identita': OK"
    ELSE
        PRINT "identita': ERRORE"
    END IF
    PRINT

    PRINT "--- 4. identita' pubblica: ETH (non compressa) e BTC (compressa) ---"
    ' ETHADDRFROMPUB vuole la chiave NON compressa (65 byte: 0x04 || X || Y):
    ' GETFULLPUBLICKEY la ricava dalla stessa privata. Con la compressa di DERIVEKEY
    ' l'indirizzo ETH sarebbe un ALTRO indirizzo (la lib non decomprime il punto).
    DIM full(64) AS BYTE
    DIM nfull AS LONG
    nfull = GETFULLPUBLICKEY(priv, full)
    PRINT "chiave completa: "; nfull; " byte"
    DIM eth$ AS STRING
    eth$ = ETHADDRFROMPUB(full)
    PRINT "ETH:        "; eth$
    PRINT "ETH EIP-55: "; ETHADDRCHECKSUM(eth$)
    PRINT "BTC main:   "; BTCADDRFROMPUB(pub, FALSE)
    PRINT "BTC test:   "; BTCADDRFROMPUB(pub, TRUE)
    PRINT

    PRINT "--- 5. firma di tre digest distinti ---"
    DIM lotto1(31) AS BYTE
    DIM nL1 AS LONG = FROMHEX("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", lotto1)
    DIM lotto2(31) AS BYTE
    DIM nL2 AS LONG = FROMHEX("1c8aff950685c2ed4bc3174f3472287b56d9517b9c948127319a09a7a36deac8", lotto2)
    DIM lotto3(31) AS BYTE
    DIM nL3 AS LONG = FROMHEX("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", lotto3)
    DIM esito AS LONG
    DIM firme AS LONG
    firme = 0
    esito = FirmaValida(lotto1, priv)
    PRINT "  lotto 1: DER valida = "; esito
    firme = firme + esito
    esito = FirmaValida(lotto2, priv)
    PRINT "  lotto 2: DER valida = "; esito
    firme = firme + esito
    esito = FirmaValida(lotto3, priv)
    PRINT "  lotto 3: DER valida = "; esito
    firme = firme + esito
    PRINT "firme valide: "; firme; " / 3"
    PRINT

    PRINT "--- 6. payload che il wallet invierebbe al contratto ---"
    DIM nums(0) AS DOUBLE
    nums(0) = 1
    DIM payload$ AS STRING
    payload$ = ABIENCODE("balanceOf(address)", nums)
    PRINT "selector:  "; LEFT(payload$, 10)
    PRINT "payload:   "; LEN(payload$); " caratteri hex"
    DIM o(63) AS BYTE
    n = VARINTENCODE(300, o)
    PRINT "nonce:     "; LEFT(BYTESHEX(o), n * 2)
    PRINT
    PRINT "lotto firmato e pronto alla trasmissione."
END SUB
