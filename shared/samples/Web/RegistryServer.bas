' FILE: Web/RegistryServer.bas
' Demo frontend BSP del registry (PackageManager-BSP §3): un server HTTP che
' fornisce l'API /api/packages (dati di esempio) e serve le pagine .bsp del
' frontend (samples/Web/RegistryBsp/). Avvio:
'   dotnet run --project REPL/REPL.csproj -- RegistryServer.bas
'   poi apri http://localhost:8099/index.bsp
'
' Nota: i dati sono HARDCODED (demo). Con un registry reale, RegistryUrl
' (default http://localhost:8087) risponde a /api/packages.

IMPORT HTTP
IMPORT JSON

DIM pkgs AS LIST OF JSON
SUB Main()
pkgs = []

DIM p1 AS JSON
p1 = NEW JSON
p1("name") = "gmb-vec"
p1("version") = "1.2.0"
p1("description") = "Vettori e algebra lineare per GMBasic"
p1("contentType") = "mixed"
p1("backends") = "il, c99"
pkgs.Add(p1)

DIM p2 AS JSON
p2 = NEW JSON
p2("name") = "geom"
p2("version") = "0.9.1"
p2("description") = "Geometria computazionale (AABB, ray casting, mesh)"
p2("contentType") = "cffi"
p2("backends") = "c99"
pkgs.Add(p2)

DIM p3 AS JSON
p3 = NEW JSON
p3("name") = "crypto-utils"
p3("version") = "2.0.3"
p3("description") = "Hash, firme e cifratura (SHA-256, ECDSA, AES-GCM)"
p3("contentType") = "source"
p3("backends") = "il, c99, vm"
pkgs.Add(p3)

DIM p4 AS JSON
p4 = NEW JSON
p4("name") = "tui-dashboard"
p4("version") = "0.5.0"
p4("description") = "Widget TUI per cruscotti operativi"
p4("contentType") = "dotnet"
p4("backends") = "il"
pkgs.Add(p4)

PRINT "=== GMBasic BSP Registry (demo) ==="

HTTP ROUTE "GET", "/api/packages", ListHandler()
HTTP ROUTE "GET", "/api/packages/{name}", DetailHandler()
HTTP ROUTE "GET", "/", RedirectIndex()

DIM cfg AS HTTPCONFIG
cfg = NEW HTTPCONFIG
cfg.Port = 8099
HTTP CONFIG cfg

HTTP STATIC "/", "RegistryBsp"

PRINT "Server on http://localhost:8099"
PRINT "  Frontend: /index.bsp  /package.bsp?name=gmb-vec"
PRINT "  API:      /api/packages   /api/packages/{name}"
PRINT

HTTP SERVER cfg.Port

' Tiene il processo vivo (il server gira in background): 120s di demo, poi stop.
DIM demoFor AS INTEGER
FOR demoFor = 1 TO 120
  SLEEP 1000
NEXT

HTTP SERVER STOP

END SUB

SUB ListHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.SendJSON(pkgs)
END SUB

SUB DetailHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  ' Il nome arriva dal parametro di PATH della route (/api/packages/{name}), non dalla query
  ' string — prima leggeva req.Query["name"] (sempre vuoto → 404 "not found" per ogni nome,
  ' e la pagina package.bsp renderizzava la scheda vuota).
  DIM name = req.Params["name"]
  DIM found AS JSON
  DIM ok = FALSE
  FOR i = 0 TO pkgs.Count() - 1
    IF pkgs[i]["Name"] = name THEN
      found = pkgs[i]
      ok = TRUE
      EXIT FOR
    END IF
  NEXT
  IF ok THEN res.SendJSON(found) ELSE res.StatusCode = 404 : res.Send("not found")
END SUB

SUB RedirectIndex(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.Redirect("/index.bsp")
END SUB
