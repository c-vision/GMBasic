' FILE: Http/HttpServer.bas
' =================================================================================================
'       GUIDA: SERVER HTTP CON BASIC SCRIPTING PAGES (.bsp)
' =================================================================================================
'
' Questo server dimostra l'uso delle pagine .bsp (BASIC Scripting Pages) al posto del
' vecchio motore Scriban. Le pagine .bsp sono veri programmi BASIC compilati dalla VM.
'
' 1. AVVIO DEL SERVER
'    HTTP SERVER 8096 avvia il server sulla porta specificata.
'
' 2. PAGINE .BSP
'    e servite dalla cache a ogni richiesta. Sintassi mista HTML+BASIC:
'      <h1><%= title %></h1>
'      <% FOR i = 1 TO 10 %>...<% NEXT %>
'
' 3. ROUTE CLASSICHE
'    HTTP ROUTE per handler BASIC tradizionali (JSON API, redirect, etc.)
'
' 4. STATIC FILES
'    HTTP STATIC per CSS, JS, immagini.
'
' =================================================================================================

IMPORT HTTP
IMPORT CONCURRENCY

' --- CONFIGURAZIONE SERVER ---
DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
cfg.Port = 8096
cfg.RequestTimeoutMs = 15000
HTTP CONFIG cfg

' --- ROUTES ---
HTTP ROUTE "GET", "/api/info", InfoHandler()
HTTP ROUTE "GET", "/api/users/{id}", UserHandler()
HTTP ROUTE "POST", "/api/echo", EchoHandler()

' --- BSP PAGES (pre-compilate con BSP ./pages/) ---

' --- STATIC FILES ---
HTTP STATIC "/static", "./wwwroot"

PRINT "=== GM-BASIC HTTP Server ==="
PRINT "Porta: "; cfg.Port
PRINT "Routes: GET /api/info, GET /api/users/{id}, POST /api/echo"
PRINT "Static: /static -> ./wwwroot"
PRINT "Pre-compila con: BSP ./pages/"
PRINT

HTTP SERVER cfg.Port

' --- CLIENT TEST ---
DIM wg AS WAITGROUP
GO SUB ClientTest(wg) IN wg
WAIT wg
HTTP SERVER STOP

' =====================================================================
' HANDLER ROUTE CLASSICI
' =====================================================================

SUB InfoHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM info AS JSON = NEW JSON
  info("server") = "GM-BASIC"
  info("version") = "1.0"
  info("time") = TIME
  info("bsp_enabled") = "true"
  res.SendJSON(info)
END SUB

SUB UserHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM id = req.Params("id")
  DIM user AS JSON = NEW JSON
  user("id") = id
  user("name") = "User_" + id
  res.SendJSON(user)
END SUB

SUB EchoHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.SendJSON(req.Body)
END SUB

' =====================================================================
' CLIENT TEST
' =====================================================================

SUB ClientTest(wg AS WAITGROUP)
  SLEEP 500
  DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS
  opts.Headers.Connection = "close"

  ' Test API JSON
  DIM r1 AS FETCHRESULT = FETCH("http://127.0.0.1:8096/api/info", opts)
  PRINT "[CLIENT] GET /api/info: "; r1.StatusCode; " "; r1.Body

  DIM r2 AS FETCHRESULT = FETCH("http://127.0.0.1:8096/api/users/42", opts)
  PRINT "[CLIENT] GET /api/users/42: "; r2.StatusCode; " "; r2.Body

  ' Test BSP page (se pre-compilata)
  DIM r3 AS FETCHRESULT = FETCH("http://127.0.0.1:8096/index.bsp", opts)
  PRINT "[CLIENT] GET /index.bsp: "; r3.StatusCode; " ("; LEN(r3.Body); " bytes)"
END SUB
