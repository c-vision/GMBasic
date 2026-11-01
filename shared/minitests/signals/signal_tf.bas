' MiniTest SIGNAL TIMEFRAME — la stessa strategia gira su classi diverse: due
' sottoscrizioni con TIMEFRAME diverso (h1 / T1), handler a 5 parametri che riceve il
' codice esatto che ha generato l'evento; un terzo handler a 4 parametri (senza tf).
' EXPECT: tf=h1=h1|iv|AAPL|tf=T1=T1|iv|AAPL|notf|MSFT|notf|MSFT|done
IMPORT SIGNAL

SUB OnTF(id AS STRING, type AS STRING, symbol AS STRING, value AS DOUBLE, tf AS STRING)
  PRINT id; "="; tf; "|"; type; "|"; symbol
END SUB

SUB OnAny(id AS STRING, type AS STRING, symbol AS STRING, value AS DOUBLE)
  PRINT "notf"; "|"; symbol
END SUB

SUB Main()
  SIGNAL SUBSCRIBE TYPES("iv") TIMEFRAME("h1") AS "tf=h1"
  SIGNAL SUBSCRIBE TYPES("iv") TIMEFRAME("T1") AS "tf=T1"
  SIGNAL SUBSCRIBE TYPES("bar") AS "any"
  ON SIGNAL "tf=h1" CALL OnTF
  ON SIGNAL "tf=T1" CALL OnTF
  ON SIGNAL "any" CALL OnAny

  SIGNAL EMIT "iv", "AAPL", 25.5 TIMEFRAME("h1")
  SLEEP 60
  SIGNAL EMIT "iv", "AAPL", 99 TIMEFRAME("T1")
  SLEEP 60
  SIGNAL EMIT "bar", "MSFT", 1.0 TIMEFRAME("m5")     ' solo "any" (nessun filtro TF)
  SLEEP 60
  SIGNAL EMIT "bar", "MSFT", 2.0 TIMEFRAME("h1")     ' ancora "any": "tf=h1" ha TYPES("iv")

  SLEEP 120
  PRINT "done"
END SUB
