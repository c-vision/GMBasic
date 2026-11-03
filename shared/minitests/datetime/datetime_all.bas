' MiniTest NAMESPACE DATETIME — tutti i 36 simboli della superficie.
'
' Superficie (shared/spec/registry/surface.csv): CDOW, CMONTH, CTOD, DATEADD,
' DATEDIFF, DATEFROMUNIX, DATESERIAL, DATEVALUE, DAY, DAYSINMONTH, DOW, DTOC, DTOS,
' DATE, EOMONTH, FIRSTDAYOFMONTH, HOUR, ISDATE, ISLEAPYEAR, ISOWEEKOFYEAR, MINUTE,
' MONTH, NOW, SECOND, SECONDS, STOD, STRDATE, STRTI, STRTIME, TI, TIMESERIAL,
' TODAY, TIME, UNIXTIMESTAMP, WEEKDAY, YEAR.
'
' Scopo: verificare che i 4 backend supportino la STESSA versione del namespace,
' non un'applicazione. Le funzioni "adesso" (NOW/TODAY/DATE/TIME/TI/SECONDS/STR*)
' sono verificate con predicati o lunghezze, non con il valore esatto.
' EXPECT: ymd= 2020 - 3 - 15|dtos=20200315|stod= 1999|dtoc=03-15-2020|dow= 1 |weekday= 1 |cdow=Sunday|cmonth=March|daysinmonth= 31|leap=-1 | 0|isdate=-1 | 0|dserial= 1999 - 12 - 31|eom= 31|fdom= 1|add= 25|diff= 10|isoweek= 11|hms= 23 : 45 : 30|unixts= 86400|fromunix= 2020 - 3 - 15|serial=-1 |-1 |-1 |-1|secs=-1|strlen= 10 | 8 | 8|ti=-1
IMPORT DATETIME

SUB Main()
  DIM d AS DATETIME
  d = DATETIME.CTOD("2020-03-15")

  ' ── componenti e formati ─────────────────────────────────────────────────
  PRINT "ymd="; YEAR(d); "-"; MONTH(d); "-"; DAY(d)
  PRINT "dtos="; DTOS(d)
  PRINT "stod="; YEAR(DATETIME.STOD("19991231"))
  PRINT "dtoc="; DTOC(d)
  PRINT "dow="; DOW(d); "|weekday="; WEEKDAY(d); "|cdow="; CDOW(d)
  PRINT "cmonth="; CMONTH(d)
  PRINT "daysinmonth="; DAYSINMONTH(d)

  ' ── predicati ────────────────────────────────────────────────────────────
  PRINT "leap="; ISLEAPYEAR(d); "|"; ISLEAPYEAR(DATETIME.DATEVALUE("2021-01-01"))
  PRINT "isdate="; ISDATE("2020-03-15"); "|"; ISDATE("nope")

  ' ── costruzione e aritmetica ─────────────────────────────────────────────
  PRINT "dserial="; YEAR(DATETIME.DATESERIAL(1999, 12, 31)); "-"; MONTH(DATETIME.DATESERIAL(1999, 12, 31)); "-"; DAY(DATETIME.DATESERIAL(1999, 12, 31))
  PRINT "eom="; DAY(DATETIME.EOMONTH(d))
  PRINT "fdom="; DAY(DATETIME.FIRSTDAYOFMONTH(d))
  PRINT "add="; DAY(DATETIME.DATEADD("D", 10, d))
  PRINT "diff="; DATETIME.DATEDIFF("D", DATETIME.DATEVALUE("2000-01-10"), DATETIME.DATEVALUE("2000-01-20"))
  PRINT "isoweek="; ISOWEEKOFYEAR(d)

  ' ── orario ───────────────────────────────────────────────────────────────
  DIM t AS DATETIME
  t = DATETIME.TIMESERIAL(23, 45, 30)
  PRINT "hms="; HOUR(t); ":"; MINUTE(t); ":"; SECOND(t)

  ' ── timestamp ────────────────────────────────────────────────────────────
  PRINT "unixts="; UNIXTIMESTAMP(DATETIME.CTOD("1970-01-02"))
  DIM u AS DATETIME
  u = DATETIME.DATEFROMUNIX(1584230400)
  PRINT "fromunix="; YEAR(u); "-"; MONTH(u); "-"; DAY(u)

  ' ── adesso: predicati/lunghezze (mai il valore esatto) ───────────────────
  PRINT "serial="; (DATE() > 40000); "|"; (TIME() > 40000); "|"; (NOW() > 40000); "|"; (TODAY() > 40000)
  PRINT "secs="; (SECONDS() >= 0 AND SECONDS() < 86400)
  PRINT "strlen="; LEN(STRDATE()); "|"; LEN(STRTIME()); "|"; LEN(STRTI())
  PRINT "ti="; (TI() >= 0)
END SUB
