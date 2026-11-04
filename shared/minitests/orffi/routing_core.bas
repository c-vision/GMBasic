' OPTIMIZE (FFI or) — routing: TSP euristico/esatto, VRP, arc routing.
' EXPECT: nn=4.000|2opt=4.000|cost=4.000|hk=4.000|dt=4.000|1tree=4.000|subtours= 1 |vrp= 1 :6.0|arc=3.0: 4 
IMPORT OPTIMIZE

SUB Main()
  ' quadrato 4 citta' (0,0)(1,0)(1,1)(0,1)
  DIM d(15) AS DOUBLE
  DIM px(3) AS DOUBLE
  DIM py(3) AS DOUBLE
  px(0)=0 : px(1)=1 : px(2)=1 : px(3)=0
  py(0)=0 : py(1)=0 : py(2)=1 : py(3)=1
  FOR i = 0 TO 3
    FOR j = 0 TO 3
      d(i*4+j) = SQR((px(i)-px(j))^2 + (py(i)-py(j))^2)
    NEXT j
  NEXT i

  DIM tour(3) AS LONG
  FOR i = 0 TO 3
    tour(i) = i
  NEXT i
  PRINT "nn="; FORMAT(OPTIMIZE.TSP_NEAREST(d, 4, tour), "#.000");
  PRINT "|2opt="; FORMAT(OPTIMIZE.TSP_2OPT(d, 4, tour, 50), "#.000");
  PRINT "|cost="; FORMAT(OPTIMIZE.TSP_TOUR_COST(d, 4, tour), "#.000");

  DIM t2(3) AS LONG
  FOR i = 0 TO 3
    t2(i) = i
  NEXT i
  PRINT "|hk="; FORMAT(OPTIMIZE.TSP_HELD_KARP(d, 4, t2), "#.000");
  PRINT "|dt="; FORMAT(OPTIMIZE.TSP_DOUBLE_TREE(d, 4, t2), "#.000");

  DIM edges(7) AS LONG
  PRINT "|1tree="; FORMAT(OPTIMIZE.TSP_ONE_TREE(d, 4, 0, edges), "#.000");

  ' sottotour del ciclo 0->1->2->0
  DIM succ(2) AS LONG
  succ(0)=1 : succ(1)=2 : succ(2)=0
  DIM comp(2) AS LONG
  DIM sizes(2) AS LONG
  PRINT "|subtours="; OPTIMIZE.TSP_SUBTOURS(succ, 3, comp, sizes);

  ' VRP: 3 clienti su una linea, deposito 0, cap 3 -> 1 rotta, distanza 6
  DIM vd(15) AS DOUBLE
  FOR i = 0 TO 3
    FOR j = 0 TO 3
      vd(i*4+j) = ABS(i-j)
    NEXT j
  NEXT i
  DIM dem(2) AS DOUBLE
  dem(0)=1 : dem(1)=1 : dem(2)=1
  DIM rof(2) AS LONG
  DIM tot(0) AS DOUBLE
  PRINT "|vrp="; OPTIMIZE.VRP_CLARKE_WRIGHT(vd, dem, 3, 3.0, 0, rof, tot); ":"; FORMAT(tot(0), "#.0");

  ' arc routing: ciclo orientato 0->1->2->0 (peso 1)
  DIM w(8) AS DOUBLE
  w(1)=1 : w(5)=1 : w(6)=1
  DIM euler(10) AS LONG
  DIM elen(0) AS LONG
  PRINT "|arc="; FORMAT(OPTIMIZE.ARC_ROUTING(w, 3, euler, elen), "#.0"); ":"; elen(0)
  PRINT ""
END SUB
