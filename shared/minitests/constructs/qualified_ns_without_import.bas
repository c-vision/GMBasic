' MiniTest COSTRUTTI — namespace COMPOSTO qualificato SENZA IMPORT (parità 4 backend).
'
' `TRADING.PORTFOLIO.ISLONGONLY(w)` è la forma canonica del registry: il parser deve
' risolvere il namespace più lungo registrato anche quando non è materializzato da un
' IMPORT. Prima: C99Native la risolveva, IL/PCode/VM-C davano
' "Simbolo non trovato: funzione o subroutine non definita: 'ISLONGONLY'".
'
' EXPECT: -1
SUB Main()
  DIM w(2) AS DOUBLE : w = {0.5, 0.3, 0.2}
  PRINT TRADING.PORTFOLIO.ISLONGONLY(w)
END SUB
