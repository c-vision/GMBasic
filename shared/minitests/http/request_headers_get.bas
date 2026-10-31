' MiniTest HTTP — header di RICHIESTA letti dall'handler: `req.Headers.Get("…")`.
'
' Regressione (2026-10-30). Due bug trovati verificando `http/parseform.bas` sulla gamba C99Native:
'  1) `req.ParseForm()` tornava un dict VUOTO (campi null): `gmb_http_req_parse_form` scriveva i
'     campi in una COPIA del dict (`gmb_val_as_dict` è per valore) — il parse era corretto (n=3
'     con le chiavi giuste) ma il dict tornava vuoto. Corretto: scrittura in-place
'     (`gmb_val_dict_set`).
'  2) `HTTPHEADERS.Get(...)` in una VARIABILE (`DIM hh AS HTTPHEADERS : hh = req.Headers`) non
'     era supportato su C99Native (errore di emissione "simbolo non trovato: 'hh.Get'", e
'     l'assegnazione `hh = req.Headers` passava da gmb_val_to_double distruggendo il dict).
'     Ora entrambe le forme funzionano su 4 gambe: il test copre la VARIABILE e la CONCATENATA.
' Copre: header presente (Content-Type inviato dal client) e header ASSENTE → stringa vuota
' (parità con il Dictionary managed: chiave mancante = "", non "<nil>").
' EXPECT: ct=application/x-www-form-urlencoded|ct2=application/x-www-form-urlencoded|missing=[]|ok=1
IMPORT HTTP

SUB OnHdr(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM hh AS HTTPHEADERS
  DIM ct AS STRING, ct2 AS STRING, manca AS STRING
  hh = req.Headers
  ct = hh.Get("Content-Type")               ' forma VARIABILE (era rotta su C99Native)
  ct2 = req.Headers.Get("Content-Type")     ' forma CONCATENATA (già supportata)
  manca = hh.Get("X-MT-Assente")            ' chiave assente → "" (non "<nil>")
  res.Send("ct=" + ct + "|ct2=" + ct2 + "|missing=[" + manca + "]")
END SUB

SUB Main()
  DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
  cfg.Port = 8815
  HTTP CONFIG cfg
  HTTP ROUTE "POST", "/h", OnHdr()
  HTTP SERVER 8815
  SLEEP 800

  DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS
  opts.Method = "POST"
  opts.Body = "a=1"
  opts.Headers.ContentType = "application/x-www-form-urlencoded"
  opts.Headers.Connection = "close"
  DIM r AS FETCHRESULT = FETCH("http://127.0.0.1:8815/h", opts)
  PRINT r.Body
  IF r.IsSuccess THEN PRINT "ok=1" ELSE PRINT "ok=0"
  HTTP SERVER STOP
END SUB
