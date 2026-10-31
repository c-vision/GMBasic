' Minitest HTTP — req.ParseForm(): body application/x-www-form-urlencoded → JSON dei campi.
' Copre: parsing form-urlencoded (gmb_http_form_parse, sorgente unica del runtime C, §0bis),
' il metodo req.ParseForm() (IL/PCode/VM-C/C99Native) e l'URL-decode (%20 spazio,
' %40 @). Server + client self-contained col pattern che funziona su TUTTE le gambe (come
' HttpRoutes.bas): il client gira in una goroutine con WAITGROUP e termina con `DONE wg`.
' EXPECT: nome=Mario Rossi; email=mario@test.com; eta=42
IMPORT HTTP
IMPORT JSON
IMPORT CONCURRENCY

SUB FormHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM frm AS JSON
  frm = req.ParseForm()
  res.Send("nome=" + frm.nome + "; email=" + frm.email + "; eta=" + frm.eta)
END SUB

SUB Main()
  DIM cfg AS HTTPCONFIG
  cfg = NEW HTTPCONFIG
  cfg.Port = 8799
  HTTP CONFIG cfg
  HTTP ROUTE "POST", "/form", FormHandler()
  HTTP SERVER 8799
  DIM wg AS WAITGROUP
  GO SUB Client(wg) IN wg
  WAIT wg
  HTTP SERVER STOP
END SUB

SUB Client(wg AS WAITGROUP)
  SLEEP 300
  DIM opts AS HTTPOPTIONS
  opts = NEW HTTPOPTIONS
  opts.Method = "POST"
  opts.Body = "nome=Mario%20Rossi&email=mario%40test.com&eta=42"
  opts.Headers.ContentType = "application/x-www-form-urlencoded"
  opts.Headers.Connection = "close"
  DIM r AS FETCHRESULT
  r = FETCH("http://127.0.0.1:8799/form", opts)
  PRINT r.Body
  DONE wg
END SUB
