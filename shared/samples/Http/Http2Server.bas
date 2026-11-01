' ==============================================================================
' Sample: server HTTP/2 (h2c + ALPN h2 su TLS)
' ==============================================================================
' Dimostra che lo STESSO server parla HTTP/1.1 e HTTP/2:
'   - h2c (cleartext, prior knowledge):  curl --http2-prior-knowledge http://…
'   - h2 su TLS (ALPN):                  curl -k --http2 https://…
'   - fallback automatico a HTTP/1.1 per i client che non offrono h2
'
' Portata (milestone 1): una richiesta attiva alla volta per connessione, risposte
' bufferizzate, HPACK con tabella statica + dinamica + Huffman, Range/304/gzip/br come su
' HTTP/1.1. NON ancora: multiplexing vero, streaming/SSE sopra h2, WebSocket-over-h2.
'
' Come provarlo (server già avviato dal sample):
'   curl -s --http2-prior-knowledge http://127.0.0.1:8782/hello
'   curl -s --http1.1                http://127.0.0.1:8782/hello
'   curl -s --http2-prior-knowledge -H 'Range: bytes=0-9' http://127.0.0.1:8782/static/x.txt
'
' Il programma si autoverifica su HTTP/1.1 (l'unico che il client BASIC sa usare): la
' verifica di HTTP/2 si fa con curl, come documentato sopra e in tools/sh/http2_smoke.sh.
' EXPECT: start|h1-ok|static-ok
IMPORT HTTP

SUB Hello(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.SetHeader("X-Protocol-Demo", "h1-or-h2")
  res.Send("hello from " + req.Path)
END SUB

SUB Main()
  DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
  cfg.Port = 8782
  ' TLS facoltativo: togliere il commento e indicare un PEM (cert+key) o un PFX per
  ' negoziare h2 via ALPN anche su https.
  ' cfg.CertificatePath = "/tmp/both.pem"
  HTTP CONFIG cfg
  HTTP ROUTE "GET", "/hello", Hello()
  HTTP STATIC "/static", "."

  PRINT "start"
  HTTP SERVER 8782
  SLEEP 400

  ' Autoverifica su HTTP/1.1 (client BASIC): stessa route, stesso corpo.
  DIM r AS FETCHRESULT = FETCH("http://127.0.0.1:8782/hello")
  IF r.StatusCode = 200 AND INSTR(r.Body, "hello from /hello") > 0 THEN PRINT "h1-ok" ELSE PRINT "h1-fail=["; r.StatusCode; "]"

  DIM rs AS FETCHRESULT = FETCH("http://127.0.0.1:8782/static/Http2Server.bas")
  IF rs.StatusCode = 200 AND LEN(rs.Body) > 0 THEN PRINT "static-ok" ELSE PRINT "static-fail=["; rs.StatusCode; "]"

  HTTP SERVER STOP
END SUB
