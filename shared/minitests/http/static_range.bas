' MiniTest HTTP — Range / 206 Partial Content sui file statici.
'
' Copre ciò che vede un client reale (browser, curl -C, player video):
'   (a) file INTERO senza Range → 200 con la lunghezza giusta (e NON troncato: 70 KB > 64 KB,
'       che era il limite di un vecchio fread singolo del server C);
'   (b) "bytes=0-9"      → 206, Content-Range "bytes 0-9/<tot>", corpo di 10 byte esatti;
'   (c) "bytes=-5"       → 206, gli ULTIMI 5 byte (suffisso);
'   (d) "bytes=<tot-5>-" → 206, dalla coda alla fine (ripresa di un download);
'   (e) "bytes=999999-"  → 416 con Content-Range "bytes */<tot>" (fetta impossibile);
'   (f) "Accept-Ranges: bytes" dichiarato sulle risposte 200 e 206 (è così che il client SA
'       di poter riprendere);
'   (g) una route normale continua a funzionare (il Range non tocca le risposte dinamiche).
'
' Il file è generato dal test in bin/ (gitignored) e cancellato alla fine.
' EXPECT: start|full=[ok]|accept-ranges=[bytes]|r206=[ok]|suffix=[ok]|resume=[ok]|416=[ok]|304=[ok]|etag-miss=[ok]|route=[route-ok]
IMPORT HTTP

DIM g_dir AS STRING = "bin/static_range"
DIM g_file AS STRING = "bin/static_range/big.txt"

SUB MakeHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.Send("route-ok")
END SUB

