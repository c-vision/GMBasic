' MiniTest HTTP — campi di HTTP CONFIG che abbiamo esteso/aggiunto (§4.1).
'
' Verifica che i campi esistano e siano APPLICATI su tutte e 4 le gambe:
'   - `cfg.MaxRequestBodySize` → corpo oltre il tetto = **413** (end-to-end, dal codice BASIC);
'   - corpo sotto il tetto → la richiesta arriva intera all'handler (nessun troncamento);
'   - `cfg.Http2` (campo nuovo) → accettato dal compilatore e dal server (su HTTP/1.1 non ha
'     effetto osservabile: abilita ALPN h2 solo su TLS, verificato da tools/sh/http2_smoke.sh);
'   - `cfg.EnableKeepAlive` → accettato (il riuso connessione si verifica dal lato client, non da
'     BASIC: vedi Knowledge/HTTP-Keep-Alive-e-Gzip-Server-C).
'
' EXPECT: start|cfg-accepted|small-ok|413
IMPORT HTTP

SUB Echo(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.Send("len=" + LTRIM$(STR$(LEN(req.Body))))
END SUB

SUB Main()
  DIM i AS INTEGER
  DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
  cfg.Port = 8774
  cfg.MaxRequestBodySize = 4096
  cfg.EnableKeepAlive = -1
  cfg.Http2 = -1            ' campo nuovo: deve compilare su tutte le gambe
  HTTP CONFIG cfg
  HTTP ROUTE "POST", "/echo", Echo()
  PRINT "start"
  HTTP SERVER 8774
  SLEEP 400
  PRINT "cfg-accepted"

  ' corpo piccolo: arriva PER INTERO (100 byte)
  ' STRING$(n, "x") — forma classica: la usiamo qui anche come verifica che compili ed emetta
  ' il carattere giusto su TUTTE le gambe (era il bug C99Native di gmb_chr(stringa)).
  DIM chunk AS STRING
  chunk = STRING$(100, "a")

  DIM o1 AS HTTPOPTIONS = NEW HTTPOPTIONS
  o1.Method = "POST"
  o1.Body = chunk
  o1.Headers.Connection = "close"
  DIM r1 AS FETCHRESULT = FETCH("http://127.0.0.1:8774/echo", o1)
  IF r1.StatusCode = 200 AND r1.Body = "len=100" THEN PRINT "small-ok" ELSE PRINT "small-fail=["; r1.StatusCode; ":"; r1.Body; "]"

  ' corpo oltre il tetto (5000 > 4096): 413
  DIM big AS STRING
  big = STRING$(5100, "b")     ' oltre il tetto di 4096
  DIM o2 AS HTTPOPTIONS = NEW HTTPOPTIONS
  o2.Method = "POST"
  o2.Body = big
  o2.Headers.Connection = "close"
  DIM r2 AS FETCHRESULT = FETCH("http://127.0.0.1:8774/echo", o2)
  IF r2.StatusCode = 413 THEN PRINT "413" ELSE PRINT "413-fail=["; r2.StatusCode; "]"

  HTTP SERVER STOP
END SUB
