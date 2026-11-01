' MiniTest HTTP — HEAD su una route che usa res.SendFile.
'
' Verifica:
'   (a) HEAD risponde 200 (una route registrata GET serve anche HEAD: "GET senza corpo",
'       RFC 9110 §9.3.2 — prima cadeva in 404);
'   (b) la risposta dichiara la lunghezza del corpo che una GET avrebbe mandato;
'   (c) il corpo NON viaggia (un HEAD non deve scaricare il file solo per scartarlo);
'   (d) la stessa route in GET manda il corpo completo (il test non è "verde" per caso).
'
' Nota: il client è FETCH con opts.Method = "HEAD" (non un socket grezzo: sulla gamba managed
' un TCP RECV bloccante occuperebbe il worker GPM e il server in-process non risponderebbe mai).
' Il file è creato dal test in /tmp e cancellato alla fine. Porta dedicata.
' EXPECT: start|head-status|head-length|no-body|get-ok
IMPORT HTTP

SUB Dl(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.SendFile("/tmp/gmb_mt_head_sendfile.dat")
END SUB

SUB Main()
  OPEN "/tmp/gmb_mt_head_sendfile.dat" FOR WRITE AS #1
  PRINT #1, "HEADBODY-0123456789";
  CLOSE #1

  DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
  cfg.Port = 8785
  HTTP CONFIG cfg
  HTTP ROUTE "GET", "/dl", Dl()
  PRINT "start"
  HTTP SERVER 8785
  SLEEP 400

  DIM oh AS HTTPOPTIONS = NEW HTTPOPTIONS
  oh.Method = "HEAD"
  oh.Headers.Connection = "close"
  DIM fh AS FETCHRESULT = FETCH("http://127.0.0.1:8785/dl", oh)

  IF fh.StatusCode = 200 THEN PRINT "head-status" ELSE PRINT "head-status-fail=["; fh.StatusCode; "]"
  ' La lunghezza dichiarata si legge dall'header: FETCHRESULT.ContentLength è il numero di byte
  ' RICEVUTI (0 per un HEAD, che non ne riceve) — l'header è la verità sulla wire.
  DIM cl AS STRING
  cl = fh.Headers.Get("Content-Length")
  IF LEN(cl) > 0 AND cl <> "0" THEN PRINT "head-length" ELSE PRINT "head-length-fail=["; cl; "]"
  IF LEN(fh.Body) = 0 THEN PRINT "no-body" ELSE PRINT "body-leaked=["; LEN(fh.Body); "]"

  DIM og AS HTTPOPTIONS = NEW HTTPOPTIONS
  og.Headers.Connection = "close"
  DIM fg AS FETCHRESULT = FETCH("http://127.0.0.1:8785/dl", og)
  IF INSTR(fg.Body, "HEADBODY") > 0 THEN PRINT "get-ok" ELSE PRINT "get-fail=["; fg.StatusCode; ":"; LEN(fg.Body); "]"

  HTTP SERVER STOP
  KILL "/tmp/gmb_mt_head_sendfile.dat"
END SUB
