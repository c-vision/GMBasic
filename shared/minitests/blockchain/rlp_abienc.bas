' EXPECT: rlpA=41|rlpDog=83646f67|sel=0x70a08231
' MiniTest BLOCKCHAIN (pacchetto FFI): RLPENCODE su byte singolo <0x80 e su stringa corta,
' + selettore ABI (ABISELECTOR, nessun argomento). La stringa vuota non e' rappresentabile
' come array BYTE di lunghezza 0: il caso rlpEmpty (0x80) e' coperto dal selfcheck C della lib.
REQUIRE "blockchain"
REQUIRE "crypto"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY

SUB Main()
  DIM o(63) AS BYTE
  DIM n AS LONG
  DIM a(0) AS BYTE
  DIM _n_a AS LONG
  _n_a = FROMHEX("41", a)
  n = RLPENCODE(a, o)
  PRINT "rlpA="; LEFT(BYTESHEX(o), n * 2)
  DIM dog(2) AS BYTE
  DIM _n_dog AS LONG
  _n_dog = FROMHEX("646f67", dog)
  n = RLPENCODE(dog, o)
  PRINT "rlpDog="; LEFT(BYTESHEX(o), n * 2)
  PRINT "sel="; ABISELECTOR("balanceOf(address)")
END SUB
