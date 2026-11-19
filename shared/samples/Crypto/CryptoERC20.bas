' FILE: Crypto/CryptoERC20.bas
' =================================================================================================
'       GUIDA TEORICA: ABI EVM DI BASSO LIVELLO (ERC-20) col pacchetto `blockchain`
' =================================================================================================
'
' Il builtin `BLOCKCHAIN.CONTRACT` (proxy dinamico di smart contract) e il tipo `IERC20` NON
' esistono piu': erano tipi/parsing del LINGUAGGIO, non ABI nativa, quindi non avevano un
' equivalente in C e non sono stati portati nel pacchetto.
'
' Resta la parte che serve davvero a costruire una chiamata: il SELETTORE (primi 4 byte del
' Keccak della firma) e la codifica/decodifica ABI delle parole da 32 byte.
' =================================================================================================

REQUIRE "blockchain"
IMPORT BLOCKCHAIN
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY

SUB Main()
    CLS
    PRINT "=================================================================="
    PRINT " CryptoERC20 - ABI EVM di basso livello"
    PRINT "=================================================================="
    PRINT

    PRINT "--- 1. Selettori (Keccak della firma, primi 4 byte) ---"
    PRINT "name():           "; ABISELECTOR("name()")
    PRINT "symbol():         "; ABISELECTOR("symbol()")
    PRINT "decimals():       "; ABISELECTOR("decimals()")
    PRINT "balanceOf(addr):  "; ABISELECTOR("balanceOf(address)")
    PRINT "transfer(...):    "; ABISELECTOR("transfer(address,uint256)")
    PRINT

    PRINT "--- 2. Una parola ABI da 32 byte ---"
    ' balanceOf(holder) = 1 -> selettore + parola con l'argomento
    DIM nums(0) AS DOUBLE
    nums(0) = 1
    PRINT ABIENCODE("balanceOf(address)", nums)
    PRINT

    PRINT "--- 3. Decodifica di una parola ABI ---"
    DIM word$ AS STRING
    word$ = "0x000000000000000000000000000000000000000000000000000000000000002a"
    DIM out(9) AS DOUBLE
    DIM n AS LONG
    n = ABIDECODE(word$, out)
    PRINT "parole decodificate: "; n
    PRINT "valore: "; out(0)
    PRINT

    PRINT "--- 4. Catena completa: selettore + argomento verificati a mano ---"
    PRINT "ATTESO per transfer(address,uint256): 0xa9059cbb"
    PRINT "calcolato: "; ABISELECTOR("transfer(address,uint256)")
END SUB
