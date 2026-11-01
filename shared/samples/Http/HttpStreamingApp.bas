' ==============================================================================
' Sample completo: HTTP in STREAMING (chunked) — usa TUTTA la superficie
' ==============================================================================
' Ogni endpoint esercita un pezzo diverso della superficie di streaming di HTTPRESPONSE,
' inclusi gli alias, e il programma si AUTOVERIFICA leggendo le risposte come farebbe un
' browser (FETCH) — così l'output è deterministico e identico sulle 4 gambe.
'
' Superficie coperta (guida: Guide/utente/HTTP-Streaming-Guida-Utente.md):
'   res.StartStream(contentType)   apre il flusso (status + header + Transfer-Encoding: chunked)
'   res.Stream(contentType)        alias di StartStream
'   res.StartStreaming(contentType) alias di StartStream
'   res.Write(testo)               accoda nel flusso (avvia da solo se non è aperto)
'   res.Flush()                    manda ORA al client
'   res.EndStream()                chiude il flusso (chunk terminale)
'   res.CloseStream()              alias di EndStream
'   res.Send(testo)                con lo stream aperto si comporta come Write
'   res.SetStatus(n)               PRIMA di StartStream: cambia lo status del flusso
'
' Uso reale: /progress è il caso SSE (log/progress live), /export è il caso "righe man mano"
' (NDJSON/CSV), /created è il caso "lavoro creato" con status 201 e corpo composto.
'
' Come provarlo a mano (server avviato dal sample):
'   curl -N http://127.0.0.1:8775/progress
'   curl -N http://127.0.0.1:8775/export
'   curl -i -X POST http://127.0.0.1:8775/created
'
' EXPECT: start|progress=[ok]|header=[export-42]|export=[ok]|created=[201]|body=[phase1;phase2;done]
IMPORT HTTP

' --- 1) SSE di avanzamento: StartStream + Write + Flush + EndStream -------------------
SUB JobProgress(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM i AS INTEGER
  ' Header e status vanno impostati PRIMA dello stream: appena parte, la testata è sul filo.
  res.SetHeader("X-Job", "export-42")
  res.StartStream("text/event-stream")
  FOR i = 1 TO 5
    res.Write("data: step " + LTRIM$(STR$(i)) + "/5" + CHR$(10) + CHR$(10))
    res.Flush()
  NEXT i
  res.EndStream()
END SUB

' --- 2) export NDJSON: alias Stream(...) + CloseStream() ------------------------------
' NB: la SUB non può chiamarsi `Export` (EXPORT è una keyword del linguaggio).
SUB ExportRows(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM i AS INTEGER
  res.Stream("application/x-ndjson")
  FOR i = 1 TO 3
    res.Write("{" + CHR$(34) + "row" + CHR$(34) + ":" + LTRIM$(STR$(i)) + "}" + CHR$(10))
  NEXT i
  res.CloseStream()
END SUB

' --- 3) lavoro creato: SetStatus(201) + alias StartStreaming + Send dentro lo stream ---
SUB Created(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.SetStatus(201)
  res.StartStreaming("text/plain")
  res.Send("phase1;")        ' con lo stream aperto, Send scrive NEL flusso
  res.Write("phase2")
  res.Flush()
  res.Send(";done")
  res.EndStream()
END SUB

SUB Main()
  DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
  cfg.Port = 8775
  HTTP CONFIG cfg
  HTTP ROUTE "GET", "/progress", JobProgress()
  HTTP ROUTE "GET", "/export", ExportRows()
  HTTP ROUTE "POST", "/created", Created()
  PRINT "start"
  HTTP SERVER 8775
  SLEEP 400

  DIM o AS HTTPOPTIONS = NEW HTTPOPTIONS
  o.Headers.Connection = "close"

  ' (1) SSE: 5 eventi completi, e l'header impostato prima dello stream arriva al client
  DIM p AS FETCHRESULT = FETCH("http://127.0.0.1:8775/progress", o)
  DIM n AS INTEGER
  n = 0
  IF INSTR(p.Body, "step 1/5") > 0 THEN n = n + 1
  IF INSTR(p.Body, "step 3/5") > 0 THEN n = n + 1
  IF INSTR(p.Body, "step 5/5") > 0 THEN n = n + 1
  IF n = 3 AND p.StatusCode = 200 THEN PRINT "progress=[ok]" ELSE PRINT "progress-fail=["; p.StatusCode; ":"; n; "]"
  PRINT "header=["; p.Headers.Get("X-Job"); "]"

  ' (2) export: 3 righe NDJSON, content-type esplicito dello stream
  DIM e AS FETCHRESULT = FETCH("http://127.0.0.1:8775/export", o)
  DIM m AS INTEGER
  m = 0
  IF INSTR(e.Body, CHR$(34) + "row" + CHR$(34) + ":1") > 0 THEN m = m + 1
  IF INSTR(e.Body, CHR$(34) + "row" + CHR$(34) + ":3") > 0 THEN m = m + 1
  IF m = 2 AND e.StatusCode = 200 AND e.Headers.Get("Content-Type") = "application/x-ndjson" THEN
    PRINT "export=[ok]"
  ELSE
    PRINT "export-fail=["; e.StatusCode; ":"; e.Headers.Get("Content-Type"); ":"; m; "]"
  END IF

  ' (3) creato: status 201 dal SetStatus, corpo composto da Send+Write+Send
  '     La route è POST: si chiama con Method=POST (il FETCH semplice è GET → 404).
  DIM oc AS HTTPOPTIONS = NEW HTTPOPTIONS
  oc.Method = "POST"
  oc.Body = "job=42"
  oc.Headers.Connection = "close"
  DIM c AS FETCHRESULT = FETCH("http://127.0.0.1:8775/created", oc)
  IF c.StatusCode = 201 THEN PRINT "created=[201]" ELSE PRINT "created-fail=["; c.StatusCode; "]"
  PRINT "body=["; c.Body; "]"

  HTTP SERVER STOP
END SUB
