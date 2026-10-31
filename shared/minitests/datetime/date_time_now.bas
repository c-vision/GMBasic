' EXPECT: date-ok|time-ok|now-ok
IMPORT DATETIME
SUB Main()
  IF DATETIME.DATE() > 0 THEN PRINT "date-ok"
  IF DATETIME.TIME() >= 0 THEN PRINT "time-ok"
  IF DATETIME.NOW() > 0 THEN PRINT "now-ok"
END SUB
