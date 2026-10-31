' Minitest MESSAGE — isolamento fra bus LOCALI (cross-bus).
' Due `MESSAGE CONNECT "local"` sono due bus distinti: una PUBLISH sul bus B NON deve
' consegnare alla SUB aperta sul bus A (IL/PCode filtrano per bus, C99 per id del bus nel
' runtime condiviso). Prima la VM consegnava a TUTTI i bus locali (il fan-out iterava
' vmsg_id/vmsg_handler senza guardare il bus) → "delivered" solo su VM/C.
' Nessun broker, nessuna rete: determinismo totale.
' EXPECT: timeout
IMPORT MESSAGE
DIM got AS LONG = 0
SUB OnMsg(id AS STRING, topic AS STRING, payload AS STRING)
  PRINT "RX "; id; " "; topic; " "; payload
  got = 1
END SUB
SUB Main()
  DIM a AS LONG
  DIM b AS LONG
  MESSAGE CONNECT "local" AS a
  MESSAGE CONNECT "local" AS b
  MESSAGE SUBSCRIBE a TOPIC("t") AS "s"
  ON MESSAGE "s" CALL OnMsg
  MESSAGE PUBLISH b TOPIC("t") DATA("hello")
  SLEEP 60
  IF got = 1 THEN PRINT "delivered" ELSE PRINT "timeout"
  MESSAGE CLOSE b
  MESSAGE CLOSE a
END SUB
