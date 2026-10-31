' FILE: samples/Community/Crypto/BlockchainFullCoverage.bas
' Sample COMPLETO BLOCKCHAIN — tutte le 18 funzioni del namespace, deterministico.
' Copre: KECCAK256/RIPEMD160/BLAKE2B256, RLPENCODE/VARINTENCODE/VARINTDECODE,
' ETHADDRFROMPUB/ETHADDRCHECKSUM/BTCADDRFROMPUB, BLOCKDIFFICULTY, SECP256K1SIGN,
' GENERATEMNEMONIC/MNEMONICTOSEED/DERIVEKEY/GETPUBLICKEY, CONTRACT/ABIENCODE/ABIDECODE.
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY
IMPORT SYSTEM

SUB Main()
    DIM data$ = "gm-basic blockchain"
    DIM dataBytes() = FROMHEX("676d2d626173696320626c6f636b636861696e")

    PRINT "=== 1. Hashing ==="
    PRINT "keccak   = "; LEFT(TOHEX(KECCAK256(dataBytes())), 16)
    PRINT "ripemd   = "; LEFT(TOHEX(RIPEMD160(dataBytes())), 16)
    PRINT "blake2b  = "; LEFT(TOHEX(BLAKE2B256(dataBytes())), 16)

    PRINT "=== 2. Encoding ==="
    PRINT "rlpDog   = "; TOHEX(RLPENCODE("dog"))
    DIM vi() = VARINTENCODE(300)
    PRINT "varintE  = "; TOHEX(vi())
    PRINT "varintD  = "; VARINTDECODE(vi())

    PRINT "=== 3. HD Wallet (BIP39/BIP44) ==="
    ' GENERATEMNEMONIC è CASUALE: si verifica solo che produca una frase (deterministico),
    ' mentre la catena seed→priv→pub usa un MNEMONICO FISSO (vettore di test BIP-39) così
    ' l'output del sample è riproducibile e comparabile fra i backend.
    DIM gen$ = GENERATEMNEMONIC(12)
    IF LEN(gen$) > 0 THEN PRINT "mnemonic = ok" ELSE PRINT "mnemonic = no"
    DIM mnemonic$ = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about"
    DIM seed() AS BYTE = MNEMONICTOSEED(mnemonic$, "passphrase")
    PRINT "seed     = "; LEFT(TOHEX(seed), 16)
    DIM dk = DERIVEKEY(seed(), "m/44'/60'/0'/0/0")
    DIM priv() = dk("privateKey")
    DIM pub() = GETPUBLICKEY(priv())
    PRINT "priv     = "; LEFT(TOHEX(priv()), 16)
    PRINT "pub      = "; LEFT(TOHEX(pub()), 16)

    PRINT "=== 4. Indirizzi ==="
    PRINT "eth      = "; ETHADDRFROMPUB(pub())
    PRINT "ethcksum = "; LEFT(ETHADDRCHECKSUM(ETHADDRFROMPUB(pub())), 10)
    PRINT "btc      = "; BTCADDRFROMPUB(pub())
    PRINT "btctest  = "; BTCADDRFROMPUB(pub(), TRUE)

    PRINT "=== 5. Firma + Mining ==="
    DIM msg() = FROMHEX("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff")
    DIM signpriv() = FROMHEX("0000000000000000000000000000000000000000000000000000000000000001")
    DIM sig() = SECP256K1SIGN(msg(), signpriv())
    PRINT "sigprefix= "; LEFT(TOHEX(sig()), 2)
    DIM hdr() = FROMHEX("0000000000000000000000000000000000000000000000000000000000000000")
    PRINT "difficulty="; BLOCKDIFFICULTY(hdr(), 553648127)

    PRINT "=== 6. Smart Contract (ABI) ==="
    DIM enc$ = ABIENCODE("balanceOf(address)", 1)
    PRINT "abienc   = "; LEFT(enc$, 18)
    DIM words() = ABIDECODE("0x0000000000000000000000000000000000000000000000000000000000000001", {"uint256"})
    PRINT "abidec   = "; words(0)
    DIM c = CONTRACT("0x1234567890123456789012345678901234567890", "IERC20")
    PRINT "contract = "; c("address")

    PRINT "Sample BLOCKCHAIN completo."
END SUB
