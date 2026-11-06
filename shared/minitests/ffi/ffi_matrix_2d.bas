' Matrice 2D passata a un FFI: la DECLARE resta `a()`, le estensioni (d0, d1) le emette il
' compilatore dal rango dell'array (nessuna dimensione esplicita).
' EXPECT: mean= 2.5
DECLARE FUNCTION SUM2D LIB "libdims_ext" ALIAS "sum2" (a() AS DOUBLE) AS DOUBLE
SUB Main()
  DIM m(1, 1) AS DOUBLE
  m(0, 0) = 1: m(0, 1) = 2
  m(1, 0) = 3: m(1, 1) = 4
  PRINT "mean="; SUM2D(m) / 4
END SUB
