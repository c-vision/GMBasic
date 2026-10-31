' MiniTest HTTP — parità 4 backend per FetchResult.Headers.Get("nome").
' Regressione (Headers-Get-Divergenza-Backend): su C99Native `DIM fr AS FETCHRESULT =
' FETCH(...)` NON era tipato boxed quanto bastava, quindi `fr.Headers.Get("k")` restava
' TY_VAL e l'assegnazione a STRING ri-avvolgeva l'accessor (già stringa) in un secondo
' gmb_val_to_string → doppio wrap "passing gmb_string_t to gmb_val_t" (cc error).
' Copre: (a) header di RISPOSTA presente; (b) header assente (stringa vuota);
' (c) forma DIM-con-init e forma DIM+assegnazione separata.
' Path/ porta dedicati per non collidere con gli altri minitest HTTP.
' EXPECT: hdr-ok|hdr-missing|hdr2-ok
IMPORT HTTP

SUB OnHdr(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.SetHeader("X-MT-Header", "hdr-value-42")
  res.Send("body")
END SUB

SUB Main()
  DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
  cfg.Port = 8799
  HTTP CONFIG cfg
  HTTP ROUTE "GET", "/hdr", OnHdr()
  HTTP SERVER 8799
  SLEEP 800

  DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS
  opts.Headers.Connection = "close"

  ' (a) forma DIM con inizializzatore
  DIM fb AS FETCHRESULT = FETCH("http://localhost:8799/hdr", opts)
  DIM hv AS STRING = fb.Headers.Get("X-MT-Header")
  IF hv = "hdr-value-42" THEN PRINT "hdr-ok" ELSE PRINT "hdr-fail=["; hv; "]"

  ' (b) header assente → stringa vuota
  DIM miss AS STRING = fb.Headers.Get("X-MT-Absent")
  IF miss = "" THEN PRINT "hdr-missing" ELSE PRINT "hdr-missing-fail=["; miss; "]"

  ' (c) forma DIM + assegnazione separata (typing via assegnazione)
  DIM fb2 AS FETCHRESULT
  fb2 = FETCH("http://localhost:8799/hdr", opts)
  DIM hv2 AS STRING
  hv2 = fb2.Headers.Get("X-MT-Header")
  IF hv2 = "hdr-value-42" THEN PRINT "hdr2-ok" ELSE PRINT "hdr2-fail=["; hv2; "]"

  HTTP SERVER STOP
END SUB
