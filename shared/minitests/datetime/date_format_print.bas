' MiniTest: le funzioni che rendono una DATA (DateTime) si stampano formattate e in JSON
' come ISO-8601, su tutte e quattro le gambe.
' Prima: PCode/VM-C/C99 stampavano il seriale OADate (43832) dove IL dava
' "02/01/2020 00:00:00"; STOD su PCode rendeva una data senza orario (10 caratteri).
' EXPECT: dv=02/01/2020 00:00:00|ctod=02/01/2020 00:00:00|ds=02/01/2020 00:00:00|ts=30/12/1899 10:30:00|da=07/01/2020 00:00:00|eo=31/01/2020 00:00:00|fd=01/01/2020 00:00:00|dfu=01/01/1970 00:00:00|stod=31/12/1999 00:00:00|len= 19|iso="2020-01-02T00:00:00"
IMPORT JSON

SUB Main()
  PRINT "dv="; DATEVALUE("2020-01-02")
  PRINT "ctod="; CTOD("2020-01-02")
  PRINT "ds="; DATESERIAL(2020, 1, 2)
  PRINT "ts="; TIMESERIAL(10, 30, 0)
  PRINT "da="; DATEADD("d", 5, DATESERIAL(2020, 1, 2))
  PRINT "eo="; EOMONTH(DATESERIAL(2020, 1, 15))
  PRINT "fd="; FIRSTDAYOFMONTH(DATESERIAL(2020, 1, 15))
  PRINT "dfu="; DATEFROMUNIX(0)
  PRINT "stod="; STOD("19991231")
  PRINT "len="; LEN(DATEVALUE("2020-01-02"))
  PRINT "iso="; JSON.STRINGIFY(DATESERIAL(2020, 1, 2))
END SUB
