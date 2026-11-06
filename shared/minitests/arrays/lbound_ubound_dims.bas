' LBOUND/UBOUND per asse con lower bound espliciti (1-D/2-D/3-D/4-D), lower misti e
' negativi, array STRING e array di UDT. Verifica anche gli indici utente.
' EXPECT: a= 1 , 3 , 1 , 3 , 60|m= 0 , 1 , 1 , 2 , 1 , 4|n=-1 , 1 ,-2 , 0 , 7 , 9|c= 1 , 2 , 1 , 24|h= 2 , 1 , 16|s=abcd|p= 5 ,ok, 1 , 2
TYPE Cell
  V AS DOUBLE
  Tag AS STRING
END TYPE
SUB Main()
  DIM a(1 TO 3) AS DOUBLE
  a(1) = 10 : a(2) = 20 : a(3) = 30
  PRINT "a="; LBOUND(a); ","; UBOUND(a); ","; LBOUND(a,1); ","; UBOUND(a,1); ","; a(1)+a(2)+a(3)
  DIM m(0 TO 1, 1 TO 2) AS DOUBLE
  m(0,1) = 1 : m(1,2) = 4
  PRINT "m="; LBOUND(m,1); ","; UBOUND(m,1); ","; LBOUND(m,2); ","; UBOUND(m,2); ","; m(0,1); ","; m(1,2)
  DIM n(-1 TO 1, -2 TO 0) AS DOUBLE
  n(-1,-2) = 7 : n(1,0) = 9
  PRINT "n="; LBOUND(n,1); ","; UBOUND(n,1); ","; LBOUND(n,2); ","; UBOUND(n,2); ","; n(-1,-2); ","; n(1,0)
  DIM c(1 TO 2, 1 TO 3, 1 TO 2) AS DOUBLE
  c(1,1,1) = 1 : c(2,3,2) = 24
  PRINT "c="; LBOUND(c,3); ","; UBOUND(c,3); ","; c(1,1,1); ","; c(2,3,2)
  DIM h(1 TO 2, 1 TO 2, 1 TO 2, 1 TO 2) AS DOUBLE
  h(1,1,1,1) = 1 : h(2,2,2,2) = 16
  PRINT "h="; UBOUND(h,4); ","; h(1,1,1,1); ","; h(2,2,2,2)
  DIM s(1 TO 2, 1 TO 2) AS STRING = {{"a","b"},{"c","d"}}
  PRINT "s="; s(1,1); s(1,2); s(2,1); s(2,2)
  DIM p(1 TO 2, 1 TO 2) AS Cell
  p(1,1).V = 5 : p(2,2).Tag = "ok"
  PRINT "p="; p(1,1).V; ","; p(2,2).Tag; ","; LBOUND(p,2); ","; UBOUND(p,2)
END SUB
