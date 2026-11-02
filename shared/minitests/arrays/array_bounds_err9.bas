' MiniTest ARRAYS — indice fuori dai limiti di un array dimensionato (ERR 9) su 4 gambe.
'
' Con `DIM X(2)` gli indici validi sono 0..2: una LETTURA o una SCRITTURA su X(3) deve
' sollevare ERR 9 ("Subscript out of range") e finire nel CATCH, senza corrompere l'array.
' Prima: IL sollevava l'errore, PCodeVM lo scartava in silenzio (nil), VM/C e C99Native
' ESTENDEVANO l'array (X(3) scritto e riletto) mentre C99 leggeva/sciveva fuori allocazione.
'
' EXPECT: W: 9|R: 9|ok: 10 : 20
SUB Main()
  DIM X(2)
  X(0) = 10
  X(2) = 20
  TRY
    X(3) = 30
    PRINT "BAD-write"
  CATCH e AS ERR
    PRINT "W:"; e.Number
  END TRY
  TRY
    PRINT "BAD-read:"; X(3)
  CATCH e AS ERR
    PRINT "R:"; e.Number
  END TRY
  PRINT "ok:"; X(0); ":"; X(2)
END SUB
