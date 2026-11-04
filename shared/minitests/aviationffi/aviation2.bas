' OPTIMIZE (FFI aviation) — MiniTest §4.1 della 2ª fascia:
' AVFLEET, AVCREWPAIRING, AVRECOVERY, AVCREWRESCHED, AVHUB, AVTAILCONTINUITY.
' EXPECT: fleet=35.0: 1  0 |pair=11.0: 2 |recov=0.0: 0  0 |resched=2.0: 0  1 |hub=20.0: 2  1 |tail=2.0: 0  1
IMPORT OPTIMIZE

SUB Main()
  ' ── Fleet assignment: 2 flotte x 2 rotte ──────────────────────────────────────────
  DIM prof(3) AS DOUBLE
  DIM cap(1) AS DOUBLE
  DIM dem(1) AS DOUBLE
  DIM av(1) AS LONG
  DIM asg(1) AS LONG
  prof(0)=10 : prof(1)=20
  prof(2)=15 : prof(3)=5
  cap(0)=100 : cap(1)=100
  dem(0)=50 : dem(1)=60
  av(0)=1 : av(1)=1
  PRINT "fleet="; FORMAT(OPTIMIZE.AVFLEET(prof, cap, dem, av, 2, 2, asg), "#.0"); ":";
  PRINT asg(0); asg(1);

  ' ── Crew pairing: 3 voli, 2 equipaggi (pairing0 = voli 0-1, pairing1 = 1-2) ───────
  DIM voli(5) AS LONG
  DIM ccost(1) AS DOUBLE
  DIM ch(1) AS LONG
  DIM nch(0) AS LONG
  voli(0)=1 : voli(1)=0
  voli(2)=1 : voli(3)=1
  voli(4)=0 : voli(5)=1
  ccost(0)=5 : ccost(1)=6
  PRINT "|pair="; FORMAT(OPTIMIZE.AVCREWPAIRING(voli, ccost, 3, 2, ch, nch), "#.0"); ":";
  PRINT nch(0);

  ' ── Recovery: 2 voli x 2 opzioni di ritardo ──────────────────────────────────────
  DIM rcost(3) AS DOUBLE
  DIM rasg(1) AS LONG
  rcost(0)=10 : rcost(1)=50
  rcost(2)=30 : rcost(3)=5
  PRINT "|recov="; FORMAT(OPTIMIZE.AVRECOVERY(rcost, 2, 2, rasg), "#.0"); ":";
  PRINT rasg(0); rasg(1);

  ' ── Crew rescheduling: 2 equipaggi x 2 voli ──────────────────────────────────────
  DIM scost(3) AS DOUBLE
  DIM savail(3) AS LONG
  DIM sasg(1) AS LONG
  scost(0)=1 : scost(1)=9
  scost(2)=9 : scost(3)=1
  savail(0)=1 : savail(1)=1 : savail(2)=1 : savail(3)=1
  PRINT "|resched="; FORMAT(OPTIMIZE.AVCREWRESCHED(scost, savail, 2, 2, sasg), "#.0"); ":";
  PRINT sasg(0); sasg(1);

  ' ── Hub location: 4 città in linea, p=2 hub, alpha=0.5 ───────────────────────────
  DIM hd(15) AS DOUBLE
  DIM fl(15) AS DOUBLE
  DIM hubs(1) AS LONG
  FOR i = 0 TO 3
    FOR j = 0 TO 3
      hd(i*4+j) = ABS(i-j)
      fl(i*4+j) = 1
    NEXT j
  NEXT i
  PRINT "|hub="; FORMAT(OPTIMIZE.AVHUB(hd, fl, 4, 2, 0.5, hubs), "#.0"); ":";
  PRINT hubs(0); hubs(1);

  ' ── Tail continuity: 2 aeromobili x 2 voli ───────────────────────────────────────
  DIM tcost(3) AS DOUBLE
  DIM tasg(1) AS LONG
  tcost(0)=1 : tcost(1)=5
  tcost(2)=5 : tcost(3)=1
  PRINT "|tail="; FORMAT(OPTIMIZE.AVTAILCONTINUITY(tcost, 2, 2, tasg), "#.0"); ":";
  PRINT tasg(0); tasg(1)
  PRINT ""
END SUB
