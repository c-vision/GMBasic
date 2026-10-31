' MiniTest: JSON.TOOBJECT dentro una FUNCTION che ritorna un UDT (caso RESULT() del live).
' Verifica che i campi dell'UDT siano popolati (non il dict grezzo "{...").
' EXPECT: order= 100001|price= 150.5
IMPORT JSON

TYPE TradeResult
  retcode AS DOUBLE
  order AS DOUBLE
  deal AS DOUBLE
  price AS DOUBLE
END TYPE

FUNCTION TRADEJSON() AS STRING
  TRADEJSON = "{""retcode"":10009,""order"":100001,""deal"":1,""price"":150.5}"
END FUNCTION

FUNCTION RESULT() AS TradeResult
  RESULT = JSON.ToObject(TradeResult, TRADEJSON())
END FUNCTION

SUB Main()
  DIM res AS TradeResult
  res = RESULT()
  PRINT "order="; res.order
  PRINT "price="; res.price
END SUB
