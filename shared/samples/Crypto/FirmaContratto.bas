' FILE: Crypto/FirmaContratto.bas
' SCENARIO DIDATTICO — firmare digitalmente un documento e preparare la transazione
' per uno smart contract.
'
' Un notaio digitale deve: (1) calcolare l'impronta (hash) del documento con tre algoritmi,
' (2) firmarla con la propria chiave privata (ECDSA secp256k1, firma DER), (3) calcolare
' l'indirizzo pubblico a cui la firma appartiene (ETH e BTC), (4) costruire il payload
' della chiamata al contratto (ABI) e il framing della transazione (RLP + varint).
'
' Domini: BLOCKCHAIN + CRYPTOGRAPHY + STRINGS. Tutto deterministico: la chiave privata
' è un vettore di test fisso e della firma si verifica la STRUTTURA (tag DER 0x30 e
' lunghezza plausibile), non i byte — la nonce ECDSA è casuale.
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY

SUB Main()
    DIM documento AS STRING
    documento = "Contratto di vendita del 2026-11-03 - valore 1500 EUR"

    PRINT "== 1. Impronta del documento =="
    ' TRE algoritmi diversi sullo stesso testo: la lunghezza del digest ne identifica
    ' la famiglia (32 byte per Keccak-256/BLAKE2b-256, 20 per RIPEMD-160).
    DIM kh() AS BYTE
    kh() = KECCAK256(documento)
    DIM bh() AS BYTE
    bh() = BLAKE2B256(documento)
    DIM rh() AS BYTE
    rh() = RIPEMD160(documento)
    PRINT "keccak-256: "; LEN(kh()); " byte, inizio "; LEFT(TOHEX(kh()), 16)
    PRINT "blake2b-256: "; LEN(bh()); " byte, inizio "; LEFT(TOHEX(bh()), 16)
    PRINT "ripemd-160: "; LEN(rh()); " byte, inizio "; LEFT(TOHEX(rh()), 16)

    PRINT "== 2. Firma digitale del digest =="
    ' Chiave privata di test (1): il suo indirizzo pubblico è noto, quindi il resto
    ' dello scenario è riproducibile.
    DIM priv() AS BYTE
    priv() = FROMHEX("0000000000000000000000000000000000000000000000000000000000000001")
    DIM firma() AS BYTE
    firma() = SECP256K1SIGN(kh(), priv())
    ' Una firma DER inizia con 0x30 (SEQUENCE) e occupa 68..72 byte a seconda della
    ' codifica di r e s: si verifica la struttura, non il valore (nonce casuale).
    PRINT "tag DER: "; LEFT(TOHEX(firma()), 2); " (atteso 30)"
    PRINT "lunghezza plausibile: "; (LEN(firma()) >= 68 AND LEN(firma()) <= 72)

    PRINT "== 3. Identità del firmatario =="
    DIM pub() AS BYTE
    pub() = GETPUBLICKEY(priv())
    DIM eth AS STRING
    eth = ETHADDRFROMPUB(pub())
    PRINT "ETH:         "; eth
    PRINT "ETH EIP-55:  "; ETHADDRCHECKSUM(eth)
    PRINT "BTC mainnet: "; BTCADDRFROMPUB(pub())
    PRINT "BTC testnet: "; BTCADDRFROMPUB(pub(), TRUE)

    PRINT "== 4. Payload della chiamata al contratto =="
    ' balanceOf(address) con l'indirizzo del firmatario come argomento: selettore
    ' (4 byte) + argomento (32 byte, left-padded).
    DIM payload AS STRING
    payload = ABIENCODE("balanceOf(address)", 1)
    PRINT "payload: "; LEFT(payload, 10); "... ("; LEN(payload); " caratteri hex)"
    ' ABIDECODE restituisce la lista degli argomenti decodificati (qui un uint256).
    DIM argomenti = ABIDECODE("0x0000000000000000000000000000000000000000000000000000000000000001", {"uint256"})
    PRINT "argomento decodificato: "; argomenti(0)

    ' Istanza del contratto: nome leggibile, indirizzo e interfaccia dichiarata.
    DIM contratto = CONTRACT("0x1234567890123456789012345678901234567890", "IERC20")
    PRINT "contratto: "; contratto("address"); " via "; contratto("interface")

    PRINT "== 5. Framing della transazione =="
    ' Una transazione si serializza come lista RLP [nonce, valore]: qui mostriamo i
    ' due mattoni (RLP per i campi, varint per i contatori di protocollo).
    PRINT "rlp('A'):    "; TOHEX(RLPENCODE("A"))
    PRINT "rlp('dog'):  "; TOHEX(RLPENCODE("dog"))
    DIM nonce() AS BYTE
    nonce() = VARINTENCODE(300)
    PRINT "nonce(300): "; TOHEX(nonce()); " -> "; VARINTDECODE(nonce())

    PRINT "Documento firmato e transazione pronta."
END SUB
