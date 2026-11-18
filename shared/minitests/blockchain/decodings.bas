' EXPECT: b32d=68656c6c6f|b58d=68656c6c6f
' MiniTest BLOCKCHAIN (pacchetto FFI): BASE32/BASE58 decode in un buffer BYTE dimensionato
' dal chiamante; la lunghezza scritta la restituisce la funzione (n).
REQUIRE "blockchain"
REQUIRE "crypto"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY

SUB Main()
  DIM o(63) AS BYTE
  DIM n AS LONG
  n = BASE32DECODE("NBSWY3DP", o)
  PRINT "b32d="; LEFT(BYTESHEX(o), n * 2)
  n = BASE58DECODE("Cn8eVZg", o)
  PRINT "b58d="; LEFT(BYTESHEX(o), n * 2)
END SUB
