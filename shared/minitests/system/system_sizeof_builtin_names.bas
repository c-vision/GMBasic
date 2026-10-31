' MiniTest: SIZEOF di un NOME NUDO di FUNZIONE/COSTANTE built-in — canone regola 5
'  (common/Ast/TypeLayout.cs): il token è una KEYWORD che non è nome di tipo né TYPE utente,
'  quindi l'argomento NON ha tipo statico → -1 su tutte e cinque le gambe. Il test è sul
'  TokenKind (TypeLayout.IsBuiltinFunctionKeyword, e in C99Native gmb_sizeof_arg), mai sul lexeme.
'  FLOAT/WORD/NUMERIC valgono 8: NON sono keyword del lessico condiviso (il C# non le lessifica)
'  e arrivano quindi come identificatori impliciti → DOUBLE di default del linguaggio.
' EXPECT: 8|8|8|-1|-1|-1|-1|-1|-1|-1|-1|-1|-1|-1|-1
IMPORT SYSTEM
SUB Main()
  PRINT SIZEOF(FLOAT)
  PRINT SIZEOF(WORD)
  PRINT SIZEOF(NUMERIC)
  PRINT SIZEOF(LEN)
  PRINT SIZEOF(CHR)
  PRINT SIZEOF(VAL)
  PRINT SIZEOF(STR)
  PRINT SIZEOF(PI)
  PRINT SIZEOF(NOW)
  PRINT SIZEOF(FREEFILE)
  PRINT SIZEOF(DATE)
  PRINT SIZEOF(TIME)
  PRINT SIZEOF(TRUE)
  PRINT SIZEOF(FALSE)
  PRINT SIZEOF(NIL)
END SUB
