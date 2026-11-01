' MiniTest HTTP — TUTTI i metodi di streaming di HTTPRESPONSE (superficie §4.1).
'
' Copre la superficie completa introdotta per lo streaming, con i suoi alias e i casi limite,
' e verifica che le 4 gambe (IL, PCode, VM/C, C99Native) diano lo STESSO risultato:
'   (1) StartStream + Write + Flush + EndStream           → corpo completo, framing chunked
'   (2) alias: Stream(...) + CloseStream()                → stesso risultato di (1)
'   (3) alias: StartStreaming(...) + Write dopo EndStream → ignorata (stream chiuso)
'   (4) Write() SENZA StartStream (avvio implicito)       → corpo presente, content-type default
'   (5) Send() con lo stream APERTO                       → il testo va nel flusso, non perso
'   (6) SetStatus prima di StartStream                    → lo status arriva al client
'   (7) StartStream due volte (idempotente)               → un solo corpo, nessun errore
'   (8) Flush senza dati in coda (no-op)                  → nessun byte spurious
'   (9) EndStream senza stream aperto (no-op)             → una risposta bufferizzata resta valida
'  (10) stream che poi chiama SendJSON                    → JSON nel flusso (o body bufferizzato),
'                                                           identico su tutte le gambe
'
' Il client è FETCH: vede il corpo concatenato (il chunked lo decodifica il client HTTP).
' Porta dedicata. EXPECT: i token sotto, nell'ordine.
' EXPECT: start|v1=[ABC]|v2=[XYZ]|v3=[12]|v4=[implicit]|v5=[S1S2]|v6=201|v7=[Z]|v8=[Q]|v9=[buffered]|v10=[{"k":1}]
IMPORT HTTP

SUB H1(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.StartStream("text/plain")
  res.Write("A")
  res.Write("B")
  res.Flush()
  res.Write("C")
  res.EndStream()
END SUB

SUB H2(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.Stream("text/plain")
  res.Write("X")
  res.Write("Y")
  res.Flush()
  res.Write("Z")
  res.CloseStream()
END SUB

SUB H3(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.StartStreaming("text/plain")
  res.Write("1")
  res.Write("2")
  res.EndStream()
  res.Write("3")   ' dopo la chiusura: ignorato (nessun errore, nessuno stream riaperto)
END SUB

SUB H4(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.Write("implicit")   ' nessun StartStream: parte con content-type default
  res.Flush()
  res.EndStream()
END SUB

SUB H5(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.StartStream("text/plain")
  res.Write("S1")
  res.Send("S2")          ' con lo stream aperto Send = Write
  res.EndStream()
END SUB

SUB H6(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.SetStatus(201)
  res.StartStream("text/plain")
  res.Write("created")
  res.EndStream()
END SUB

SUB H7(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.StartStream("text/plain")
  res.StartStream("application/json")   ' seconda chiamata: no-op
  res.Write("Z")
  res.EndStream()
END SUB

SUB H8(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.StartStream("text/plain")
  res.Flush()                            ' niente in coda: no-op
  res.Write("Q")
  res.Flush()
  res.EndStream()
END SUB

SUB H9(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.EndStream()                        ' nessuno stream aperto: no-op
  res.Send("buffered")                   ' risposta normale
END SUB

SUB H10(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM d AS DICT OF STRING, INTEGER
  d("k") = 1
  res.StartStream("application/json")
  res.Write(JSON.STRINGIFY(d))
  res.EndStream()
END SUB

SUB Main()
  DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
  cfg.Port = 8776
  HTTP CONFIG cfg
  HTTP ROUTE "GET", "/v1", H1()
  HTTP ROUTE "GET", "/v2", H2()
  HTTP ROUTE "GET", "/v3", H3()
  HTTP ROUTE "GET", "/v4", H4()
  HTTP ROUTE "GET", "/v5", H5()
  HTTP ROUTE "GET", "/v6", H6()
  HTTP ROUTE "GET", "/v7", H7()
  HTTP ROUTE "GET", "/v8", H8()
  HTTP ROUTE "GET", "/v9", H9()
  HTTP ROUTE "GET", "/v10", H10()
  PRINT "start"
  HTTP SERVER 8776
  SLEEP 500

  DIM o AS HTTPOPTIONS = NEW HTTPOPTIONS
  o.Headers.Connection = "close"

  DIM r1 AS FETCHRESULT = FETCH("http://127.0.0.1:8776/v1", o)
  PRINT "v1=["; r1.Body; "]"

  DIM r2 AS FETCHRESULT = FETCH("http://127.0.0.1:8776/v2", o)
  PRINT "v2=["; r2.Body; "]"

  DIM r3 AS FETCHRESULT = FETCH("http://127.0.0.1:8776/v3", o)
  PRINT "v3=["; r3.Body; "]"

  DIM r4 AS FETCHRESULT = FETCH("http://127.0.0.1:8776/v4", o)
  PRINT "v4=["; r4.Body; "]"

  DIM r5 AS FETCHRESULT = FETCH("http://127.0.0.1:8776/v5", o)
  PRINT "v5=["; r5.Body; "]"

  DIM r6 AS FETCHRESULT = FETCH("http://127.0.0.1:8776/v6", o)
  IF r6.StatusCode = 201 THEN PRINT "v6=201" ELSE PRINT "v6-fail=["; r6.StatusCode; "]"

  DIM r7 AS FETCHRESULT = FETCH("http://127.0.0.1:8776/v7", o)
  PRINT "v7=["; r7.Body; "]"

  DIM r8 AS FETCHRESULT = FETCH("http://127.0.0.1:8776/v8", o)
  PRINT "v8=["; r8.Body; "]"

  DIM r9 AS FETCHRESULT = FETCH("http://127.0.0.1:8776/v9", o)
  PRINT "v9=["; r9.Body; "]"

  DIM r10 AS FETCHRESULT = FETCH("http://127.0.0.1:8776/v10", o)
  PRINT "v10=["; r10.Body; "]"

  HTTP SERVER STOP
END SUB
