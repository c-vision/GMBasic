' MiniTest TYPES — INTEGER è un intero a 64 BIT su tutte e 4 le gambe.
'
' `DIM i AS INTEGER` non deve troncare a 16 bit (era il comportamento della sola gamba IL:
' `i = 70000` lanciava OverflowException, mentre PCode/VM-C/C99Native stampavano 70000;
' `i = LEN(corpo)` di 70 KB diventava 4464 = 70000 MOD 65536). CINT deve conservare il valore
' (70000) e arrotondare all'intero più vicino (2.5→2, 3.5→4, -2.5→-2: banker's rounding,
' identico a Convert.ToInt64 delle gambe gestite e a llround delle gambe C).
'
' EXPECT: int= 70000|len=[ 70000 ]|cint=[ 70000 ]|cint25= 2|cint35= 4|cintneg=-2|sum=[ 70001 ]|arr=[ 70000 ]|ovf= 32768
IMPORT CONVERSIONS
IMPORT STRINGS

SUB Main()
  ' (1) assegnazione oltre il limite a 16 bit
  DIM i AS INTEGER
  i = 70000
  PRINT "int="; i

  ' (2) LEN di una stringa da 70000 caratteri in una variabile INTEGER
  DIM big AS STRING
  big = DUP(70000, "x")
  i = LEN(big)
  PRINT "len=["; i; "]"

  ' (3) CINT di un valore oltre il limite a 16 bit
  PRINT "cint=["; CINT(70000); "]"

  ' (4) arrotondamento all'intero più vicino (banker's rounding, parità 4 gambe)
  PRINT "cint25="; CINT(2.5)
  PRINT "cint35="; CINT(3.5)
  PRINT "cintneg="; CINT(-2.5)

  ' (5) aritmetica su INTEGER senza overflow a 16 bit
  DIM j AS INTEGER
  j = i + 1
  PRINT "sum=["; j; "]"

  ' (6) array di INTEGER: lo store/load conserva il valore (stelem.i8/ldelem.i8)
  DIM v(2) AS INTEGER
  v(0) = 70000
  PRINT "arr=["; v(0); "]"

  ' (7) il vecchio limite (32767 + 1) resta un normale intero, non un errore
  i = 32767
  i = i + 1
  PRINT "ovf="; i
END SUB
