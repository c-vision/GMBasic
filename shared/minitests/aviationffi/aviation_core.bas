' OPTIMIZE (FFI aviation) — revenue, assegnazioni, ULD, routing, Steiner, column generation.
' EXPECT: emsr=-1 |ovb=-1 |bid=-1 |tail=5.0|slot=7.0|uld= 1 |route= 1 |steiner=2.0|colgen=2.0
IMPORT OPTIMIZE

SUB Main()
  ' EMSR-b: 2 classi, capacità 20
  DIM fares(1) AS DOUBLE
  fares(0)=100 : fares(1)=50
  DIM dmean(1) AS DOUBLE
  dmean(0)=10 : dmean(1)=20
  DIM dstd(1) AS DOUBLE
  dstd(0)=2 : dstd(1)=4
  DIM prot(1) AS DOUBLE
  PRINT "emsr="; (OPTIMIZE.AVEMSRB(fares, dmean, dstd, 2, 20.0, prot) >= 0);

  DIM bk(0) AS DOUBLE
  PRINT "|ovb="; (OPTIMIZE.AVOVERBOOKING(100.0, 200.0, 400.0, 0.9, 5.0, bk) > 0);
  PRINT "|bid="; (OPTIMIZE.AVBIDPRICE(100.0, 80.0, 100.0, 300.0) > 0);

  ' tail assignment 2x2 (Hungarian) -> 5
  DIM tc(3) AS DOUBLE
  tc(0)=1 : tc(1)=2
  tc(2)=3 : tc(3)=4
  DIM asg(1) AS LONG
  PRINT "|tail="; FORMAT(OPTIMIZE.AVTAILASSIGNMENT(tc, 2, 2, asg), "#.0");

  ' slot allocation 2x2 -> 7
  DIM bid(3) AS DOUBLE
  bid(0)=3 : bid(1)=1
  bid(2)=2 : bid(3)=4
  DIM alloc(1) AS LONG
  PRINT "|slot="; FORMAT(OPTIMIZE.AVSLOT(bid, 2, 2, alloc), "#.0");

  ' ULD: 2 colli 1x1x1 in 10x10x10 -> 1
  DIM items(5) AS DOUBLE
  FOR i = 0 TO 5
    items(i)=1
  NEXT i
  DIM uld(1) AS LONG
  PRINT "|uld="; OPTIMIZE.AVULD(items, 2, 10.0, 10.0, 10.0, uld);

  ' aircraft routing: f0 (1->2), f1 (2->3) -> 1 rotta
  DIM fl(7) AS DOUBLE
  fl(0)=0 : fl(1)=60 : fl(2)=1 : fl(3)=2
  fl(4)=100 : fl(5)=160 : fl(6)=2 : fl(7)=3
  DIM rof(1) AS LONG
  PRINT "|route="; OPTIMIZE.AVAIRCRAFTROUTING(fl, 2, 30.0, rof);

  ' Steiner su linea 0-1-2, terminali {0,2} -> costo 2
  DIM INF = 1000000000000000000000000000000.0
  DIM w(8) AS DOUBLE
  FOR i = 0 TO 8
    w(i)=INF
  NEXT i
  w(0)=0 : w(1)=1 : w(3)=1 : w(4)=0 : w(5)=1 : w(7)=1 : w(8)=0
  DIM term(1) AS LONG
  term(0)=0 : term(1)=2
  DIM itree(8) AS LONG
  PRINT "|steiner="; FORMAT(OPTIMIZE.AVSTEINER(w, 3, term, 2, itree), "#.0");

  ' column generation: 2 voli, 2 pairings (uno per volo), costo 2
  DIM voli(3) AS LONG
  voli(0)=1 : voli(1)=0
  voli(2)=0 : voli(3)=1
  DIM pcost(1) AS DOUBLE
  pcost(0)=1 : pcost(1)=1
  DIM chosen(1) AS LONG
  DIM nch(0) AS LONG
  PRINT "|colgen="; FORMAT(OPTIMIZE.AVCOLUMNGEN(voli, pcost, 2, 2, chosen, nch, 20), "#.0")
  PRINT ""
END SUB
