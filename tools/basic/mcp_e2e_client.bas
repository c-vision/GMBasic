' mcp_e2e_client.bas — client MCP JSON-RPC 2.0 su stdio (da mcp_e2e_client.py).
' Uso: mcp_e2e_client.bas <comando> [argomenti...]
IMPORT IO
IMPORT JSON
IMPORT SYSTEM

FUNCTION Q(s AS STRING) AS STRING
  RETURN "'" + REPLACE(s, "'", "'\\''") + "'"
END FUNCTION

FUNCTION Main() AS INTEGER
  SET SHELL ON
  DIM req AS STRING, cmd AS STRING, out AS STRING, line AS STRING
  DIM res$() AS STRING, i AS LONG, n AS LONG, h AS LONG, id AS LONG
  DIM o AS JSON, txt AS STRING, code AS LONG, a AS STRING
  IF COMMANDCOUNT() < 1 THEN
    PRINT "uso: mcp_e2e_client.bas <comando> [argomenti...]"
    RETURN 2
  END IF
  cmd = ""
  i = 1
  DO WHILE i <= COMMANDCOUNT()
    a = Q(COMMAND$(i))
    IF LEN(cmd) > 0 THEN cmd = cmd + " "
    cmd = cmd + a
    i = i + 1
  LOOP
  req = "/tmp/gmb_mcp_e2e_requests.txt"
  h = IO.FCREATE(req)
  IO.FWRITE(h, "{" + CHR$(34) + "jsonrpc" + CHR$(34) + ":" + CHR$(34) + "2.0" + CHR$(34) + "," + CHR$(34) + "id" + CHR$(34) + ":1," + CHR$(34) + "method" + CHR$(34) + ":" + CHR$(34) + "initialize" + CHR$(34) + "," + CHR$(34) + "params" + CHR$(34) + ":{" + CHR$(34) + "protocolVersion" + CHR$(34) + ":" + CHR$(34) + "2024-11-05" + CHR$(34) + "," + CHR$(34) + "capabilities" + CHR$(34) + ":{}," + CHR$(34) + "clientInfo" + CHR$(34) + ":{" + CHR$(34) + "name" + CHR$(34) + ":" + CHR$(34) + "gmbasic-e2e" + CHR$(34) + "}}}" + CHR$(10))
  IO.FWRITE(h, "{" + CHR$(34) + "jsonrpc" + CHR$(34) + ":" + CHR$(34) + "2.0" + CHR$(34) + "," + CHR$(34) + "method" + CHR$(34) + ":" + CHR$(34) + "notifications/initialized" + CHR$(34) + "}" + CHR$(10))
  IO.FWRITE(h, "{" + CHR$(34) + "jsonrpc" + CHR$(34) + ":" + CHR$(34) + "2.0" + CHR$(34) + "," + CHR$(34) + "id" + CHR$(34) + ":2," + CHR$(34) + "method" + CHR$(34) + ":" + CHR$(34) + "tools/list" + CHR$(34) + "}" + CHR$(10))
  IO.FWRITE(h, "{" + CHR$(34) + "jsonrpc" + CHR$(34) + ":" + CHR$(34) + "2.0" + CHR$(34) + "," + CHR$(34) + "id" + CHR$(34) + ":3," + CHR$(34) + "method" + CHR$(34) + ":" + CHR$(34) + "tools/call" + CHR$(34) + "," + CHR$(34) + "params" + CHR$(34) + ":{" + CHR$(34) + "name" + CHR$(34) + ":" + CHR$(34) + "add_cents" + CHR$(34) + "," + CHR$(34) + "arguments" + CHR$(34) + ":{" + CHR$(34) + "amountA" + CHR$(34) + ":2500," + CHR$(34) + "amountB" + CHR$(34) + ":1250}}}" + CHR$(10))
  IO.FWRITE(h, "{" + CHR$(34) + "jsonrpc" + CHR$(34) + ":" + CHR$(34) + "2.0" + CHR$(34) + "," + CHR$(34) + "id" + CHR$(34) + ":4," + CHR$(34) + "method" + CHR$(34) + ":" + CHR$(34) + "tools/call" + CHR$(34) + "," + CHR$(34) + "params" + CHR$(34) + ":{" + CHR$(34) + "name" + CHR$(34) + ":" + CHR$(34) + "total" + CHR$(34) + "," + CHR$(34) + "arguments" + CHR$(34) + ":{" + CHR$(34) + "id" + CHR$(34) + ":7," + CHR$(34) + "cents" + CHR$(34) + ":100}}}" + CHR$(10))
  IO.FWRITE(h, "{" + CHR$(34) + "jsonrpc" + CHR$(34) + ":" + CHR$(34) + "2.0" + CHR$(34) + "," + CHR$(34) + "id" + CHR$(34) + ":5," + CHR$(34) + "method" + CHR$(34) + ":" + CHR$(34) + "tools/call" + CHR$(34) + "," + CHR$(34) + "params" + CHR$(34) + ":{" + CHR$(34) + "name" + CHR$(34) + ":" + CHR$(34) + "mutate" + CHR$(34) + "," + CHR$(34) + "arguments" + CHR$(34) + ":{" + CHR$(34) + "id" + CHR$(34) + ":1}}}" + CHR$(10))
  IO.FWRITE(h, "{" + CHR$(34) + "jsonrpc" + CHR$(34) + ":" + CHR$(34) + "2.0" + CHR$(34) + "," + CHR$(34) + "id" + CHR$(34) + ":6," + CHR$(34) + "method" + CHR$(34) + ":" + CHR$(34) + "tools/call" + CHR$(34) + "," + CHR$(34) + "params" + CHR$(34) + ":{" + CHR$(34) + "name" + CHR$(34) + ":" + CHR$(34) + "nope" + CHR$(34) + "," + CHR$(34) + "arguments" + CHR$(34) + ":{}" + CHR$(34) + "}}}" + CHR$(10))
  IO.FWRITE(h, "{" + CHR$(34) + "jsonrpc" + CHR$(34) + ":" + CHR$(34) + "2.0" + CHR$(34) + "," + CHR$(34) + "id" + CHR$(34) + ":7," + CHR$(34) + "method" + CHR$(34) + ":" + CHR$(34) + "ping" + CHR$(34) + "}" + CHR$(10))
  IO.FCLOSE(h)
  out = SYSTEM.SHELLOUT("cat " + Q(req) + " | " + cmd)
  DIM lines$() AS STRING
  lines$ = SPLIT(out, CHR$(10))
  n = 0
  DIM rs$(500) AS STRING
  i = 0
  DO WHILE i <= UBOUND(lines$)
    line = TRIM$(lines$(i))
    IF LEFT$(line, 1) = "{" THEN
      rs$(n) = line : n = n + 1
    END IF
    i = i + 1
  LOOP
  DIM seen AS INTEGER
  seen = 0
  i = 0
  DO WHILE i < n
    o = JSON.PARSE(rs$(i))
    id = VAL(JSON.EXTRACT(o, "id"))
    IF id = 1 THEN
      IF JSON.EXTRACT(o, "result.protocolVersion") <> "2024-11-05" THEN PRINT "FAIL initialize" : RETURN 1
      IF JSON.EXTRACT(o, "result.serverInfo.name") <> "gmbasic-mcp" THEN PRINT "FAIL serverInfo" : RETURN 1
      PRINT "initialize OK -> "; JSON.EXTRACT(o, "result.serverInfo.name")
      seen = seen + 1
    ELSEIF id = 2 THEN
      txt = JSON.STRINGIFY(o)
      IF INSTR(txt, "add_cents") = 0 OR INSTR(txt, "mutate") = 0 OR INSTR(txt, "total") = 0 THEN PRINT "FAIL tools/list" : RETURN 1
      PRINT "tools/list OK"
      seen = seen + 1
    ELSEIF id = 3 THEN
      txt = JSON.EXTRACT(o, "result.content[0].text")
      IF TRIM$(txt) <> "3750c" THEN PRINT "FAIL add_cents: ["; txt; "]" : RETURN 1
      PRINT "tools/call add_cents -> "; txt
      seen = seen + 1
    ELSEIF id = 4 THEN
      txt = REPLACE(JSON.EXTRACT(o, "result.content[0].text"), " ", "")
      IF txt <> "id=7total=700" THEN PRINT "FAIL total: ["; txt; "]" : RETURN 1
      PRINT "tools/call total -> "; txt
      seen = seen + 1
    ELSEIF id = 5 THEN
      txt = JSON.EXTRACT(o, "error.message")
      IF INSTR(txt, "Read-only server") = 0 THEN PRINT "FAIL read-only: "; txt : RETURN 1
      PRINT "read-only rifiuta il tool mutante -> "; txt
      seen = seen + 1
    ELSEIF id = 6 THEN
      txt = JSON.EXTRACT(o, "error.message")
      IF INSTR(txt, "Unknown tool") = 0 THEN PRINT "FAIL nope: "; txt : RETURN 1
      PRINT "tool ignoto -> "; txt
      seen = seen + 1
    ELSEIF id = 7 THEN
      PRINT "ping OK"
      seen = seen + 1
    END IF
    i = i + 1
  LOOP
  IF seen <> 7 THEN PRINT "FAIL risposte: "; seen; "/7" : RETURN 1
  PRINT "E2E MCP: TUTTO OK"
  RETURN 0
END FUNCTION
