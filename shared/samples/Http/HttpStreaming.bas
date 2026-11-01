' Esempio completo: risposta HTTP in STREAMING (chunked) — Server-Sent Events.
'
' Mostra la superficie completa dello streaming, la stessa su TUTTE le gambe
' (IL, PCodeVM, VM/C, C99Native):
'   res.StartStream(contentType)  apre la risposta: status + header con Transfer-Encoding: chunked
'   res.Write(testo)              accoda il testo (flush automatico oltre 8 KB)
'   res.Flush()                   manda ORA il blocco al client (senza chiudere)
'   res.EndStream()               chiude il framing (chunk terminale)
'
' Differenza rispetto a res.Send: con res.Send la risposta parte solo alla FINE dell'handler
' (Content-Length). Con lo streaming il client riceve i byte mentre l'handler lavora: è ciò che
' serve per SSE, log in tempo reale, download progressivi, risposte lunghe.
'
' Il programma si autoverifica: avvia il server, consuma lo stream come farebbe un browser
' (FETCH), stampa ciò che ha ricevuto e chiude. Output deterministico su tutte le gambe.
' EXPECT: start|received=[evento 1|evento 2|evento 3]|status-ok|plain-ok

IMPORT HTTP

' ---------------------------------------------------------------------------
' Handler SSE: 3 eventi, uno ogni 150 ms. Ogni evento è un blocco "data:".
' ---------------------------------------------------------------------------
SUB SseHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM i AS INTEGER
  ' Header e status vanno impostati PRIMA di StartStream: appena lo stream parte, la riga di
  ' stato e gli header sono già sul filo (come in ogni API di streaming HTTP).
  res.SetHeader("X-Stream-Demo", "on")
  res.StartStream("text/event-stream")
  FOR i = 1 TO 3
    res.Write("data: evento " + LTRIM$(STR$(i)) + CHR$(10) + CHR$(10))
    res.Flush()
    SLEEP 150
  NEXT i
  res.EndStream()
END SUB

' Risposta NORMALE sulla stessa porta: lo streaming non deve cambiare le altre route.
SUB PlainHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.Send("plain-ok")
END SUB

SUB Main()
  DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
  cfg.Port = 8794
  HTTP CONFIG cfg
  HTTP ROUTE "GET", "/events", SseHandler()
  HTTP ROUTE "GET", "/plain", PlainHandler()

  PRINT "start"
  HTTP SERVER 8794
  SLEEP 400

  ' Consuma lo stream: il client legge i blocchi chunked man mano che arrivano.
  DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS
  opts.Headers.Connection = "close"
  DIM fr AS FETCHRESULT = FETCH("http://127.0.0.1:8794/events", opts)

  IF fr.StatusCode = 200 THEN PRINT "status-ok" ELSE PRINT "status-fail="; fr.StatusCode

  ' Il corpo COMPLETO contiene tutti e tre gli eventi, nell'ordine in cui sono stati scritti.
  DIM n AS INTEGER
  n = 0
  IF INSTR(fr.Body, "evento 1") > 0 THEN n = n + 1
  IF INSTR(fr.Body, "evento 2") > 0 THEN n = n + 1
  IF INSTR(fr.Body, "evento 3") > 0 THEN n = n + 1
  IF n = 3 THEN PRINT "received=[evento 1|evento 2|evento 3]" ELSE PRINT "received-fail=["; fr.Body; "]"

  ' Una risposta bufferizzata convive con lo streaming sulla stessa porta.
  DIM p AS STRING = HTTP.GET("http://127.0.0.1:8794/plain")
  IF p = "plain-ok" THEN PRINT "plain-ok" ELSE PRINT "plain-fail=["; p; "]"

  HTTP SERVER STOP
END SUB
