' Minitest TBROWSE sort/filter — motore sort/filter esposto via CONTROL SET/GET (§12.4), 4 backend.
' EXPECT: tbsort-ok
IMPORT CONTROLS
SCREEN 0

SUB Main()
  DIM hd(1) AS STRING
  hd(0) = "Nome" : hd(1) = "Val"
  DIM dt(2, 1) AS STRING
  dt(0, 0) = "b" : dt(0, 1) = "2"
  dt(1, 0) = "a" : dt(1, 1) = "1"
  dt(2, 0) = "c" : dt(2, 1) = "3"
  DIM sel AS INTEGER
  TBROWSE 2, 2, 30, 6, hd, dt INTO sel NAME "tbf"
  CONTROL SET "tbf", "sort", 0, ""
  CONTROL SET "tbf", "sortdir", 1, ""
  DIM sc AS INTEGER
  DIM sd AS INTEGER
  CONTROL GET "tbf", "sort" INTO sc
  CONTROL GET "tbf", "sortdir" INTO sd
  CONTROL SET "tbf", "filter", 0, "a"
  DIM fs AS STRING
  CONTROL GETS "tbf", "filter" INTO fs
  IF sc = 0 AND sd = 1 AND fs = "a" THEN
    PRINT "tbsort-ok"
  ELSE
    PRINT "tbsort-no " + STR(sc) + "|" + STR(sd) + "|" + fs
  END IF
END SUB
