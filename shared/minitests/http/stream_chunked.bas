' MiniTest HTTP — risposta in STREAMING (chunked): res.StartStream / Write / Flush /
' EndStream. Verifica ciò che vede il CLIENT (non l'intenzione del server):
'   (a) il corpo arriva completo e in ordine (il client deve saper leggere il chunked);
'   (b) l'header Transfer-Encoding: chunked è presente sulla wire;
'   (c) una risposta normale sulla stessa porta continua a funzionare (nessuna regressione:
'       la modalità streaming non deve "sporcare" le risposte bufferizzate).
' Porta dedicata per non collidere con gli altri minitest HTTP.
' EXPECT: start|status-ok|body=[uno|due]|chunked=chunked|plain=[plain-ok]
IMPORT HTTP

SUB SseHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  ' Streaming: header inviati SUBITO, poi i byte escono mentre l'handler gira.
  res.StartStream("text/event-stream")
  res.Write("uno" + CHR$(10))
  res.Flush()
  res.Write("due" + CHR$(10))
  res.Flush()
  res.EndStream()
END SUB

SUB PlainHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.Send("plain-ok")
END SUB

SUB Main()
  DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
  cfg.Port = 8796
  HTTP CONFIG cfg
  HTTP ROUTE "GET", "/stream", SseHandler()
  HTTP ROUTE "GET", "/plain", PlainHandler()
  PRINT "start"
  HTTP SERVER 8796
  SLEEP 500

  DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS
  opts.Headers.Connection = "close"

  ' (a) corpo completo, in ordine: "uno" poi "due" (il CHR$(10) finale è nell'ultima riga)
  ' NB: `DIM x AS STRING = fr.Body` non è usato qui di proposito — su C99Native quella forma
  ' dichiara la locale `gmb_val_t` (membro boxed) e poi la stampa come stringa (divergenza di
  ' tipizzazione pre-esistente, indipendente dallo streaming). Il corpo si legge inline.
  DIM fr AS FETCHRESULT = FETCH("http://127.0.0.1:8796/stream", opts)
  IF fr.StatusCode = 200 THEN PRINT "status-ok" ELSE PRINT "status-fail="; fr.StatusCode
  DIM b1 AS INTEGER = INSTR(fr.Body, "uno")
  DIM b2 AS INTEGER = INSTR(fr.Body, "due")
  IF b1 > 0 AND b2 > b1 THEN PRINT "body=[uno|due]" ELSE PRINT "body-fail=["; fr.Body; "]"

  ' (b) framing chunked realmente in uso (non Content-Length)
  DIM te AS STRING = fr.Headers.Get("Transfer-Encoding")
  PRINT "chunked="; te

  ' (c) risposta normale dopo una streaming, stessa connessione logica
  PRINT "plain=["; HTTP.GET("http://127.0.0.1:8796/plain"); "]"

  HTTP SERVER STOP
END SUB
