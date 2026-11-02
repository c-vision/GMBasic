' MiniTest CONTROLS — CLICKED/CHANGED con handle OPZIONALE (parità 4 backend).
'
' Il parametro `handle` è opzionale (StdLib.Controls: IsOptional; registry arity 0..1):
' `CONTROLS.CLICKED()` e `CONTROLS.CLICKED(0)` devono valere lo stesso (0 senza click).
' Prima: IL andava in IndexOutOfRange (ILGEN) con 0 argomenti; C99Native rifiutava la forma
' con 0 argomenti con un fuorviante "funzione non definita"; PCode/VM-C restituivano 0.
'
' EXPECT: C0= 0  C1= 0  eq=-1|CH0= 0|DBL0= 0
IMPORT CONTROLS

SUB Main()
  DIM a AS DOUBLE
  DIM b AS DOUBLE
  a = CONTROLS.CLICKED()
  b = CONTROLS.CLICKED(0)
  PRINT "C0="; a; " C1="; b; " eq="; (a = b)
  PRINT "CH0="; CONTROLS.CHANGED()
  PRINT "DBL0="; CONTROLS.DOUBLECLICKED()
END SUB
