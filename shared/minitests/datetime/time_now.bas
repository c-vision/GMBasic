' EXPECT: strtime-ok|seconds-ok|ti-ok
IMPORT DATETIME
SUB Main()
  IF LEN(DATETIME.STRTIME()) > 0 THEN PRINT "strtime-ok"
  DIM s = DATETIME.SECONDS()
  IF s >= 0 AND s < 86400 THEN PRINT "seconds-ok"
  IF DATETIME.TI() > 0 THEN PRINT "ti-ok"
END SUB
