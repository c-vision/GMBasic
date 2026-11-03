' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
IMPORT TYPEINFO
' FILE: Crypto/CryptoFullCoverage.bas
' Copertura completa delle funzioni Crypto non ancora coperte.
IMPORT BLOCKCHAIN

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "Crypto - Copertura Completa"
    PRINT "============================================================"
    PRINT

    PRINT "-- ABIDECODE --"
    DIM abi_hex$ = "0xa9059cbb0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002a"
    DIM types(0) AS STRING
    DIM decoded = BLOCKCHAIN.ABIDECODE(abi_hex$, types)
    PRINT "ABIDECODE type: "; TYPEINFO(decoded)
    PRINT

    PRINT "Test Completato."
END SUB