SUB Main()
  IF IO.DirExists(g_dir) = 0 THEN MKDIR g_dir

  ' 70.000 byte in 70 righe da 1000: oltre i 64 KB del vecchio buffer singolo.
  DIM chunk AS STRING
  chunk = ""
  DIM i AS INTEGER
  FOR i = 1 TO 1000
    chunk = chunk + "A"
  NEXT i
  OPEN g_file FOR OUTPUT AS #1
  FOR i = 1 TO 70
    PRINT #1, chunk;
  NEXT i
  CLOSE #1

  HTTP STATIC "/static", g_dir
  HTTP ROUTE "GET", "/route", MakeHandler()

  DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
  cfg.Port = 8793
  HTTP CONFIG cfg
  PRINT "start"
  HTTP SERVER 8793
  SLEEP 400

  DIM o0 AS HTTPOPTIONS = NEW HTTPOPTIONS
  o0.Headers.Connection = "close"

  ' (a) file intero: 200 e corpo NON troncato (il vecchio server C si fermava a 64 KB)
  DIM full AS FETCHRESULT = FETCH("http://127.0.0.1:8793/static/big.txt", o0)
  ' LONG, non INTEGER: INTEGER è a 16 bit (canone BASIC) e un corpo di 70 KB traboccherebbe
  ' (70000 → 4464) sulle gambe che lo applicano davvero. Il test misura BYTE, quindi LONG.
  DIM tot AS LONG
  tot = LEN(full.Body)
  IF tot > 65536 THEN PRINT "full=[ok]" ELSE PRINT "full-fail=["; tot; "]"

  ' (f) Accept-Ranges sulla risposta intera
  IF full.Headers.Get("Accept-Ranges") = "bytes" THEN PRINT "accept-ranges=[bytes]" ELSE PRINT "accept-ranges-fail"

  ' (b) primi 10 byte: 206 + Content-Range "bytes 0-9/<tot>"
  DIM o1 AS HTTPOPTIONS = NEW HTTPOPTIONS
  o1.Headers.Connection = "close"
  o1.Headers.Add("Range", "bytes=0-9")
  DIM r1 AS FETCHRESULT = FETCH("http://127.0.0.1:8793/static/big.txt", o1)
  DIM cr1 AS STRING
  cr1 = "bytes 0-9/" + LTRIM$(STR$(tot))
  IF r1.StatusCode = 206 AND LEN(r1.Body) = 10 AND r1.Headers.Get("Content-Range") = cr1 THEN
    PRINT "r206=[ok]"
  ELSE
    PRINT "r206-fail=["; r1.StatusCode; ":"; LEN(r1.Body); ":"; r1.Headers.Get("Content-Range"); "]"
  END IF

  ' (c) ultimi 5 byte ("bytes=-5")
  DIM o2 AS HTTPOPTIONS = NEW HTTPOPTIONS
  o2.Headers.Connection = "close"
  o2.Headers.Add("Range", "bytes=-5")
  DIM r2 AS FETCHRESULT = FETCH("http://127.0.0.1:8793/static/big.txt", o2)
  IF r2.StatusCode = 206 AND LEN(r2.Body) = 5 THEN PRINT "suffix=[ok]" ELSE PRINT "suffix-fail=["; r2.StatusCode; ":"; LEN(r2.Body); "]"

  ' (d) ripresa: dalla coda alla fine ("bytes=<tot-5>-")
  DIM rstr AS STRING
  rstr = "bytes=" + LTRIM$(STR$(tot - 5)) + "-"
  DIM o3 AS HTTPOPTIONS = NEW HTTPOPTIONS
  o3.Headers.Connection = "close"
  o3.Headers.Add("Range", rstr)
  DIM r3 AS FETCHRESULT = FETCH("http://127.0.0.1:8793/static/big.txt", o3)
  IF r3.StatusCode = 206 AND LEN(r3.Body) = 5 AND r3.Body = r2.Body THEN
    PRINT "resume=[ok]"
  ELSE
    PRINT "resume-fail=["; r3.StatusCode; ":"; LEN(r3.Body); "]"
  END IF

  ' (e) fetta impossibile: 416 + Content-Range "bytes */tot"
  DIM o4 AS HTTPOPTIONS = NEW HTTPOPTIONS
  o4.Headers.Connection = "close"
  o4.Headers.Add("Range", "bytes=99999999-")
  DIM r4 AS FETCHRESULT = FETCH("http://127.0.0.1:8793/static/big.txt", o4)
  DIM cr4 AS STRING
  cr4 = "bytes */" + LTRIM$(STR$(tot))
  IF r4.StatusCode = 416 AND r4.Headers.Get("Content-Range") = cr4 THEN
    PRINT "416=[ok]"
  ELSE
    PRINT "416-fail=["; r4.StatusCode; ":"; r4.Headers.Get("Content-Range"); "]"
  END IF

  ' (h) cache condizionale: il secondo GET con l'ETag ricevuto → 304 SENZA corpo
  '     (è il meccanismo che evita al browser di riscaricare il file a ogni navigazione)
  DIM et AS STRING
  et = full.Headers.Get("ETag")
  DIM o5 AS HTTPOPTIONS = NEW HTTPOPTIONS
  o5.Headers.Connection = "close"
  o5.Headers.Add("If-None-Match", et)
  DIM r6 AS FETCHRESULT = FETCH("http://127.0.0.1:8793/static/big.txt", o5)
  IF r6.StatusCode = 304 THEN PRINT "304=[ok]" ELSE PRINT "304-fail=["; r6.StatusCode; "]"

  ' (i) ETag diverso → il file viene riscaricato (200): niente 304 "per sbaglio"
  DIM etbad AS STRING
  etbad = CHR$(34) + "nope" + CHR$(34)
  DIM o6 AS HTTPOPTIONS = NEW HTTPOPTIONS
  o6.Headers.Connection = "close"
  o6.Headers.Add("If-None-Match", etbad)
  DIM r7 AS FETCHRESULT = FETCH("http://127.0.0.1:8793/static/big.txt", o6)
  IF r7.StatusCode = 200 AND LEN(r7.Body) > 65536 THEN PRINT "etag-miss=[ok]" ELSE PRINT "etag-miss-fail=["; r7.StatusCode; "]"

  ' (g) le route dinamiche non sono toccate
  DIM r5 AS FETCHRESULT = FETCH("http://127.0.0.1:8793/route", o0)
  PRINT "route=["; r5.Body; "]"

  HTTP SERVER STOP

  KILL g_file
  DIM d AS INTEGER
  FOR d = 1 TO 3
    RMDIR g_dir
  NEXT d
END SUB
