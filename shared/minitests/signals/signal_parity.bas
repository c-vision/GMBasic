' MiniTest SIGNAL — ciclo completo: SUBSCRIBE (filtro strutturato) / ON SIGNAL / EMIT /
' UNSUBSCRIBE / PING. Parità 4 backend (IL, PCode, VM/C, C99Native).
' Il filtro è un AND: TYPES + SYMBOLS + ASSET. L'evento con simbolo non in lista NON
' arriva; dopo UNSUBSCRIBE non arriva più nulla.
' EXPECT: iv|AAPL|iv_rank_high|un|TSLA|done
IMPORT SIGNAL

SUB OnIV(id AS STRING, type AS STRING, symbol AS STRING, value AS DOUBLE)
  PRINT "iv"; "|"; symbol; "|"; type
END SUB

SUB OnUnusual(id AS STRING, type AS STRING, symbol AS STRING, value AS DOUBLE)
  PRINT "un"; "|"; symbol
END SUB

SUB Main()
  SIGNAL SUBSCRIBE TYPES("iv_rank_high","iv_rank_low") ASSET("options") SYMBOLS("AAPL","MSFT") AS "iv"
  SIGNAL SUBSCRIBE TYPES("unusual") AS "un"
  ON SIGNAL "iv" CALL OnIV
  ON SIGNAL "un" CALL OnUnusual

  SIGNAL EMIT "iv_rank_high", "AAPL", 85, ASSET("options")     ' consegnato a "iv"
  SLEEP 60
  SIGNAL EMIT "unusual", "TSLA", 12                            ' consegnato a "un"
  SLEEP 60
  SIGNAL EMIT "iv_rank_high", "EUR/USD", 30, ASSET("options")  ' simbolo fuori filtro: scartato
  SIGNAL EMIT "iv_rank_high", "AAPL", 40, ASSET("fx")          ' asset fuori filtro: scartato

  SLEEP 120

  SIGNAL UNSUBSCRIBE
  SIGNAL EMIT "iv_rank_high", "AAPL", 99, ASSET("options")     ' nessun iscritto: scartato
  SIGNAL PING
  SLEEP 100
  PRINT "done"
END SUB
