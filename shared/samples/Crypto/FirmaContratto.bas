' FILE: Crypto/FirmaContratto.bas
' SCENARIO DIDATTICO — firmare digitalmente un documento e preparare la transazione
' per uno smart contract.
'
' Un notaio digitale deve: (1) calcolare l'impronta (hash) del documento con tre algoritmi,
' (2) firmarla con la propria chiave privata (ECDSA secp256k1, firma DER), (3) calcolare
' l'indirizzo pubblico a cui la firma appartiene (ETH e BTC), (4) costruire il payload
' della chiamata al contratto (ABI) e il framing della transazione (RLP + varint).
'
' Domini: BLOCKCHAIN + CRYPTOGRAPHY. Tutto deterministico: la chiave privata e' un vettore di
' test fisso e della firma si verifica la STRUTTURA (tag DER 0x30 e lunghezza plausibile),
' non i byte — la nonce ECDSA e' casuale.
'
' NB (2026-11-13) — API dei pacchetti: gli hash del dominio BLOCKCHAIN sono SUB che prendono
' un ARRAY DI BYTE e scrivono in `out()` (dimensionato dal chiamante); i byte si rendono in
' esadecimale con `BYTESHEX`. `CONTRACT(...)` (proxy dinamico di smart contract) NON esiste
' piu': era un tipo del linguaggio, non ABI nativa.
REQUIRE "blockchain"
REQUIRE "crypto"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY

SUB Main()
    DIM documento AS STRING
    documento = "Contratto di vendita del 2026-11-03 - valore 1500 EUR"

    ' I byte del documento: gli hash di BLOCKCHAIN prendono un array BYTE.
    DIM doc(63) AS BYTE
    DIM nDoc AS LONG = FROMHEX(TOHEX(documento), doc)
    ' DIM n(63) => 64 byte: il documento e' piu' corto, quindi si dimensiona ESATTO.
    ' (LEN(array) = UBOUND+1 e' la lunghezza d'ingresso degli hash: un buffer piu' grande
    ' farebbe hasare anche gli zeri in coda.)

    PRINT "== 1. Impronta del documento =="
    ' TRE algoritmi diversi sullo stesso testo: la lunghezza del digest ne identifica
    ' la famiglia (32 byte per Keccak-256/BLAKE2b-256, 20 per RIPEMD-160).
    DIM kh(31) AS BYTE
    DIM bh(31) AS BYTE
    DIM rh(19) AS BYTE
    KECCAK256(doc, kh)
    BLAKE2B256(doc, bh)
    RIPEMD160(doc, rh)
    PRINT "keccak-256:  "; LEN(kh); " byte, inizio "; LEFT(BYTESHEX(kh), 16)
    PRINT "blake2b-256: "; LEN(bh); " byte, inizio "; LEFT(BYTESHEX(bh), 16)
    PRINT "ripemd-160:  "; LEN(rh); " byte, inizio "; LEFT(BYTESHEX(rh), 16)

    PRINT "== 2. Firma digitale del digest =="
    ' Chiave privata di test (1): il suo indirizzo pubblico e' noto, quindi il resto
    ' dello scenario e' riproducibile.
    DIM priv(31) AS BYTE
    priv(31) = 1
    DIM firma(79) AS BYTE
    DIM nFirma AS LONG = SECP256K1SIGN(kh, priv, firma)
    ' Una firma DER inizia con 0x30 (SEQUENCE) e occupa ~68..72 byte a seconda della
    ' codifica di r e s: si verifica la struttura, non il valore (nonce casuale).
    PRINT "tag DER: "; LEFT(BYTESHEX(firma), 2); " (atteso 30)"
    PRINT "lunghezza plausibile: "; (nFirma >= 68 AND nFirma <= 72)

    PRINT "== 3. Identita' del firmatario =="
    DIM pub(63) AS BYTE
    DIM nPub AS LONG = GETPUBLICKEY(priv, pub)
    DIM eth AS STRING
    eth = ETHADDRFROMPUB(pub)
    PRINT "ETH:         "; eth
    PRINT "ETH EIP-55:  "; ETHADDRCHECKSUM(eth)
    PRINT "BTC mainnet: "; BTCADDRFROMPUB(pub, FALSE)
    PRINT "BTC testnet: "; BTCADDRFROMPUB(pub, TRUE)

    PRINT "== 4. Payload della chiamata al contratto =="
    ' balanceOf(address) con un argomento: selettore (4 byte) + parola (32 byte).
    DIM args(0) AS DOUBLE
    args(0) = 1
    DIM payload AS STRING
    payload = ABIENCODE("balanceOf(address)", args)
    PRINT "payload: "; LEFT(payload, 10); "... ("; LEN(payload); " caratteri hex)"
    ' ABIDECODE riempie out() con le parole da 32 byte e ritorna quante ne ha decodificate.
    DIM paroleA(9) AS DOUBLE
    DIM nPar AS LONG = ABIDECODE("0x0000000000000000000000000000000000000000000000000000000000000001", paroleA)
    PRINT "parole decodificate: "; nPar; " — argomento: "; paroleA(0)

    PRINT "== 5. Framing della transazione =="
    ' Una transazione si serializza come lista RLP [nonce, valore]: qui mostriamo i
    ' due mattoni (RLP per i campi, varint per i contatori di protocollo).
    DIM a1(0) AS BYTE
    a1(0) = 65                       ' "A"
    DIM o(63) AS BYTE
    DIM nEnc AS LONG = RLPENCODE(a1, o)
    PRINT "rlp('A'):    "; LEFT(BYTESHEX(o), nEnc * 2)
    DIM dog(2) AS BYTE
    DIM nDog AS LONG = FROMHEX("646f67", dog)
    nEnc = RLPENCODE(dog, o)
    PRINT "rlp('dog'):  "; LEFT(BYTESHEX(o), nEnc * 2)
    DIM nonce(9) AS BYTE
    DIM nNonce AS LONG = VARINTENCODE(300, nonce)
    PRINT "nonce(300): "; LEFT(BYTESHEX(nonce), nNonce * 2); " -> "; VARINTDECODE(nonce)

    PRINT "Documento firmato e transazione pronta."
END SUB
