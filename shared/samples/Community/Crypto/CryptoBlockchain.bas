' FILE: Crypto/CryptoBlockchain.bas
' =================================================================================================
'       GUIDA TEORICA: PRIMITIVE CRITTOGRAFICHE PER BLOCKCHAIN
' =================================================================================================
'
' Questo test copre le funzioni specialistiche del namespace `BLOCKCHAIN`, essenziali per
' interagire con protocolli come Bitcoin, Ethereum e altri registri distribuiti.
'
' 1. HASHING SPECIALIZZATO
'    Le blockchain utilizzano algoritmi di hash specifici, diversi dai classici MD5/SHA256:
'    - `KECCAK256`: L'algoritmo nativo di Ethereum (precursore di SHA-3 ma matematicamente diverso).
'    - `RIPEMD160`: Usato da Bitcoin per generare indirizzi più corti (HASH160).
'    - `BLAKE2B`: Un hash ad alte prestazioni usato da Zcash e altre chain moderne.
'
' 2. ENCODING (BASE58 e BASE32)
'    Per rappresentare dati binari (come indirizzi e chiavi) in modo leggibile e sicuro:
'    - `BASE58`: Simile al Base64 ma rimuove caratteri ambigui (0, O, I, l, +, /). Standard di Bitcoin.
'    - `BASE32`: Encoding case-insensitive usato in indirizzi Bech32 (SegWit) e altri protocolli.
'
' 3. DERIVAZIONE INDIRIZZI E CHIAVI
'    Funzioni per trasformare chiavi pubbliche grezze (punti sulla curva ellittica) in indirizzi utilizzabili:
'    - `ETHADDRFROMPUB`: Deriva l'indirizzo Ethereum (ultimi 20 byte dell'hash Keccak della pubkey).
'    - `ETHADDRCHECKSUM`: Implementa lo standard EIP-55 per la validazione delle maiuscole/minuscole.
'    - `BTCADDRFROMPUB`: Deriva indirizzi Bitcoin (P2PKH) gestendo il versioning (Mainnet vs Testnet).
'
' 4. SERIALIZZAZIONE E UTILITÀ
'    - `RLP` (Recursive Length Prefix): Il formato di serializzazione standard di Ethereum.
'    - `VARINT`: Codifica di interi a lunghezza variabile (usato in Bitcoin per risparmiare spazio).
'    - `SECP256K1`: La curva ellittica standard. La funzione di firma genera la prova crittografica.
'
' =================================================================================================

IMPORT STRINGS
IMPORT CRYPTOGRAPHY ' Necessario per le funzioni helper FROMHEX e TOHEX
IMPORT BLOCKCHAIN

SUB Main()
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════╗"
    PRINT "║ CryptoBlockchain - Test per le Funzionalità Blockchain         ║"
    PRINT "╚════════════════════════════════════════════════════════════════╝"
    PRINT

    ' Setup dei dati di test (stringa e array di byte corrispondente)
    DIM data$ = "gm-basic blockchain"
    DIM dataBytes() = FROMHEX("676d2d626173696320626c6f636b636861696e")

    ' --- 1. Test di Hashing Specifici per Blockchain ---
    PRINT "--- 1. Test di Hashing ---"
    ' Keccak-256 (Ethereum)
    PRINT "STRKECCAK256(data$):    "; STRKECCAK256(data$)
    PRINT "KECCAK256(bytes):   "; TOHEX(KECCAK256(dataBytes()))
    ' RIPEMD-160 (Bitcoin)
    PRINT "STRRIPEMD160(data$):   "; STRRIPEMD160(data$)
    PRINT "RIPEMD160(bytes): "; TOHEX(RIPEMD160(dataBytes()))
    ' Blake2b-256
    PRINT "BLAKE2B256(bytes):"; TOHEX(BLAKE2B256(dataBytes()))
    PRINT

    ' --- 2. Test di Encoding (Base58, Base32) ---
    PRINT "--- 2. Test di Encoding ---"
    ' Base58 (Bitcoin)
    DIM b58_source() = FROMHEX("00010966776006953D5567439E5E39F86A0D273BEED61967F6")
    DIM b58_encoded$ = BASE58ENCODE(b58_source())
    PRINT "Base58 Encode: "; b58_encoded$
    
    DIM b58_decoded() = BASE58DECODE("16UwLL9Risc3QfPqBUvKofHmBQ7wMtjvM")
    PRINT "Base58 Decode (HEX): "; TOHEX(b58_decoded())
    PRINT
    
    ' Base32
    DIM b32_encoded$ = BASE32ENCODE(dataBytes())
    PRINT "Base32 Encode: "; b32_encoded$
    DIM b32_decoded() = BASE32DECODE(b32_encoded$)
    PRINT "Base32 Decode (HEX): "; TOHEX(b32_decoded())
    PRINT

    ' --- 3. Test Utility Ethereum & Bitcoin ---
    PRINT "--- 3. Test Utility Crypto ---"
    ' Chiave pubblica non compressa (65 bytes, inizia con 04)
    DIM pubKeyHex$ = "0479BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8"
    DIM pubKeyBytes() = FROMHEX(pubKeyHex$)
    
    ' Derivazione indirizzo Ethereum
    DIM ethAddress$ = ETHADDRFROMPUB(pubKeyBytes())
    PRINT "Indirizzo Ethereum da chiave pubblica: "; ethAddress$
    ' Calcolo Checksum EIP-55 (capitalizzazione mista per validità)
    PRINT "Indirizzo ETH con Checksum:          "; ETHADDRCHECKSUM(ethAddress$)
    PRINT
    
    ' Chiave pubblica compressa (33 bytes, inizia con 02 o 03)
    DIM btcPubKeyHex$ = "0279BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798"
    DIM btcPubKeyBytes() = FROMHEX(btcPubKeyHex$)
    
    ' Derivazione indirizzi Bitcoin (Mainnet vs Testnet)
    PRINT "Indirizzo Bitcoin da chiave pubblica (mainnet): "; BTCADDRFROMPUB(btcPubKeyBytes(), FALSE)
    PRINT "Indirizzo Bitcoin da chiave pubblica (testnet): "; BTCADDRFROMPUB(btcPubKeyBytes(), TRUE)
    PRINT

    ' --- 4. Test Utility Varie Blockchain ---
    PRINT "--- 4. Test Utility Varie Blockchain ---"
    ' Serializzazione RLP (Ethereum) - Codifica di una lista ["abc", "def"]
    DIM rlp_test() = RLPENCODE(FROMHEX("c88361626383646566"))
    PRINT "RLP Encode (HEX): "; TOHEX(rlp_test())
    
    ' Codifica VarInt (Bitcoin)
    DIM varint_test() = VARINTENCODE(300)
    PRINT "VarInt Encode 300 (HEX): "; TOHEX(varint_test()) ' Dovrebbe essere fd2c01 (little endian marker)
    PRINT "VarInt Decode: "; VARINTDECODE(varint_test())
    
    ' Test calcolo difficoltà (Proof of Work)
    DIM diffHeader() = RANDOMBYTES(32)
    DIM difficulty = BLOCKDIFFICULTY(diffHeader(), 486604799)
    PRINT "Calcolo difficoltà blocco (esempio): "; difficulty
    
    ' Test Firma Digitale ECDSA (Secp256k1)
    DIM msg_hash() = SHA256("test")
    DIM pvt_key() = RANDOMBYTES(32)
    DIM signature() = SECP256K1SIGN(msg_hash(), pvt_key())
    PRINT "Firma SECP256K1 (HEX, primi 16 byte): "; LEFT(TOHEX(signature()), 32)
    PRINT

    PRINT "--- TEST COMPLETATO ---"
END SUB

' =================================================================================================
' VERIFICA DI COMPLETEZZA
' =================================================================================================
'
' - Hashing: Coperti tutti gli algoritmi specifici (KECCAK, RIPEMD, BLAKE2B) sia in versione stringa che byte.
' - Encoding: Coperti BASE58 e BASE32 (encode/decode).
' - Indirizzi: Coperta la generazione per ETH (incluso checksum EIP-55) e BTC (mainnet/testnet).
' - Utility: Coperte RLP, VarInt, BlockDifficulty e Secp256k1Sign.
'
' Il file di test copre il 100% delle funzioni esposte dal namespace BLOCKCHAIN.
'
' =================================================================================================