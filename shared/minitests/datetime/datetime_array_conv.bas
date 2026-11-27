' MiniTest: DATETIME dentro ARRAY e conversioni DATA <-> NUMERO, parità 4 gambe.
'
' Copre i casi che divergevano:
'  - `DIM a(n) AS DATETIME` con elementi assegnati: PRINT/TYPENAME/VALTYPE dell'elemento
'    devono essere quelli di una DATA (VM/C restituiva NIL, C99 il seriale/DOUBLE);
'  - `DIM a(n) AS DOUBLE : a(0) = DATEVALUE(...)`: il target numerico riceve il seriale
'    OADate (il target decide, non il tipo del valore);
'  - `x = datef(0)` (elemento DATA in variabile numerica) → seriale;
'  - `DIM d AS DATETIME : d = datef(1)` → resta DATA;
'  - `DIM back(n) AS DATETIME : back(0) = 44927` (numero in target DATA) → DATA.
'
' EXPECT: len= 4  ub= 3|d0=01/01/2023 00:00:00 d1=01/01/2024 00:00:00|t0=DATETIME v0=D|x= 44927  tx=DOUBLE|d=01/01/2024 00:00:00 td=DATETIME vd=D|n0= 44927  n1= 44927|tn=DOUBLE|b0=01/01/2023 00:00:00 tb=DATETIME vb=D
IMPORT DATETIME

SUB Main()
  DIM datef(3) AS DATETIME
  datef(0) = DATETIME.DATEVALUE("2023-01-01")
  datef(1) = DATETIME.DATEVALUE("2024-01-01")
  PRINT "len="; LEN(datef); " ub="; UBOUND(datef)
  PRINT "d0="; datef(0); " d1="; datef(1)
  PRINT "t0="; TYPENAME(datef(0)); " v0="; VALTYPE(datef(0))

  DIM x AS DOUBLE
  x = datef(0)
  PRINT "x="; x; " tx="; TYPENAME(x)

  DIM d AS DATETIME
  d = datef(1)
  PRINT "d="; d; " td="; TYPENAME(d); " vd="; VALTYPE(d)

  DIM num(3) AS DOUBLE
  num(0) = DATETIME.DATEVALUE("2023-01-01")
  num(1) = datef(0)
  PRINT "n0="; num(0); " n1="; num(1)
  PRINT "tn="; TYPENAME(num(0))

  DIM back(3) AS DATETIME
  back(0) = 44927
  PRINT "b0="; back(0); " tb="; TYPENAME(back(0)); " vb="; VALTYPE(back(0))
END SUB
