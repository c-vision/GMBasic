' EXPECT: dec= 1|n= 1
' MiniTest BLOCKCHAIN (pacchetto FFI): ABIDECODE con il buffer di uscita `out() AS DOUBLE`
' passato come PARAMETRO di una SUB — non un DIM locale del chiamante. E' l'unico percorso
' array del pacchetto che non e' `BYTE`: qui il wrapper `blockchain.inc`
' (ABIDECODE -> BCABIDECODE con cap = LEN(out)) riceve un array DOUBLE di parametro,
' quindi la lunghezza letta da LEN(out) e la scrittura delle parole avvengono su un
' buffer altrui (by-reference), non su una copia.
REQUIRE "blockchain"
IMPORT BLOCKCHAIN

SUB DecWord(hex AS STRING, out() AS DOUBLE)
  DIM n AS LONG
  n = ABIDECODE(hex, out)
  PRINT "dec="; out(0)
  PRINT "n="; n
END SUB

SUB Main()
  DIM w$ AS STRING = "0x0000000000000000000000000000000000000000000000000000000000000001"
  DIM out(9) AS DOUBLE
  DecWord(w$, out)
END SUB
