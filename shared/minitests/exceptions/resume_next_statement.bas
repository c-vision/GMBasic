' MiniTest EXCEPTIONS — RESUME NEXT riprende dall'ISTRUZIONE successiva, non dall'opcode.
'
' L'errore nasce a metà di `X = A / B` (divisione). La ripresa deve saltare l'INTERA
' istruzione: X conserva il valore precedente (10). Prima PCodeVM e VM/C riprendevano
' dall'opcode subito dopo la divisione (lo STORE), scrivendo nella variabile un valore
' vuoto/errato; IL e C99Native riprendevano già dall'istruzione successiva.
'
' EXPECT: X=[ 10 ]|end
SUB Main()
  ON ERROR GOTO h
  A = 10
  B = 0
  X = 10
  X = A / B
  PRINT "X=["; X; "]"
  GOTO e
h:
  RESUME NEXT
e:
  PRINT "end"
END SUB
