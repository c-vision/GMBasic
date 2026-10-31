' EXPECT: seed=c55257c360c07c72029aebc1b53c05ed0362ada38ead3e3e9efa3708e53495531f09a6987599d18264c1e1c92f2cf141630c7a3c4ab7c81b2f001698e7463b04|words= 12
' MiniTest BLOCKCHAIN: MNEMONICTOSEED (vettore BIP39: 11x abandon + about / TREZOR) + conteggio parole di GENERATEMNEMONIC(12) (il mnemonico e' random: non stampato).
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY
IMPORT STRINGS
SUB Main()
  DIM seed() = MNEMONICTOSEED("abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about", "TREZOR")
  PRINT "seed="; TOHEX(seed())
  DIM m$ = GENERATEMNEMONIC(12)
  DIM w = 1
  FOR i = 1 TO LEN(m$)
    IF MID(m$, i, 1) = " " THEN w = w + 1
  NEXT i
  PRINT "words="; w
END SUB
