' MiniTest: variabili IMPLICITE (senza DIM) e con solo il SUFFISSO, nomi di tipo temporale e
'  UDTPACKED/UDTALIGNED su un nome che NON è un TYPE — canone in common/Ast/TypeLayout.cs,
'  parità 4 backend (IL, PCodeVM, VM/C, C99Native).
'  Regola 4: un'implicita numerica è il DOUBLE di default (8); il suffisso $ dichiara una stringa
'  dinamica (-1), i suffissi % ! # NON cambiano il tipo statico (restano 8).
'  Regola 5: DATE/TIME nudi sono le funzioni a zero argomenti → -1; DATETIME/TIMESPAN sono solo
'  nomi di tipo → 8. BOOL (keyword di tipo) = 1.
' EXPECT: 8|-1|-1|-1|-1|8|8|8|8|-1|-1|8|8|1|0|1|0|1
IMPORT SYSTEM
SUB Main()
  iv = 5
  sv = "abc"
  PRINT SIZEOF(iv)
  PRINT SIZEOF(sv)
  PRINT SIZEOF(iv + 1)
  PRINT SIZEOF(-iv)
  a$ = "x"
  b% = 3
  c! = 1.5
  d# = 2.5
  PRINT SIZEOF(a$)
  PRINT SIZEOF(b%)
  PRINT SIZEOF(c!)
  PRINT SIZEOF(d#)
  PRINT SIZEOF(z)
  PRINT SIZEOF(DATE)
  PRINT SIZEOF(TIME)
  PRINT SIZEOF(DATETIME)
  PRINT SIZEOF(TIMESPAN)
  PRINT SIZEOF(BOOL)
  PRINT UDTPACKED(INTEGER)
  PRINT UDTALIGNED(INTEGER)
  PRINT UDTPACKED(nosuchname)
  PRINT UDTALIGNED(nosuchname)
END SUB
