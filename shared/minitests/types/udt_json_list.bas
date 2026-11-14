' udt_json_list.bas — UDT con campo LIST OF UDT + STRING var-length: JSON round-trip (4 backend).
' EXPECT: {"strategy":"bull","net_mid":1.5,"legs":[{"side":"buy","symbol":"AAPL.P335","qty":2}]}|bull  1.5   1  AAPL.P335  2
TYPE Leg
  side AS STRING
  symbol AS STRING
  qty AS DOUBLE
END TYPE
TYPE Ticket
  strategy AS STRING
  net_mid AS DOUBLE
  legs AS LIST OF Leg
END TYPE
SUB Main()
  DIM t AS Ticket
  DIM lst AS LIST OF Leg
  DIM a AS Leg
  a.side = "buy"
  a.symbol = "AAPL.P335"
  a.qty = 2
  lst.Add(a)
  t.legs = lst
  t.strategy = "bull"
  t.net_mid = 1.5
  DIM js AS STRING
  js = JSON.ToJson(t)
  PRINT js
  DIM u AS Ticket
  u = JSON.ToObject(Ticket, js)
  PRINT u.strategy; " "; u.net_mid; " "; u.legs.Count; " "; u.legs(0).symbol; " "; u.legs(0).qty
END SUB
