' MiniTest: FORME di argomento di SIZEOF / SYSTEM.UDTPACKED / SYSTEM.UDTALIGNED — canone in
'  common/Ast/TypeLayout.cs, parità 4 backend (IL, PCodeVM, VM/C, C99Native).
'  Coperture: nome di tipo scalare · letterali (numerico = 8, stringa = -1) · suffissi $ % ! #
'  su DIM ($ dichiara stringa → -1, gli altri restano DOUBLE → 8) · TYPE VUOTO (0 byte allineato
'  a 1) · UDT annidati packed/non-packed · campo array (riferimento, NON inline) · CURRENCY
'  (allineamento 16) · espressioni aritmetiche e chiamate di funzione (-1, regola 5) ·
'  UDTPACKED/UDTALIGNED = NUMERO DI CAMPI (proprietà, non serializzazione).
' EXPECT: 2|8|8|4|-1|1|8|8|-1|-1|2|4|8|0|0|0|1|2|16|4|8|2|32|-1|-1|-1|-1|2|1|2|2|1
IMPORT SYSTEM

TYPE Empty
END TYPE

TYPE HasCurrency
  a AS INTEGER
  c AS CURRENCY
END TYPE

TYPE Inner
  x AS INTEGER
END TYPE

TYPE OuterPlain
  i AS Inner
  d AS DOUBLE
END TYPE

PACKED TYPE InnerPacked
  x AS INTEGER
  y AS INTEGER
END TYPE

TYPE OuterOfPacked
  p AS InnerPacked
  b AS BYTE
END TYPE

SUB Main()
  ' NOME DI TIPO SCALARE come argomento
  PRINT SIZEOF(INTEGER)
  PRINT SIZEOF(LONG)
  PRINT SIZEOF(DOUBLE)
  PRINT SIZEOF(SINGLE)
  PRINT SIZEOF(STRING)
  PRINT SIZEOF(BIT)
  ' LETTERALI
  PRINT SIZEOF(1.0)
  PRINT SIZEOF(42)
  PRINT SIZEOF("abc")
  ' SUFFISI BASIC
  DIM sv$ : PRINT SIZEOF(sv)
  DIM si% : PRINT SIZEOF(si)
  DIM sl! : PRINT SIZEOF(sl)
  DIM sd# : PRINT SIZEOF(sd)
  ' TYPE VUOTO
  PRINT SIZEOF(Empty)
  DIM e AS Empty : PRINT SIZEOF(e)
  PRINT SYSTEM.UDTPACKED(e)
  PRINT SYSTEM.UDTALIGNED(e)
  ' NESTED
  PRINT SIZEOF(Inner)
  PRINT SIZEOF(OuterPlain)
  PRINT SIZEOF(InnerPacked)
  PRINT SIZEOF(OuterOfPacked)
  DIM oq AS OuterOfPacked : PRINT SYSTEM.UDTPACKED(oq)
  ' CURRENCY (allineamento 16)
  PRINT SIZEOF(HasCurrency)
  ' ARGOMENTO = ESPRESSIONE/FUNZIONE
  DIM a1(3) AS INTEGER : PRINT SIZEOF(a1)
  DIM parr(2) AS InnerPacked : PRINT SIZEOF(parr)
  DIM n AS LONG : n = 3 : PRINT SIZEOF(n + 1)
  PRINT SIZEOF(LEN("hello"))
  ' UDTPACKED/UDTALIGNED con NOME DI TIPO e con SCALARE
  DIM ip AS InnerPacked : PRINT SYSTEM.UDTPACKED(ip)
  PRINT SYSTEM.UDTALIGNED(ip)
  DIM op AS OuterPlain : PRINT SYSTEM.UDTPACKED(op)
  DIM cur AS HasCurrency : PRINT SYSTEM.UDTPACKED(cur)
  DIM gl AS LONG : PRINT SYSTEM.UDTALIGNED(gl)
END SUB
