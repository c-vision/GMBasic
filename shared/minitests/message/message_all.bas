' MiniTest NAMESPACE MESSAGE — unica funzione: DELIVERY(bus) → contatori del bus.
' Il bus richiede un broker TCP (CONNECT/PUBLISH); in test automatico si verifica la
' parità di SUPERFICIE (compilazione/link della firma) nel ramo IF 0: il percorso con
' broker reale è coperto da shared/samples/Messaging/MessageDeliveryMonitor.bas.
' EXPECT: superficie=ok
IMPORT MESSAGE

SUB Main()
  PRINT "superficie=ok"
  IF 0 THEN
    DIM b AS LONG
    _x = MESSAGE.DELIVERY(b)
  END IF
END SUB
