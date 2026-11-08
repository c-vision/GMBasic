' EXPECT: rlpA=41|rlpDog=83646f67|sel=0x70a08231
' MiniTest BLOCKCHAIN (pacchetto FFI): RLPENCODE su byte singolo <0x80 e su stringa corta,
' + selettore ABI (ABISELECTOR, nessun argomento). La stringa vuota non e' rappresentabile
' come array BYTE di lunghezza 0: il caso rlpEmpty (0x80) e' coperto dal selfcheck C della lib.
REQUIRE "blockchain"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY

SUB Main()
  DIM o(63) AS BYTE
  DIM n AS LONG
  DIM a() AS BYTE = FROMHEX("41")
  n = RLPENCODE(a, o)
  PRINT "rlpA="; LEFT(TOHEX(o), n * 2)
  DIM dog() AS BYTE = FROMHEX("646f67")
  n = RLPENCODE(dog, o)
  PRINT "rlpDog="; LEFT(TOHEX(o), n * 2)
  PRINT "sel="; ABISELECTOR("balanceOf(address)")
END SUB
