' TAG: library
' GmbLibMath.bas — LIBRARY: GMBASIC che PRODUCE una libreria condivisa (.gmblib).
' In modalita' LIBRARY il backend C non emette `main`: i PUBLIC SUB/FUNCTION diventano
' simboli esportati con ABI stabile `<lib>_<Proc>`. Le STRINGHE al confine sono
' `const char*` + `gmb_int_t len` in ingresso e `const char*` in uscita (ABI "comoda").
LIBRARY "minilib"

PUBLIC FUNCTION Add(a AS INTEGER, b AS INTEGER) AS INTEGER
  Add = a + b
END FUNCTION

PUBLIC FUNCTION Mul(a AS DOUBLE, b AS DOUBLE) AS DOUBLE
  Mul = a * b
END FUNCTION

PUBLIC FUNCTION Greet(name AS STRING) AS STRING
  Greet = "Ciao, " + name
END FUNCTION
