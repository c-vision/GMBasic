' MiniTest — array GLOBALE passato a un parametro array `a()` (parità 4 backend).
'
' Bug IL: il parametro array è BYREF (`ref T[]`), quindi l'argomento passa da EmitAddressOf.
' Per un array dichiarato a livello di MODULO (globale) il vero riferimento vive nel bag
' globale, ma in un'ALTRA istanza del compilatore `_arrays` è vuoto: il fallback dichiarava
' un local MAI scritto → `ldloca` di un array null → NullReferenceException nel callee al
' primo accesso. PCode/VM-C/C99Native davano il risultato corretto.
' Ora EmitAddressOf sintetizza l'ArrayInfo dal bag (LoadGlobal/LoadStatic + Castclass) e
' materializza il riferimento PRIMA di prenderne l'indirizzo.
' EXPECT: sum= 12 |g3= 6 |names=A-B-C|first=A
DIM g(5) AS LONG
DIM nomi(3) AS STRING

FUNCTION SumRange(a() AS LONG, n AS LONG) AS LONG
  DIM i AS LONG, t AS LONG
  FOR i = 0 TO n - 1
    a(i) = i * 2
    t = t + a(i)
  NEXT i
  SumRange = t
END FUNCTION

SUB FillNames(a() AS STRING, n AS LONG)
  DIM i AS LONG
  FOR i = 0 TO n - 1
    a(i) = CHR$(65 + i)      ' A, B, C
  NEXT i
END SUB

SUB Main()
  DIM i AS LONG, s AS STRING
  PRINT "sum="; SumRange(g(), 4); "|g3="; g(3);
  FillNames(nomi(), 3)
  FOR i = 0 TO 2
    IF i > 0 THEN s = s + "-"
    s = s + nomi(i)
  NEXT i
  PRINT "|names="; s; "|first="; nomi(0)
END SUB
