' OPTIMIZE (FFI or) — scheduling: macchina singola, intervalli, flow shop, progetto,
' job shop, timetabling, rostering.
' EXPECT: spt=10.0|edd=2.000|moore= 1 |imax= 2 |ipart= 2 |johnson=4.0|lpt=3.0|cpm=5.0|tt= 3 |rost= 0 
IMPORT OPTIMIZE

SUB Main()
  DIM p() = {3, 1, 2}
  DIM due() = {3, 2, 4}
  DIM ord(2) AS LONG
  PRINT "spt="; FORMAT(OPTIMIZE.SCHED_SPT(p, 3, ord), "#.0");
  PRINT "|edd="; FORMAT(OPTIMIZE.SCHED_EDD(p, due, 3, ord), "#.000");
  PRINT "|moore="; OPTIMIZE.SCHED_MOORE(p, due, 3, ord);

  ' intervalli [0,1], [1,2], [0,2] -> max 2 non sovrapposti, 2 risorse
  DIM st() = {0, 1, 0}
  DIM en() = {1, 2, 2}
  DIM sel(2) AS LONG
  PRINT "|imax="; OPTIMIZE.INTERVAL_MAX(st, en, 3, sel);
  PRINT "|ipart="; OPTIMIZE.INTERVAL_PARTITION(st, en, 3);

  ' Johnson 2 macchine: p1={2,1}, p2={1,2}
  DIM p1() = {2, 1}
  DIM p2() = {1, 2}
  DIM seq(1) AS LONG
  PRINT "|johnson="; FORMAT(OPTIMIZE.FLOWSHOP_JOHNSON(p1, p2, 2, seq), "#.0");

  ' PARALLEL_LPT: p={1,2,3}, m=2 -> Cmax 3
  DIM lp() = {1, 2, 3}
  DIM asg(2) AS LONG
  PRINT "|lpt="; FORMAT(OPTIMIZE.PARALLEL_LPT(lp, 3, 2, asg), "#.0");

  ' PROJECT_CPM: 3 task, dur {1,2,3}; task2 dipende da 0 e 1 -> progetto 5
  DIM dur() = {1, 2, 3}
  DIM preds(5) AS LONG
  FOR i = 0 TO 5
    preds(i) = -1
  NEXT i
  preds(4) = 0
  preds(5) = 1
  DIM est(2) AS DOUBLE
  DIM lst(2) AS DOUBLE
  DIM slack(2) AS DOUBLE
  PRINT "|cpm="; FORMAT(OPTIMIZE.PROJECT_CPM(dur, preds, 3, 2, est, lst, slack), "#.0");

  ' timetabling: triangolo di conflitti -> 3 slot
  DIM conf(8) AS LONG
  conf(1)=1 : conf(2)=1
  conf(3)=1 : conf(5)=1
  conf(6)=1 : conf(7)=1
  DIM slot(2) AS LONG
  PRINT "|tt="; OPTIMIZE.TIMETABLING(conf, 3, slot);

  ' rostering: 2 infermieri, 2 giorni, richiesta 1/giorno, max 1 consecutivo -> 0 scoperti
  DIM rd(1) AS LONG
  rd(0)=1 : rd(1)=1
  DIM sched(3) AS LONG
  PRINT "|rost="; OPTIMIZE.ROSTERING(rd, 2, 2, 1, sched)
  PRINT ""
END SUB
