' SignalDemo.bas — SIGNAL (namespace SIGNAL): segnali con filtro strutturato + EMIT locale.
' Endpoint implicito (bus locale). Filtri: TYPES/ASSET/MARKET/SYMBOLS; handler (id,type,symbol,value).
IMPORT SIGNAL

SUB OnIV(id AS STRING, type AS STRING, symbol AS STRING, value AS DOUBLE)
  PRINT "[iv] "; symbol; " "; type; " = "; value
END SUB
SUB OnUnusual(id AS STRING, type AS STRING, symbol AS STRING, value AS DOUBLE)
  PRINT "[unusual] "; symbol; " = "; value
END SUB

SUB Main()
  SIGNAL SUBSCRIBE TYPES("iv_rank_high","iv_rank_low") ASSET("options") SYMBOLS("AAPL","MSFT") AS "iv"
  SIGNAL SUBSCRIBE TYPES("unusual") AS "un"
  ON SIGNAL "iv" CALL OnIV
  ON SIGNAL "un" CALL OnUnusual

  ' SLEEP fra gli EMIT: la consegna è un task GPM per sottoscrizione, l'ordine di stampa
  ' fra eventi diversi non è garantito senza un punto di sincronizzazione (parità fra gambe).
  SIGNAL EMIT "iv_rank_high", "AAPL", 85, ASSET("options")
  SLEEP 60
  SIGNAL EMIT "unusual", "TSLA", 12, ASSET("options")
  SLEEP 60
  SIGNAL EMIT "iv_rank_high", "EUR/USD", 30, ASSET("fx")   ' symbol no -> non consegnato
  SLEEP 120

  SIGNAL UNSUBSCRIBE
  SIGNAL PING
  PRINT "done"
END SUB
