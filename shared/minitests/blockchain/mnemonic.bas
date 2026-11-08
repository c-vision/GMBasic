' EXPECT: seed=c55257c360c07c72029aebc1b53c05ed0362ada38ead3e3e9efa3708e53495531f09a6987599d18264c1e1c92f2cf141630c7a3c4ab7c81b2f001698e7463b04|words= 12
' MiniTest BLOCKCHAIN (pacchetto FFI): MNEMONICTOSEED (vettore BIP39 11x abandon + about,
' passphrase TREZOR) scrive 64 byte nel buffer BYTE; GENERATEMNEMONIC(12) ha 12 parole.
REQUIRE "blockchain"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY
IMPORT STRINGS

SUB Main()
  DIM seed(63) AS BYTE
  DIM n AS LONG
  n = MNEMONICTOSEED("abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about", "TREZOR", seed)
  PRINT "seed="; TOHEX(seed)
  DIM m$ AS STRING
  m$ = GENERATEMNEMONIC(12)
  DIM w AS LONG
  w = 1
  DIM i AS LONG
  FOR i = 1 TO LEN(m$)
    IF MID(m$, i, 1) = " " THEN w = w + 1
  NEXT i
  PRINT "words="; w
END SUB
