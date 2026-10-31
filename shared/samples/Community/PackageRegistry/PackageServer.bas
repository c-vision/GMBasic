' PackageRegistry - GMBasic Package Server (registry HTTP)
' ============================================================
' Avvio (solo API, consigliato — la CWD è la cartella del sample):
'   dotnet run --project REPL/REPL.csproj -- samples/Community/PackageRegistry/PackageServer.bas
'
' Serve la cache locale ~/.gmb/packages (gmb-package.json v1+v2):
'   GET /api/packages                -> lista pacchetti
'   GET /api/packages/{name}         -> dettaglio (con negoziazione ?backend&platform&edition)
'   GET /api/packages/search?q=...   -> ricerca
'   GET /api/advisories              -> DB advisory di sicurezza (gmb-package.json advisories.json)
'   GET /api/stats                   -> statistiche
'   GET /download/{name}/{version}   -> zip del pacchetto
'
' Frontend BSP (PackageManager-BSP §3): monta ANCHE le pagine Basic Server Pages del
' registry (index.bsp/package.bsp) alla radice — la cartella è configurabile via
' cfg.frontendDir (default: ../Web/RegistryBsp relativo alla CWD del sample, ovvero
' samples/Community/Web/RegistryBsp). Le route API hanno priorità sui file statici
' (dispatch: route → fallback → static, mai prima), quindi /api/* non viene oscurato.
' Le pagine BSP si auto-localizzano l'API via header Host (REGISTRY_URL$ opzionale).
' ============================================================

IMPORT HTTP
IMPORT IO
IMPORT JSON
IMPORT COMPRESSION
IMPORT CRYPTOGRAPHY

' --- Configurazione da file di settings (package-registry.json, accanto al server) ---
' FILE CONFIG UNICO: path + limiti + admin + utenti autorizzati.
' Esempio:
' {
'   "registryRoot": "...", "serverDir": "./", "frontendDir": "...", "bind": "...",
'   "maxPackageBytes": 104857600, "rateLimitPerMinute": 60,
'   "admin": { "tokenHash": "<PASSWORDHASH token admin>", "enabled": "1" },
'   "users": { "alice": { "name":"alice", "tokenHash":"<hash>", "publicKey":"<base64>",
'                         "status":"active" } }
' }
' status: "active" | "blocked". publicKey: chiave Ed25519 (base64 32B) per verificare la
' firma dei pacchetti dell'utente. Gli utenti li crea/gestisce SOLO l'admin.
DIM registryRoot AS STRING
' Portatile: RELATIVO alla dir dello script (CWD). Override via cfg.registryRoot se serve.
registryRoot = "./.registry"

DIM serverDir AS STRING
serverDir = "./"

DIM frontendDir AS STRING
frontendDir = "../Web/RegistryBsp"

DIM maxPackageBytes AS LONG
maxPackageBytes = 104857600   ' 100 MB default (limite dimensione upload, #2)

DIM rateLimitPerMinute AS LONG
rateLimitPerMinute = 60       ' max richieste mutate al minuto per utente/IP (#1)

DIM cfgPath AS STRING
cfgPath = "./package-registry.json"
DIM cfg AS JSON
cfg = NEW JSON
IF IO.FileExists(cfgPath) THEN
  DIM ch AS LONG
  ch = 0
  JSON OPEN cfgPath FOR "READ" AS #ch
  INPUT #ch, cfg
  CLOSE #ch
  IF cfg.registryRoot <> "" THEN registryRoot = cfg.registryRoot
  IF cfg.serverDir <> "" THEN serverDir = cfg.serverDir
  IF cfg.frontendDir <> "" THEN frontendDir = cfg.frontendDir
  IF cfg.maxPackageBytes > 0 THEN maxPackageBytes = cfg.maxPackageBytes
  IF cfg.rateLimitPerMinute > 0 THEN rateLimitPerMinute = cfg.rateLimitPerMinute
END IF
' assicura che registryRoot esista (dove stanno users/claims/repos e i pacchetti)
IF IO.DirExists(registryRoot) = 0 THEN MKDIR registryRoot

' ROOT SEMPRE ASSOLUTA (bug reale, 2026-10-28). `/download/{nome}/{ver}` comprime la
' cartella del pacchetto: con una root RELATIVA (default di questo sample: "./.registry")
' il nome delle entry nello zip conserva il prefisso, quindi l'archivio contiene
' ".registry/<nome>/<ver>/gmb-package.json" invece di "gmb-package.json". Il client estrae
' in <cache>/<nome>/<ver>/ e poi non trova il manifest ("Manifest 'gmb-package.json' non
' trovato per il pacchetto ..."), quindi OGNI install dal registry fallisce con la
' configurazione di default. Con una root assoluta lo zip usa il path relativo alla
' cartella comune e il layout è corretto (verificato end-to-end).
IF LEFT(registryRoot, 1) <> "/" THEN
  registryRoot = IO.NORMALIZEPATH(IO.CWD() + "/" + registryRoot)
END IF
IF LEFT(serverDir, 1) <> "/" THEN
  serverDir = IO.NORMALIZEPATH(IO.CWD() + "/" + serverDir)
END IF

DIM gPackages AS JSON
gPackages = NEW JSON
DIM gStats AS JSON
gStats = NEW JSON
DIM gRateLimits AS JSON
gRateLimits = NEW JSON   ' rate-limit in-memory: chiave -> { minute, count } (#1)

' --- Utenti autorizzati e admin (dalla CONFIG UNICA cfgPath, non file separati) ---
' cfg.users: nome -> { name, tokenHash, publicKey, status: active|blocked }
' cfg.admin : { tokenHash, enabled }
' Gli utenti li crea/gestisce SOLO l'admin (endpoint /api/admin/users/*); niente self-register.
DIM gUsers AS JSON
gUsers = NEW JSON
IF cfg.users.name <> "" OR cfg.users <> "" THEN gUsers = cfg.users
DIM gAdmin AS JSON
gAdmin = NEW JSON
IF cfg.admin.tokenHash <> "" THEN gAdmin = cfg.admin

' se non c'è admin configurato, crea admin di primo accesso con token "gmb_admin" (hash) — da cambiare.
IF gAdmin.tokenHash = "" THEN
  gAdmin = NEW JSON
  gAdmin["tokenHash"] = CRYPTOGRAPHY.PASSWORDHASH("gmb_admin")
  gAdmin["enabled"] = "1"
  cfg["admin"] = gAdmin
  SaveConfig()
END IF

' claims.json: nome -> { type: official|scope, owner, collaborators[], reserved }
' repos.json: nome -> { repo, owner, tag, status, verify_report }
' Vivono dentro registryRoot (dati di runtime, come i pacchetti), non nella config.
DIM claimsPath AS STRING
claimsPath = registryRoot + "/claims.json"
DIM gClaims AS JSON
gClaims = NEW JSON
IF IO.FileExists(claimsPath) THEN
  DIM ch2 AS LONG
  ch2 = 0
  JSON OPEN claimsPath FOR "READ" AS #ch2
  INPUT #ch2, gClaims
  CLOSE #ch2
END IF

DIM reposPath AS STRING
reposPath = registryRoot + "/repos.json"
DIM gRepos AS JSON
gRepos = NEW JSON
IF IO.FileExists(reposPath) THEN
  DIM rh AS LONG
  rh = 0
  JSON OPEN reposPath FOR "READ" AS #rh
  INPUT #rh, gRepos
  CLOSE #rh
END IF

PRINT "=== GM-BASIC Package Registry ==="
PRINT "Loading packages..."

' Inline startup loading (non GOSUB, per evitare scoping issues)
DIM totalPkgs AS LONG
totalPkgs = 0
DIM totalVers AS LONG
totalVers = 0

' IO.DIR restituisce una LISTA (pattern canonico: FOR EACH, come AiVision.bas).
' NB: NON usare STRING+SPLIT (IO.DIR non è una stringa multilinea) né Count()
' (Count() su liste create da IO.DIR è 0 nel VM/C, ma FOR EACH itera correttamente).
DIM entries AS LIST
entries = IO.DIR(registryRoot + "/")

FOR EACH pkgName IN entries
  IF pkgName = "" THEN CONTINUE FOR
  ' IO.DIR restituisce nomi base: costruisci il path completo contro registryRoot
  DIM pkgPath AS STRING
  pkgPath = registryRoot + "/" + pkgName
  IF IO.DirExists(pkgPath) = 0 THEN CONTINUE FOR

  DIM verEntries AS LIST
  verEntries = IO.DIR(pkgPath + "/")
  DIM verCount AS LONG
  verCount = 0
  DIM pkgData AS JSON
  pkgData = NEW JSON

  DIM lastVer AS STRING
  lastVer = ""
  ' Elenco COMPLETO delle versioni: `versions` resta il conteggio (lo mostra la BSP), ma il client
  ' ne ha bisogno per risolvere un VINCOLO (>=1.0, ~2.1, ...) in una versione concreta: con solo
  ' il massimo non puo' scegliere, e prima finiva per passare il vincolo come versione
  ' (GET /download/pkg/>=1.0 -> 404).
  DIM verList AS JSON
  verList = NEW JSON
  FOR EACH vName IN verEntries
    IF vName = "" THEN CONTINUE FOR
    DIM vPath AS STRING
    vPath = pkgPath + "/" + vName
    DIM mPath AS STRING
    mPath = vPath + "/gmb-package.json"
    IF IO.FileExists(mPath) = 0 THEN CONTINUE FOR

    verCount = verCount + 1
    totalVers = totalVers + 1
    verList[LTRIM$(STR(verCount))] = vName

    DIM mh AS LONG
    mh = 0
    JSON OPEN mPath FOR "READ" AS #mh
    DIM m AS JSON
    m = NEW JSON
    INPUT #mh, m
    CLOSE #mh

    IF verCount = 1 THEN
      pkgData["name"] = m.name
      pkgData["description"] = m.description
      pkgData["author"] = m.author
      pkgData["license"] = m.license
      pkgData["contentType"] = m.contentType
      pkgData["backends"] = m.backends
    END IF
    IF m.version <> "" THEN lastVer = m.version
  NEXT

  IF verCount > 0 THEN
    pkgData["versions"] = verCount
    pkgData["version"] = lastVer
    pkgData["versionList"] = verList
    gPackages[pkgName] = pkgData
    totalPkgs = totalPkgs + 1
  END IF
NEXT

gStats["totalPackages"] = totalPkgs
gStats["totalVersions"] = totalVers

PRINT "  Packages: "; STR(totalPkgs)
PRINT "  Versions: "; STR(totalVers)

' Persiste la config unica (path, limits, admin, users) su cfgPath.
SUB SaveConfig()
  DIM cfgStr AS STRING
  cfgStr = JSON.STRINGIFY(cfg)
  OPEN cfgPath FOR OUTPUT AS #9
  PRINT #9, cfgStr
  CLOSE #9
END SUB

HTTP ROUTE "GET", "/api/stats", HandleApiStats()
HTTP ROUTE "GET", "/api/packages", HandleApiPackages()
HTTP ROUTE "GET", "/api/packages/search", HandleApiSearch()
HTTP ROUTE "GET", "/api/advisories", HandleApiAdvisories()
HTTP ROUTE "GET", "/download/{name}/{version}", HandleDownload()
HTTP ROUTE "GET", "/api/packages/{name}", HandleApiPackageDetail()
HTTP ROUTE "POST", "/api/packages", HandleApiPublish()
HTTP ROUTE "GET", "/api/packages/{name}/tags", HandleApiTags()
HTTP ROUTE "POST", "/api/packages/{name}/tags", HandleApiSetTag()
HTTP ROUTE "POST", "/api/packages/{name}/owners", HandleApiOwners()
HTTP ROUTE "POST", "/api/packages/{name}/yank", HandleApiYank()
HTTP ROUTE "POST", "/api/packages/{name}/deprecate", HandleApiDeprecate()
HTTP ROUTE "POST", "/api/packages/register", HandleApiRegisterRepo()
HTTP ROUTE "POST", "/api/packages/{name}/materialize", HandleApiMaterialize()
HTTP ROUTE "POST", "/api/packages/{name}/sync", HandleApiSync()
HTTP ROUTE "POST", "/api/admin/reserve", HandleApiAdminReserve()
HTTP ROUTE "POST", "/api/admin/assign", HandleApiAdminAssign()
HTTP ROUTE "POST", "/api/admin/revoke", HandleApiAdminRevoke()
HTTP ROUTE "POST", "/api/admin/password", HandleApiAdminPassword()
HTTP ROUTE "POST", "/api/admin/users/create", HandleApiAdminUserCreate()
HTTP ROUTE "POST", "/api/admin/users/setkey", HandleApiAdminUserSetKey()
HTTP ROUTE "POST", "/api/admin/users/status", HandleApiAdminUserStatus()
HTTP ROUTE "GET", "/api/admin/users/list", HandleApiAdminUserList()

HTTP ROUTE "GET", "/", HandleIndex()
HTTP STATIC "/", frontendDir
HTTP STATIC "/static", serverDir + "/wwwroot"

' Porta dal file di settings (cfg.bind es. http://localhost:8087), default 8087.
DIM bindStr AS STRING
bindStr = ""
IF cfg.bind <> "" THEN bindStr = cfg.bind
DIM portNum AS LONG
portNum = 8087
IF INSTR(bindStr, ":") > 0 THEN
  DIM lastColon AS LONG
  lastColon = 0
  DIM ci AS LONG
  FOR ci = 1 TO LEN(bindStr)
    IF MID(bindStr, ci, 1) = ":" THEN lastColon = ci
  NEXT ci
  IF lastColon > 0 AND lastColon < LEN(bindStr) THEN
    portNum = VAL(MID(bindStr, lastColon + 1))
  END IF
END IF

PRINT "Server on http://localhost:"; STR(portNum)
PRINT

' autoStopSeconds: opzionale (default 0 = forever). Utile per test/demo deterministici.
DIM autoStop AS LONG
autoStop = 0
IF cfg.autoStopSeconds > 0 THEN autoStop = cfg.autoStopSeconds

' Configura il server HTTP: timeout richiesta esteso (default 5 min) così il download
' (COMPRESSION.ZIP + SendFile) di pacchetti grandi non finisce in 504 Gateway Timeout
' (RequestTimeoutMs di default = 30s). Sovrascrivibile con cfg.requestTimeoutMs.
DIM httpTimeoutMs AS LONG
httpTimeoutMs = 300000
IF cfg.requestTimeoutMs > 0 THEN httpTimeoutMs = cfg.requestTimeoutMs
DIM httpCfg AS HTTPCONFIG = NEW HTTPCONFIG
httpCfg.Port = portNum
httpCfg.RequestTimeoutMs = httpTimeoutMs
' LIMITE DEL BODY HTTP allineato a `maxPackageBytes` (bug reale 2026-10-28): il default di
' HttpConfig.MaxRequestBodySize è 10 MB, quindi OGNI pubblicazione oltre ~7,5 MB di archivio
' (base64 ≈ 4/3) veniva rifiutata dall'HTTP layer con 413 "Payload Too Large" — PRIMA di
' arrivare al controllo `maxPackageBytes` (100 MB) del server. Sintomo: `gmb publish` di un
' pacchetto da 29 MB (duckdb) falliva mentre uno da 5 MB passava. Si allinea al limite
' dichiarato, con 1 MB di overhead per il JSON (name/version/token).
httpCfg.MaxRequestBodySize = (maxPackageBytes * 4) \ 3 + 1048576
HTTP CONFIG httpCfg

' Opt-in SHELL: abilita SYSTEM.SHELLCODE/SHELLOUT per la verifica git indipendente dei
' pacchetti contributor (Fase C2). Il server esegue SOLO comandi git costruiti dal proprio
' codice (repo/tag del request), mai input utente arbitrario.
SET SHELL ON

HTTP SERVER portNum
IF autoStop > 0 THEN
  DIM demoLeft AS LONG
  demoLeft = autoStop
  WHILE demoLeft > 0
    SLEEP 1000
    demoLeft = demoLeft - 1
  WEND
  HTTP SERVER STOP
  PRINT "Registry stopped."
ELSE
  PRINT "Running. Ctrl+C to stop."
  DIM running AS LONG
  running = 1
  WHILE running
    SLEEP 1000
  WEND
END IF

' ==================== HANDLERS ====================

SUB HandleApiStats(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.SendJSON(gStats)
END SUB

SUB HandleApiPackages(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM list AS JSON
  list = NEW JSON
  DIM idx AS LONG
  idx = 0
  DIM k AS STRING
  FOR EACH k IN JSON.KEYS(gPackages)
    idx = idx + 1
    list[STR(idx)] = gPackages[k]
  NEXT
  res.SendJSON(list)
END SUB

SUB HandleApiPackageDetail(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM name AS STRING
  name = req.Params["name"]
  DIM pkg AS JSON
  pkg = gPackages[name]
  DIM pkgName AS STRING
  pkgName = pkg.name
  IF pkgName = "" THEN
    res.StatusCode = 404
    DIM e AS JSON
    e = NEW JSON
    e["error"] = "Package not found"
    res.SendJSON(e)
    EXIT SUB
  END IF
  res.SendJSON(pkg)
END SUB

' GET /api/advisories — DB degli advisory di sicurezza del registry.
' Sorgente: file <registryRoot>/advisories.json (array JSON di oggetti
' {id, package, range, severity, summary, fixed}). Se assente risponde {}.
' Il client `gmb pkg audit` (senza --advisories) interroga questo endpoint.
SUB HandleApiAdvisories(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM advPath AS STRING
  advPath = registryRoot + "/advisories.json"
  IF IO.FileExists(advPath) = 0 THEN
    DIM advNone AS JSON
    advNone = NEW JSON
    res.SendJSON(advNone)
    EXIT SUB
  END IF
  DIM advRaw AS STRING
  advRaw = ReadOneLine(advPath)
  IF advRaw = "" THEN
    DIM advEmpty AS JSON
    advEmpty = NEW JSON
    res.SendJSON(advEmpty)
    EXIT SUB
  END IF
  DIM advParsed AS JSON
  advParsed = JSON.PARSE(advRaw)
  res.SendJSON(advParsed)
END SUB

' Legge una riga da un file di testo ("" se assente). I marker del registry
' (.deprecated/.yanked) e advisories.json sono JSON su una sola riga.
FUNCTION ReadOneLine(path AS STRING) AS STRING
  DIM lineIn AS STRING
  lineIn = ""
  IF IO.FileExists(path) = 0 THEN
    ReadOneLine = ""
    EXIT FUNCTION
  END IF
  OPEN path FOR INPUT AS #7
  INPUT #7, lineIn
  CLOSE #7
  ReadOneLine = lineIn
END FUNCTION

SUB HandleApiSearch(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM q AS STRING
  q = req.Query["q"]
  IF q = "" THEN q = "*"

  DIM list AS JSON
  list = NEW JSON
  DIM idx AS LONG
  idx = 0
  DIM k AS STRING
  FOR EACH k IN JSON.KEYS(gPackages)
    DIM pkg AS JSON
    pkg = gPackages[k]
    IF q = "*" THEN
      idx = idx + 1
      list[STR(idx)] = pkg
    ELSE
      DIM haystack AS STRING
      haystack = UPPER(pkg.name + " " + pkg.description + " " + pkg.author)
      IF INSTR(haystack, UPPER(q)) > 0 THEN
        idx = idx + 1
        list[STR(idx)] = pkg
      END IF
    END IF
  NEXT
  res.SendJSON(list)
END SUB

SUB HandleDownload(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM name AS STRING
  name = req.Params["name"]
  DIM ver AS STRING
  ver = req.Params["version"]

  DIM pkgDir AS STRING
  pkgDir = registryRoot + "/" + name + "/" + ver
  IF IO.DirExists(pkgDir) = 0 THEN
    res.StatusCode = 404
    DIM e AS JSON
    e = NEW JSON
    e["error"] = "Package version not found"
    res.SendJSON(e)
    EXIT SUB
  END IF

  ' Fase A6: rifiuta le versioni yanked (410 Gone)
  DIM yankMark AS STRING
  yankMark = yankedPath(name, ver)
  IF IO.FileExists(yankMark) THEN
    res.StatusCode = 410
    DIM eY AS JSON
    eY = NEW JSON
    eY["error"] = "Version yanked: " + name + "@" + ver
    res.SendJSON(eY)
    EXIT SUB
  END IF

  DIM tmp AS STRING
  tmp = "/tmp/gmb_" + name + "_" + ver + ".zip"

  ' Download PER-PIATTAFORMA: se il client ha negoziato una piattaforma (?platform=osx-arm64) e la
  ' cartella lib/<plat> esiste, zippa SOLO i file comuni + lib/<plat> (non scarica le altre
  ' piattaforme di un pacchetto multi-RID). Senza platform -> intera cartella (back-compat, sorgenti).
  DIM plat AS STRING
  plat = req.Params["platform"]
  DIM ok AS LONG
  IF plat <> "" AND IO.DirExists(pkgDir + "/lib/" + plat) THEN
    DIM srcs AS STRING
    srcs = ""
    DIM ents AS LIST
    ents = IO.DIR(pkgDir + "/")
    FOR EACH e IN ents
      IF e = "" THEN CONTINUE FOR
      IF e = "lib" THEN CONTINUE FOR
      srcs = srcs + pkgDir + "/" + e + ";"
    NEXT
    srcs = srcs + pkgDir + "/lib/" + plat
    ok = COMPRESSION.ZIP(srcs, tmp)
  ELSE
    ok = COMPRESSION.ZIP(pkgDir + ";", tmp)
  END IF

  res.SendFile(tmp)
  KILL tmp
END SUB
' --- Gestione utenti autorizzati (SOLO admin; niente self-register) ---
' Gli utenti vivono in cfg.users ({name, tokenHash, publicKey, status}).
' create: genera il token e crea l'utente (status "blocked" finché l'admin non lo attiva).

SUB HandleApiAdminUserCreate(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  IF AdminOk(req, res) = "" THEN
    res.StatusCode = 401
    DIM eUC0 AS JSON
    eUC0 = NEW JSON
    eUC0["error"] = "Admin required"
    res.SendJSON(eUC0)
    EXIT SUB
  END IF
  DIM bodyUC AS JSON
  bodyUC = JSON.PARSE(req.Body)
  DIM unameUC AS STRING
  unameUC = bodyUC.name
  IF unameUC = "" THEN
    res.StatusCode = 400
    DIM eUC1 AS JSON
    eUC1 = NEW JSON
    eUC1["error"] = "name mancante"
    res.SendJSON(eUC1)
    EXIT SUB
  END IF
  DIM existingUC AS JSON
  existingUC = gUsers[unameUC]
  IF existingUC.name <> "" THEN
    res.StatusCode = 409
    DIM eUC2 AS JSON
    eUC2 = NEW JSON
    eUC2["error"] = "Utente già esistente"
    res.SendJSON(eUC2)
    EXIT SUB
  END IF
  DIM tokenUC AS STRING
  ' RND non accetta argomenti in GMBasic (genera in [0,1)): RND(n) -> 1..n e' l'idioma
  ' di Clipper. Il suffisso casuale si ottiene moltiplicando per 1000000 e troncando.
  tokenUC = "gmb_" + unameUC + "_" + LTRIM$(STR(TI())) + "_" + LTRIM$(STR(INT(RND() * 1000000)))
  DIM newUserUC AS JSON
  newUserUC = NEW JSON
  newUserUC["name"] = unameUC
  newUserUC["tokenHash"] = CRYPTOGRAPHY.PASSWORDHASH(tokenUC)
  newUserUC["publicKey"] = ""
  newUserUC["status"] = "blocked"   ' attiva solo l'admin (status active)
  gUsers[unameUC] = newUserUC
  cfg["users"] = gUsers
  SaveConfig()
  DIM outUC AS JSON
  outUC = NEW JSON
  outUC["ok"] = "1"
  outUC["name"] = unameUC
  outUC["status"] = "blocked"
  outUC["token"] = tokenUC   ' mostrato una sola volta
  res.SendJSON(outUC)
END SUB

' setkey: registra la chiave pubblica Ed25519 dell'utente (per verificare la firma dei pacchetti).
SUB HandleApiAdminUserSetKey(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  IF AdminOk(req, res) = "" THEN
    res.StatusCode = 401
    DIM eSK0 AS JSON
    eSK0 = NEW JSON
    eSK0["error"] = "Admin required"
    res.SendJSON(eSK0)
    EXIT SUB
  END IF
  DIM bodySK AS JSON
  bodySK = JSON.PARSE(req.Body)
  DIM unameSK AS STRING
  unameSK = bodySK.name
  DIM pubSK AS STRING
  pubSK = bodySK.publicKey
  IF unameSK = "" OR pubSK = "" THEN
    res.StatusCode = 400
    DIM eSK1 AS JSON
    eSK1 = NEW JSON
    eSK1["error"] = "name/publicKey mancanti"
    res.SendJSON(eSK1)
    EXIT SUB
  END IF
  DIM uSK AS JSON
  uSK = gUsers[unameSK]
  IF uSK.name = "" THEN
    res.StatusCode = 404
    DIM eSK2 AS JSON
    eSK2 = NEW JSON
    eSK2["error"] = "Utente non trovato: " + unameSK
    res.SendJSON(eSK2)
    EXIT SUB
  END IF
  uSK["publicKey"] = pubSK
  gUsers[unameSK] = uSK
  cfg["users"] = gUsers
  SaveConfig()
  DIM outSK AS JSON
  outSK = NEW JSON
  outSK["ok"] = "1"
  outSK["name"] = unameSK
  outSK["publicKey"] = pubSK
  res.SendJSON(outSK)
END SUB

' status: attiva/blocca un utente. body { name, status: "active"|"blocked" }.
SUB HandleApiAdminUserStatus(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  IF AdminOk(req, res) = "" THEN
    res.StatusCode = 401
    DIM eST0 AS JSON
    eST0 = NEW JSON
    eST0["error"] = "Admin required"
    res.SendJSON(eST0)
    EXIT SUB
  END IF
  DIM bodyST AS JSON
  bodyST = JSON.PARSE(req.Body)
  DIM unameST AS STRING
  unameST = bodyST.name
  DIM statusST AS STRING
  statusST = bodyST.status
  IF unameST = "" OR (statusST <> "active" AND statusST <> "blocked") THEN
    res.StatusCode = 400
    DIM eST1 AS JSON
    eST1 = NEW JSON
    eST1["error"] = "name/status (active|blocked) richiesti"
    res.SendJSON(eST1)
    EXIT SUB
  END IF
  DIM uST AS JSON
  uST = gUsers[unameST]
  IF uST.name = "" THEN
    res.StatusCode = 404
    DIM eST2 AS JSON
    eST2 = NEW JSON
    eST2["error"] = "Utente non trovato: " + unameST
    res.SendJSON(eST2)
    EXIT SUB
  END IF
  uST["status"] = statusST
  gUsers[unameST] = uST
  cfg["users"] = gUsers
  SaveConfig()
  DIM outST AS JSON
  outST = NEW JSON
  outST["ok"] = "1"
  outST["name"] = unameST
  outST["status"] = statusST
  res.SendJSON(outST)
END SUB

' list: elenco utenti (senza tokenHash in chiaro). Solo admin.
SUB HandleApiAdminUserList(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  IF AdminOk(req, res) = "" THEN
    res.StatusCode = 401
    DIM eUL0 AS JSON
    eUL0 = NEW JSON
    eUL0["error"] = "Admin required"
    res.SendJSON(eUL0)
    EXIT SUB
  END IF
  DIM outUL AS JSON
  outUL = NEW JSON
  DIM kUL AS STRING
  FOR EACH kUL IN JSON.KEYS(gUsers)
    DIM uUL AS JSON
    uUL = gUsers[kUL]
    DIM pubUL AS STRING
    pubUL = uUL.publicKey
    IF pubUL = "" THEN pubUL = "(none)"
    DIM pubKeyField AS JSON
    pubKeyField = NEW JSON
    pubKeyField["status"] = uUL.status
    pubKeyField["publicKey"] = pubUL
    outUL[kUL] = pubKeyField
  NEXT
  res.SendJSON(outUL)
END SUB

' Gestisce i collaboratori di un pacchetto (Fase A5). body JSON { action: add|remove, user }.
' Solo l'owner può modificare. Il claim.owner è immutabile (mai rimovibile).
SUB HandleApiOwners(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM caller AS STRING
  caller = ApiAuthUser(req)
  IF caller = "" THEN
    res.StatusCode = 401
    DIM eO AS JSON
    eO = NEW JSON
    eO["error"] = "Unauthorized"
    res.SendJSON(eO)
    EXIT SUB
  END IF

  DIM pname AS STRING
  pname = req.Params["name"]
  DIM claim AS JSON
  claim = gClaims[pname]
  IF claim.name = "" THEN
    res.StatusCode = 404
    DIM eO1 AS JSON
    eO1 = NEW JSON
    eO1["error"] = "Pacchetto non trovato: " + pname
    res.SendJSON(eO1)
    EXIT SUB
  END IF
  IF claim.owner <> caller THEN
    res.StatusCode = 403
    DIM eO2 AS JSON
    eO2 = NEW JSON
    eO2["error"] = "Solo l'owner può gestire i collaboratori"
    res.SendJSON(eO2)
    EXIT SUB
  END IF

  DIM bodyO AS JSON
  bodyO = JSON.PARSE(req.Body)
  DIM actionO AS STRING
  actionO = bodyO.action
  DIM userO AS STRING
  userO = bodyO.user
  IF actionO = "" OR userO = "" THEN
    res.StatusCode = 400
    DIM eO3 AS JSON
    eO3 = NEW JSON
    eO3["error"] = "action/user mancanti"
    res.SendJSON(eO3)
    EXIT SUB
  END IF

  IF userO = claim.owner THEN
    res.StatusCode = 400
    DIM eO4 AS JSON
    eO4 = NEW JSON
    eO4["error"] = "L'owner non può essere aggiunto/rimosso come collaboratore"
    res.SendJSON(eO4)
    EXIT SUB
  END IF

  ' il collaboratore deve essere un utente registrato
  IF gUsers[userO].name = "" THEN
    res.StatusCode = 404
    DIM eO5 AS JSON
    eO5 = NEW JSON
    eO5["error"] = "Utente non registrato: " + userO
    res.SendJSON(eO5)
    EXIT SUB
  END IF

  IF actionO = "add" THEN
    claim.collaborators[userO] = "1"
  ELSEIF actionO = "remove" THEN
    claim.collaborators[userO] = ""
  ELSE
    res.StatusCode = 400
    DIM eO6 AS JSON
    eO6 = NEW JSON
    eO6["error"] = "action deve essere add o remove"
    res.SendJSON(eO6)
    EXIT SUB
  END IF

  gClaims[pname] = claim
  DIM claimsStrO AS STRING
  claimsStrO = JSON.STRINGIFY(gClaims)
  OPEN claimsPath FOR OUTPUT AS #8
  PRINT #8, claimsStrO
  CLOSE #8

  DIM outO AS JSON
  outO = NEW JSON
  outO["ok"] = "1"
  outO["package"] = pname
  outO["owner"] = claim.owner
  res.SendJSON(outO)
END SUB

' Verifica un token: ritorna il nome utente se valido ("" altrimenti).
' Il client manda il token in header Authorization: Bearer <token>, ?token=, o nel body JSON.
FUNCTION ApiAuthUser(req AS HTTPREQUEST) AS STRING
  DIM token AS STRING
  token = req.Query["token"]
  DIM authHeader AS STRING
  authHeader = req.Headers.Get("Authorization")
  IF authHeader <> "" THEN
    IF LEFT(authHeader, 7) = "Bearer " THEN token = MID(authHeader, 8)
  END IF

  IF token = "" AND req.Body <> "" THEN
    DIM bodyTok AS JSON
    bodyTok = NEW JSON
    bodyTok = JSON.PARSE(req.Body)
    IF bodyTok.token <> "" THEN token = bodyTok.token
  END IF

  DIM found AS STRING
  found = ""
  IF token <> "" THEN
    DIM k AS STRING
    FOR EACH k IN JSON.KEYS(gUsers)
      IF found = "" THEN
        DIM u AS JSON
        u = gUsers[k]
        IF u.tokenHash <> "" THEN
          IF CRYPTOGRAPHY.PASSWORDVERIFY(token, u.tokenHash) THEN
            ' utente bloccato: non autentica (anche se il token è valido)
            IF u.status = "blocked" THEN
              found = ""
            ELSE
              found = k
            END IF
          END IF
        END IF
      END IF
    NEXT
  END IF
  ApiAuthUser = found
END FUNCTION

' Ritorna lo status di un utente ("active"/"blocked"/"") dalla config.
FUNCTION UserStatus(uname AS STRING) AS STRING
  DIM u AS JSON
  u = gUsers[uname]
  IF u.name = "" THEN
    UserStatus = ""
  ELSE
    UserStatus = u.status
  END IF
END FUNCTION

' Ritorna la chiave pubblica Ed25519 (base64) di un utente dalla config.
FUNCTION UserPublicKey(uname AS STRING) AS STRING
  DIM u AS JSON
  u = gUsers[uname]
  IF u.name = "" THEN
    UserPublicKey = ""
  ELSE
    UserPublicKey = u.publicKey
  END IF
END FUNCTION

' Aggiunge un pacchetto agli ownedPackages di un utente (crea il campo se assente).
SUB AddOwned(uname AS STRING, pkgName AS STRING)
  DIM owned AS JSON
  owned = gUsers[uname].ownedPackages
  IF owned.name <> "" THEN
    owned[pkgName] = "1"
  ELSE
    owned = NEW JSON
    owned[pkgName] = "1"
  END IF
  gUsers[uname]["ownedPackages"] = owned
END SUB

' Rate-limit per client (IP): permette rateLimitPerMinute richieste mutate al minuto.
FUNCTION RateLimitOk(req AS HTTPREQUEST) AS STRING
  DIM ip AS STRING
  ip = req.RemoteIp
  IF ip = "" THEN ip = "unknown"
  DIM nowMin AS LONG
  nowMin = TI() \ 60
  DIM slot AS JSON
  slot = gRateLimits[ip]
  IF slot.count = "" THEN
    slot = NEW JSON
    slot["minute"] = STR(nowMin)
    slot["count"] = "0"
  END IF
  IF VAL(slot.minute) <> nowMin THEN
    slot["minute"] = STR(nowMin)
    slot["count"] = "0"
  END IF
  DIM c AS LONG
  c = VAL(slot.count)
  IF c >= rateLimitPerMinute THEN
    RateLimitOk = ""
  ELSE
    slot["count"] = STR(c + 1)
    gRateLimits[ip] = slot
    RateLimitOk = "1"
  END IF
END FUNCTION

' Rimuove un pacchetto dagli ownedPackages di un utente.
SUB RemoveOwned(uname AS STRING, pkgName AS STRING)
  DIM owned AS JSON
  owned = gUsers[uname].ownedPackages
  IF owned.name <> "" THEN
    owned[pkgName] = ""
    gUsers[uname]["ownedPackages"] = owned
  END IF
END SUB

' Verifica il token admin (config). Ritorna:
'   "1"      → admin valido (token cambiato dal default)
'   "default"→ admin valido MA è ancora il token di default "gmb_admin" (va cambiato; gli endpoint
'              admin, tranne /password, rispondono 403 "cambia il token default")
'   ""       → non admin
FUNCTION IsAdmin(req AS HTTPREQUEST) AS STRING
  IF gAdmin.enabled <> "1" THEN
    IsAdmin = ""
    EXIT FUNCTION
  END IF
  DIM token AS STRING
  token = req.Query["token"]
  DIM authHeader AS STRING
  authHeader = req.Headers.Get("Authorization")
  IF authHeader <> "" THEN
    IF LEFT(authHeader, 7) = "Bearer " THEN token = MID(authHeader, 8)
  END IF
  IF token = "" THEN
    IsAdmin = ""
    EXIT FUNCTION
  END IF
  IF CRYPTOGRAPHY.PASSWORDVERIFY(token, gAdmin.tokenHash) THEN
    ' #9: enforcement — se il token admin configurato È il default noto "gmb_admin", segnala
    ' "default" (gli endpoint admin risponderanno 403 tranne /password).
    IF CRYPTOGRAPHY.PASSWORDVERIFY("gmb_admin", gAdmin.tokenHash) THEN
      IsAdmin = "default"
    ELSE
      IsAdmin = "1"
    END IF
  ELSE
    IsAdmin = ""
  END IF
END FUNCTION

' Ritorna "1" se l'admin è valido e NON è il default ("" altrimenti). Se è il default, invia
' 403 con messaggio esplicito. Usata dagli endpoint admin per il controllo "prima di agire".
FUNCTION AdminOk(req AS HTTPREQUEST, res AS HTTPRESPONSE) AS STRING
  DIM a AS STRING
  a = IsAdmin(req)
  IF a = "1" THEN
    AdminOk = "1"
  ELSEIF a = "default" THEN
    res.StatusCode = 403
    DIM eD AS JSON
    eD = NEW JSON
    eD["error"] = "Il token admin è ancora il default 'gmb_admin': cambialo con gmb admin password <nuovoToken> prima di usare gli endpoint admin"
    res.SendJSON(eD)
    AdminOk = ""
  ELSE
    res.StatusCode = 401
    DIM eU AS JSON
    eU = NEW JSON
    eU["error"] = "Admin required"
    res.SendJSON(eU)
    AdminOk = ""
  END IF
END FUNCTION

' Path del marker yank per una versione (Fase A6). Esiste il file = versione ritirata.
FUNCTION yankedPath(pkgName AS STRING, pkgVer AS STRING) AS STRING
  yankedPath = registryRoot + "/" + pkgName + "/" + pkgVer + "/.yanked"
END FUNCTION

' Yank/unyank di una versione (Fase A6). body JSON { version, yank: "1"|"0", reason }.
' Solo owner/collaboratori. Una volta yanked, il download risponde 410; il publish della
' stessa versione resta vietato (immutabilità). Unyank la riabilita.
SUB HandleApiYank(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM callerY AS STRING
  callerY = ApiAuthUser(req)
  IF callerY = "" THEN
    res.StatusCode = 401
    DIM eY0 AS JSON
    eY0 = NEW JSON
    eY0["error"] = "Unauthorized"
    res.SendJSON(eY0)
    EXIT SUB
  END IF

  DIM pnameY AS STRING
  pnameY = req.Params["name"]
  DIM claimY AS JSON
  claimY = gClaims[pnameY]
  IF claimY.name = "" THEN
    res.StatusCode = 404
    DIM eY1 AS JSON
    eY1 = NEW JSON
    eY1["error"] = "Pacchetto non trovato: " + pnameY
    res.SendJSON(eY1)
    EXIT SUB
  END IF
  DIM isOwnerY AS LONG
  isOwnerY = 0
  IF claimY.owner = callerY THEN isOwnerY = 1
  DIM isCollabY AS LONG
  isCollabY = 0
  IF isOwnerY = 0 THEN
    DIM ckY AS STRING
    FOR EACH ckY IN JSON.KEYS(claimY.collaborators)
      IF ckY = callerY THEN isCollabY = 1
    NEXT
  END IF
  IF isOwnerY = 0 AND isCollabY = 0 THEN
    res.StatusCode = 403
    DIM eY2 AS JSON
    eY2 = NEW JSON
    eY2["error"] = "Non sei owner/collaboratore di '" + pnameY + "'"
    res.SendJSON(eY2)
    EXIT SUB
  END IF

  DIM bodyY AS JSON
  bodyY = JSON.PARSE(req.Body)
  DIM pverY AS STRING
  pverY = bodyY.version
  IF pverY = "" THEN
    res.StatusCode = 400
    DIM eY3 AS JSON
    eY3 = NEW JSON
    eY3["error"] = "version mancante"
    res.SendJSON(eY3)
    EXIT SUB
  END IF

  DIM pkgDirY AS STRING
  pkgDirY = registryRoot + "/" + pnameY + "/" + pverY
  IF IO.DirExists(pkgDirY) = 0 THEN
    res.StatusCode = 404
    DIM eY4 AS JSON
    eY4 = NEW JSON
    eY4["error"] = "Versione non trovata: " + pnameY + "@" + pverY
    res.SendJSON(eY4)
    EXIT SUB
  END IF

  DIM yankMark AS STRING
  yankMark = yankedPath(pnameY, pverY)
  DIM doYank AS STRING
  doYank = bodyY.yank
  IF doYank = "0" THEN
    ' unyank
    IF IO.FileExists(yankMark) THEN KILL yankMark
  ELSE
    ' yank: scrivi il marker con la motivazione
    DIM yankInfo AS JSON
    yankInfo = NEW JSON
    yankInfo["yankedBy"] = callerY
    yankInfo["reason"] = bodyY.reason
    yankInfo["when"] = STR(TI())
    DIM yankInfoStr AS STRING
    yankInfoStr = JSON.STRINGIFY(yankInfo)
    OPEN yankMark FOR OUTPUT AS #8
    PRINT #8, yankInfoStr
    CLOSE #8
  END IF

  DIM outY AS JSON
  outY = NEW JSON
  outY["ok"] = "1"
  outY["package"] = pnameY
  outY["version"] = pverY
  outY["yanked"] = doYank
  res.SendJSON(outY)
END SUB

' Path del marker di DEPRECAZIONE per una versione (messaggio di deprecazione).
FUNCTION deprecatedPath(pkgName AS STRING, pkgVer AS STRING) AS STRING
  deprecatedPath = registryRoot + "/" + pkgName + "/" + pkgVer + "/.deprecated"
END FUNCTION

' Deprecate/undeprecate di una versione. body JSON { version, message, deprecated: "1"|"0" }.
' Solo owner. Il marker .deprecated (con messaggio) segnala la versione come deprecata.
SUB HandleApiDeprecate(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM callerD AS STRING
  callerD = ApiAuthUser(req)
  IF callerD = "" THEN
    res.StatusCode = 401
    DIM eD0 AS JSON
    eD0 = NEW JSON
    eD0["error"] = "Unauthorized"
    res.SendJSON(eD0)
    EXIT SUB
  END IF

  DIM pnameD AS STRING
  pnameD = req.Params["name"]
  DIM claimD AS JSON
  claimD = gClaims[pnameD]
  IF claimD.name = "" THEN
    res.StatusCode = 404
    DIM eD1 AS JSON
    eD1 = NEW JSON
    eD1["error"] = "Pacchetto non trovato: " + pnameD
    res.SendJSON(eD1)
    EXIT SUB
  END IF
  DIM isOwnerD AS LONG
  isOwnerD = 0
  IF claimD.owner = callerD THEN isOwnerD = 1
  IF isOwnerD = 0 THEN
    res.StatusCode = 403
    DIM eD2 AS JSON
    eD2 = NEW JSON
    eD2["error"] = "Non sei owner di '" + pnameD + "'"
    res.SendJSON(eD2)
    EXIT SUB
  END IF

  DIM bodyD AS JSON
  bodyD = JSON.PARSE(req.Body)
  DIM pverD AS STRING
  pverD = bodyD.version
  IF pverD = "" THEN
    res.StatusCode = 400
    DIM eD3 AS JSON
    eD3 = NEW JSON
    eD3["error"] = "version mancante"
    res.SendJSON(eD3)
    EXIT SUB
  END IF

  DIM pkgDirD AS STRING
  pkgDirD = registryRoot + "/" + pnameD + "/" + pverD
  IF IO.DirExists(pkgDirD) = 0 THEN
    res.StatusCode = 404
    DIM eD4 AS JSON
    eD4 = NEW JSON
    eD4["error"] = "Versione non trovata: " + pnameD + "@" + pverD
    res.SendJSON(eD4)
    EXIT SUB
  END IF

  DIM depMark AS STRING
  depMark = deprecatedPath(pnameD, pverD)
  DIM doDep AS STRING
  doDep = bodyD.deprecated
  IF doDep = "0" THEN
    IF IO.FileExists(depMark) THEN KILL depMark
  ELSE
    DIM depInfo AS JSON
    depInfo = NEW JSON
    depInfo["deprecatedBy"] = callerD
    depInfo["message"] = bodyD.message
    depInfo["when"] = STR(TI())
    DIM depStr AS STRING
    depStr = JSON.STRINGIFY(depInfo)
    OPEN depMark FOR OUTPUT AS #8
    PRINT #8, depStr
    CLOSE #8
  END IF

  DIM outD AS JSON
  outD = NEW JSON
  outD["ok"] = "1"
  outD["package"] = pnameD
  outD["version"] = pverD
  outD["deprecated"] = doDep
  res.SendJSON(outD)
END SUB

' Registra un pacchetto contributor (modello GO, Fase C1). body JSON
' { repo, name, tag, verify_report }. NON carica il contenuto: il registry è un proxy —
' salva il repo+tag+report, segna status pending/active in base al report locale.
SUB HandleApiRegisterRepo(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM callerR AS STRING
  callerR = ApiAuthUser(req)
  IF callerR = "" THEN
    res.StatusCode = 401
    DIM eR0 AS JSON
    eR0 = NEW JSON
    eR0["error"] = "Unauthorized"
    res.SendJSON(eR0)
    EXIT SUB
  END IF

  IF req.Body = "" THEN
    res.StatusCode = 400
    DIM eR1 AS JSON
    eR1 = NEW JSON
    eR1["error"] = "Body JSON mancante"
    res.SendJSON(eR1)
    EXIT SUB
  END IF

  DIM bodyR AS JSON
  bodyR = JSON.PARSE(req.Body)
  DIM repoUrl AS STRING
  repoUrl = bodyR.repo
  DIM pnameR AS STRING
  pnameR = bodyR.name
  DIM tagR AS STRING
  tagR = bodyR.tag
  DIM reportR AS JSON
  reportR = bodyR.verify_report
  IF repoUrl = "" OR pnameR = "" OR tagR = "" THEN
    res.StatusCode = 400
    DIM eR2 AS JSON
    eR2 = NEW JSON
    eR2["error"] = "repo/name/tag mancanti"
    res.SendJSON(eR2)
    EXIT SUB
  END IF

  ' il report locale (verifica automatica) ha superato la pipeline?
  DIM verifiedR AS LONG
  verifiedR = 0
  IF reportR.testExitCode = 0 THEN verifiedR = 1

  ' --- Verifica indipendente GIT (Fase C2 completa): il server verifica che il repo sia
  ' raggiungibile e che il tag esista. SYSTEM.SHELLCODE richiede SET SHELL ON (fatto all'avvio).
  ' Se la verifica fallisce (git non installato / repo non raggiungibile / tag mancante),
  ' il pacchetto resta 'pending' e l'errore è registrato nel report. ---
  DIM gitOk AS LONG
  gitOk = 0
  DIM gitCheck AS STRING
  gitCheck = "git ls-remote --tags " + repoUrl + " " + tagR + " 2>/dev/null"
  DIM gitCode AS LONG
  gitCode = SYSTEM.SHELLCODE(gitCheck)
  IF gitCode = 0 THEN
    ' tag presente? (ls-remote stampa la riga; se tag manca exit 0 ma nessuna riga)
    DIM gitOut AS STRING
    gitOut = SYSTEM.SHELLOUT("git ls-remote --tags " + repoUrl + " | grep -c '" + tagR + "'")
    IF VAL(gitOut) > 0 THEN gitOk = 1
  END IF

  ' salva in repos.json: nome -> { repo, owner, tag, status, verify_report }
  DIM entryR AS JSON
  entryR = NEW JSON
  entryR["name"] = pnameR
  entryR["repo"] = repoUrl
  entryR["owner"] = callerR
  entryR["tag"] = tagR
  entryR["status"] = "active"
  IF verifiedR = 0 THEN entryR["status"] = "pending"
  IF gitOk = 0 THEN entryR["status"] = "pending"   ' verifica indipendente git fallita
  entryR["git_verified"] = STR(gitOk)
  entryR["verify_report"] = reportR
  entryR["when"] = STR(TI())
  gRepos[pnameR] = entryR

  DIM reposStrR AS STRING
  reposStrR = JSON.STRINGIFY(gRepos)
  OPEN reposPath FOR OUTPUT AS #8
  PRINT #8, reposStrR
  CLOSE #8

  ' claim del nome (come al primo publish): il contributor diventa owner del suo scope
  DIM claimR AS JSON
  claimR = gClaims[pnameR]
  IF claimR.name = "" THEN
    DIM newClaimR AS JSON
    newClaimR = NEW JSON
    newClaimR["name"] = pnameR
    newClaimR["owner"] = callerR
    newClaimR["type"] = "scope"
    DIM collsR AS JSON
    collsR = NEW JSON
    newClaimR["collaborators"] = collsR
    gClaims[pnameR] = newClaimR
    DIM claimsStrR AS STRING
    claimsStrR = JSON.STRINGIFY(gClaims)
    OPEN claimsPath FOR OUTPUT AS #9
    PRINT #9, claimsStrR
    CLOSE #9
  END IF

  DIM outR AS JSON
  outR = NEW JSON
  outR["ok"] = "1"
  outR["name"] = pnameR
  outR["repo"] = repoUrl
  outR["status"] = entryR["status"]
  res.SendJSON(outR)
END SUB

' Materializza gli asset binari di un pacchetto contributor (Fase C3bis).
' body JSON { name, version, assets: { "backend/platform": "<base64>" } }.
' Il client scarica gli URL del manifest e li carica qui; il registry li salva in
' registryRoot/<name>/<version>/bin/<backend>-<platform>-<file> così SelectArtifact
' riceve sempre un path locale (mai un URL).
' Nome file sicuro da una chiave asset: "backend/platform" -> "backend_platform".
' Neutralizza / spazio, backslash, ':' e '@' (separatori di path e caratteri non
' portabili su Windows). Il backslash si confronta come CHR$(92): un letterale con
' due backslash sarebbe lungo 2 caratteri e non matcherebbe MAI un char singolo.
FUNCTION AssetFileName(k AS STRING) AS STRING
  DIM si AS LONG, cc AS STRING
  FOR si = 1 TO LEN(k)
    cc = MID(k, si, 1)
    IF cc = "/" OR cc = " " OR cc = CHR$(92) OR cc = ":" OR cc = "@" THEN k = LEFT(k, si - 1) + "_" + MID(k, si + 1)
  NEXT si
  RETURN k
END FUNCTION

SUB HandleApiMaterialize(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM callerM AS STRING
  callerM = ApiAuthUser(req)
  IF callerM = "" THEN
    res.StatusCode = 401
    DIM eM0 AS JSON
    eM0 = NEW JSON
    eM0["error"] = "Unauthorized"
    res.SendJSON(eM0)
    EXIT SUB
  END IF

  IF req.Body = "" THEN
    res.StatusCode = 400
    DIM eM1 AS JSON
    eM1 = NEW JSON
    eM1["error"] = "Body JSON mancante"
    res.SendJSON(eM1)
    EXIT SUB
  END IF

  DIM bodyM AS JSON
  bodyM = JSON.PARSE(req.Body)
  DIM pnameM AS STRING
  pnameM = bodyM.name
  DIM pverM AS STRING
  pverM = bodyM.version
  DIM assetsM AS JSON
  assetsM = bodyM.assets
  IF pnameM = "" OR pverM = "" THEN
    res.StatusCode = 400
    DIM eM2 AS JSON
    eM2 = NEW JSON
    eM2["error"] = "name/version mancanti"
    res.SendJSON(eM2)
    EXIT SUB
  END IF

  DIM binDir AS STRING
  binDir = registryRoot + "/" + pnameM + "/" + pverM + "/bin"
  IF IO.DirExists(binDir) = 0 THEN MKDIR binDir

  DIM savedM AS LONG
  savedM = 0
  DIM aKey AS STRING
  FOR EACH aKey IN JSON.KEYS(assetsM)
    ' key = "backend/platform", valore = base64
    DIM b64M AS STRING
    b64M = assetsM[aKey]
    IF b64M <> "" THEN
      ' nome file sicuro: backend-platform-<idx>.bin (evita / e spazi dal path URL)
      DIM safeKey AS STRING
      safeKey = AssetFileName(aKey)
      DIM rawM() AS BYTE
      rawM = CRYPTOGRAPHY.FROMBASE64(b64M)
      DIM outFileM AS STRING
      outFileM = binDir + "/" + safeKey + ".bin"
      OPEN outFileM FOR BINARY AS #1
      PUT #1, , rawM
      CLOSE #1
      savedM = savedM + 1
    END IF
  NEXT

  DIM outM AS JSON
  outM = NEW JSON
  outM["ok"] = "1"
  outM["name"] = pnameM
  outM["version"] = pverM
  outM["saved"] = STR(savedM)
  outM["dir"] = binDir
  res.SendJSON(outM)
END SUB

' Sincronizza il tag di un pacchetto contributor (Fase C4). body JSON { tag, verify_report? }.
' Aggiorna il tag corrente in repos.json e ricalcola lo status dal report (se presente).
SUB HandleApiSync(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM callerS AS STRING
  callerS = ApiAuthUser(req)
  IF callerS = "" THEN
    res.StatusCode = 401
    DIM eS0 AS JSON
    eS0 = NEW JSON
    eS0["error"] = "Unauthorized"
    res.SendJSON(eS0)
    EXIT SUB
  END IF

  DIM pnameS AS STRING
  pnameS = req.Params["name"]
  DIM entryS AS JSON
  entryS = gRepos[pnameS]
  IF entryS.name = "" THEN
    res.StatusCode = 404
    DIM eS1 AS JSON
    eS1 = NEW JSON
    eS1["error"] = "Pacchetto contributor non trovato: " + pnameS
    res.SendJSON(eS1)
    EXIT SUB
  END IF

  ' solo owner/collaboratore può sincronizzare
  DIM isOwnerS AS LONG
  isOwnerS = 0
  IF entryS.owner = callerS THEN isOwnerS = 1
  DIM isCollabS AS LONG
  isCollabS = 0
  IF isOwnerS = 0 THEN
    DIM ckS AS STRING
    FOR EACH ckS IN JSON.KEYS(gClaims[pnameS].collaborators)
      IF ckS = callerS THEN isCollabS = 1
    NEXT
  END IF
  IF isOwnerS = 0 AND isCollabS = 0 THEN
    res.StatusCode = 403
    DIM eS2 AS JSON
    eS2 = NEW JSON
    eS2["error"] = "Non sei owner/collaboratore di '" + pnameS + "'"
    res.SendJSON(eS2)
    EXIT SUB
  END IF

  DIM bodyS AS JSON
  bodyS = JSON.PARSE(req.Body)
  DIM tagS AS STRING
  tagS = bodyS.tag
  IF tagS = "" THEN
    res.StatusCode = 400
    DIM eS3 AS JSON
    eS3 = NEW JSON
    eS3["error"] = "tag mancante"
    res.SendJSON(eS3)
    EXIT SUB
  END IF

  entryS["tag"] = tagS
  DIM reportS AS JSON
  reportS = bodyS.verify_report
  IF reportS.testExitCode = 0 THEN
    entryS["status"] = "active"
  ELSEIF reportS.testExitCode <> "" THEN
    entryS["status"] = "pending"
  END IF
  entryS["when"] = STR(TI())
  gRepos[pnameS] = entryS

  DIM reposStrS AS STRING
  reposStrS = JSON.STRINGIFY(gRepos)
  OPEN reposPath FOR OUTPUT AS #8
  PRINT #8, reposStrS
  CLOSE #8

  DIM outS AS JSON
  outS = NEW JSON
  outS["ok"] = "1"
  outS["name"] = pnameS
  outS["tag"] = tagS
  outS["status"] = entryS["status"]
  res.SendJSON(outS)
END SUB

' Riserva un nome (blocca il claim automatico finché l'admin non assegna l'owner).
' body JSON { name }. Solo admin. Il claim diventa { type: official, reserved: 1, owner: "" }.
SUB HandleApiAdminReserve(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  IF AdminOk(req, res) = "" THEN
    res.StatusCode = 401
    DIM eAR0 AS JSON
    eAR0 = NEW JSON
    eAR0["error"] = "Admin required"
    res.SendJSON(eAR0)
    EXIT SUB
  END IF
  DIM bodyAR AS JSON
  bodyAR = JSON.PARSE(req.Body)
  DIM pnameAR AS STRING
  pnameAR = bodyAR.name
  IF pnameAR = "" THEN
    res.StatusCode = 400
    DIM eAR1 AS JSON
    eAR1 = NEW JSON
    eAR1["error"] = "name mancante"
    res.SendJSON(eAR1)
    EXIT SUB
  END IF
  DIM claimAR AS JSON
  claimAR = gClaims[pnameAR]
  IF claimAR.name <> "" AND claimAR.owner <> "" THEN
    ' già assegnato: niente da riservare
    res.StatusCode = 409
    DIM eAR2 AS JSON
    eAR2 = NEW JSON
    eAR2["error"] = "Nome già assegnato a: " + claimAR.owner
    res.SendJSON(eAR2)
    EXIT SUB
  END IF
  IF claimAR.name = "" THEN
    claimAR = NEW JSON
    claimAR["name"] = pnameAR
    DIM collsAR AS JSON
    collsAR = NEW JSON
    claimAR["collaborators"] = collsAR
  END IF
  claimAR["type"] = "official"
  claimAR["reserved"] = "1"
  claimAR["owner"] = ""
  gClaims[pnameAR] = claimAR
  DIM claimsStrAR AS STRING
  claimsStrAR = JSON.STRINGIFY(gClaims)
  OPEN claimsPath FOR OUTPUT AS #8
  PRINT #8, claimsStrAR
  CLOSE #8
  DIM outAR AS JSON
  outAR = NEW JSON
  outAR["ok"] = "1"
  outAR["name"] = pnameAR
  outAR["reserved"] = "1"
  res.SendJSON(outAR)
END SUB

' Assegna l'owner di un nome (deve essere riservato o libero). body JSON { name, owner }.
' Solo admin. L'owner riceve il pacchetto in ownedPackages.
SUB HandleApiAdminAssign(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  IF AdminOk(req, res) = "" THEN
    res.StatusCode = 401
    DIM eAA0 AS JSON
    eAA0 = NEW JSON
    eAA0["error"] = "Admin required"
    res.SendJSON(eAA0)
    EXIT SUB
  END IF
  DIM bodyAA AS JSON
  bodyAA = JSON.PARSE(req.Body)
  DIM pnameAA AS STRING
  pnameAA = bodyAA.name
  DIM ownerAA AS STRING
  ownerAA = bodyAA.owner
  IF pnameAA = "" OR ownerAA = "" THEN
    res.StatusCode = 400
    DIM eAA1 AS JSON
    eAA1 = NEW JSON
    eAA1["error"] = "name/owner mancanti"
    res.SendJSON(eAA1)
    EXIT SUB
  END IF
  ' l'owner deve essere un utente registrato
  IF gUsers[ownerAA].name = "" THEN
    res.StatusCode = 404
    DIM eAA2 AS JSON
    eAA2 = NEW JSON
    eAA2["error"] = "Utente non registrato: " + ownerAA
    res.SendJSON(eAA2)
    EXIT SUB
  END IF
  DIM claimAA AS JSON
  claimAA = gClaims[pnameAA]
  IF claimAA.name = "" THEN
    claimAA = NEW JSON
    claimAA["name"] = pnameAA
    DIM collsAA AS JSON
    collsAA = NEW JSON
    claimAA["collaborators"] = collsAA
  END IF
  claimAA["type"] = "official"
  claimAA["owner"] = ownerAA
  claimAA["reserved"] = "0"
  gClaims[pnameAA] = claimAA
  ' ownedPackages dell'owner
  AddOwned(ownerAA, pnameAA)
  DIM claimsStrAA AS STRING
  claimsStrAA = JSON.STRINGIFY(gClaims)
  OPEN claimsPath FOR OUTPUT AS #8
  PRINT #8, claimsStrAA
  CLOSE #8
  cfg["users"] = gUsers
  SaveConfig()
  DIM outAA AS JSON
  outAA = NEW JSON
  outAA["ok"] = "1"
  outAA["name"] = pnameAA
  outAA["owner"] = ownerAA
  res.SendJSON(outAA)
END SUB

' Revoca un claim/ownership (rimuove l'owner, può ri-riservare). body JSON { name, reserve: "1"|"0" }.
' Solo admin. Se reserve=1 il nome resta riservato (niente claim automatico).
SUB HandleApiAdminRevoke(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  IF AdminOk(req, res) = "" THEN
    res.StatusCode = 401
    DIM eAV0 AS JSON
    eAV0 = NEW JSON
    eAV0["error"] = "Admin required"
    res.SendJSON(eAV0)
    EXIT SUB
  END IF
  DIM bodyAV AS JSON
  bodyAV = JSON.PARSE(req.Body)
  DIM pnameAV AS STRING
  pnameAV = bodyAV.name
  IF pnameAV = "" THEN
    res.StatusCode = 400
    DIM eAV1 AS JSON
    eAV1 = NEW JSON
    eAV1["error"] = "name mancante"
    res.SendJSON(eAV1)
    EXIT SUB
  END IF
  DIM claimAV AS JSON
  claimAV = gClaims[pnameAV]
  IF claimAV.name = "" THEN
    res.StatusCode = 404
    DIM eAV2 AS JSON
    eAV2 = NEW JSON
    eAV2["error"] = "Nessun claim per: " + pnameAV
    res.SendJSON(eAV2)
    EXIT SUB
  END IF
  ' togli dagli ownedPackages dell'owner
  IF claimAV.owner <> "" THEN
    RemoveOwned(claimAV.owner, pnameAV)
  END IF
  DIM reserveAV AS STRING
  reserveAV = bodyAV.reserve
  IF reserveAV = "1" THEN
    claimAV["owner"] = ""
    claimAV["reserved"] = "1"
    gClaims[pnameAV] = claimAV
  ELSE
    ' rimuovi del tutto il claim
    gClaims[pnameAV] = ""
  END IF
  DIM claimsStrAV AS STRING
  claimsStrAV = JSON.STRINGIFY(gClaims)
  OPEN claimsPath FOR OUTPUT AS #8
  PRINT #8, claimsStrAV
  CLOSE #8
  cfg["users"] = gUsers
  SaveConfig()
  DIM outAV AS JSON
  outAV = NEW JSON
  outAV["ok"] = "1"
  outAV["name"] = pnameAV
  outAV["removed"] = "1"
  res.SendJSON(outAV)
END SUB

' Cambia il token admin. body JSON { newToken }. Solo admin (con il token attuale).
SUB HandleApiAdminPassword(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  IF IsAdmin(req) = "" THEN
    res.StatusCode = 401
    DIM eAP0 AS JSON
    eAP0 = NEW JSON
    eAP0["error"] = "Admin required"
    res.SendJSON(eAP0)
    EXIT SUB
  END IF
  DIM bodyAP AS JSON
  bodyAP = JSON.PARSE(req.Body)
  DIM newTokAP AS STRING
  newTokAP = bodyAP.newToken
  IF newTokAP = "" THEN
    res.StatusCode = 400
    DIM eAP1 AS JSON
    eAP1 = NEW JSON
    eAP1["error"] = "newToken mancante"
    res.SendJSON(eAP1)
    EXIT SUB
  END IF
  gAdmin["tokenHash"] = CRYPTOGRAPHY.PASSWORDHASH(newTokAP)
  cfg["admin"] = gAdmin
  SaveConfig()
  DIM outAP AS JSON
  outAP = NEW JSON
  outAP["ok"] = "1"
  res.SendJSON(outAP)
END SUB

SUB HandleApiPublish(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  ' Publish (P3): body JSON { name, version, file: <base64 del .gmbpkg>, token }.
  ' Auth (Fase A): richiede un token utente valido (ApiAuthUser).
  DIM ownerName AS STRING
  ownerName = ApiAuthUser(req)
  IF ownerName = "" THEN
    res.StatusCode = 401
    DIM e AS JSON
    e = NEW JSON
    e["error"] = "Unauthorized: token utente mancante o non valido"
    res.SendJSON(e)
    EXIT SUB
  END IF

  IF req.Body = "" THEN
    res.StatusCode = 400
    DIM e0 AS JSON
    e0 = NEW JSON
    e0["error"] = "Body JSON mancante"
    res.SendJSON(e0)
    EXIT SUB
  END IF

  ' Rate-limit per client (#1)
  IF RateLimitOk(req) = "" THEN
    res.StatusCode = 429
    DIM eRl AS JSON
    eRl = NEW JSON
    eRl["error"] = "Rate limit superato (max " + STR(rateLimitPerMinute) + "/min)"
    res.SendJSON(eRl)
    EXIT SUB
  END IF

  ' Quota dimensione upload (#2): il body JSON base64 non deve superare maxPackageBytes.
  ' Base64 = 4/3 dei byte reali: la quota base64 è maxPackageBytes*4/3 + piccolo overhead JSON.
  IF LEN(req.Body) > (maxPackageBytes * 4) \ 3 + 512 THEN
    res.StatusCode = 413
    DIM eSz AS JSON
    eSz = NEW JSON
    eSz["error"] = "Pacchetto troppo grande (max " + STR(maxPackageBytes) + " byte)"
    res.SendJSON(eSz)
    EXIT SUB
  END IF

  DIM payload AS JSON
  payload = JSON.PARSE(req.Body)
  DIM b64 AS STRING
  b64 = payload.file
  DIM pname AS STRING
  pname = payload.name
  DIM pver AS STRING
  pver = payload.version
  IF b64 = "" OR pname = "" OR pver = "" THEN
    res.StatusCode = 400
    DIM e1 AS JSON
    e1 = NEW JSON
    e1["error"] = "file/name/version mancanti"
    res.SendJSON(e1)
    EXIT SUB
  END IF

  ' --- Fase A3: ownership (claim al primo publish) ---
  ' Il claim esiste? Se sì, solo owner/collaboratori possono ripubblicare.
  DIM claim AS JSON
  claim = gClaims[pname]
  ' Nome riservato dall'admin (reserved=1, owner=""): claim automatico BLOCCATO.
  ' Solo l'admin può assegnare l'owner (POST /api/admin/assign).
  IF claim.name <> "" AND claim.reserved = "1" AND claim.owner = "" THEN
    res.StatusCode = 403
    DIM eRsv AS JSON
    eRsv = NEW JSON
    eRsv["error"] = "Nome '" + pname + "' riservato dall'admin (nessun owner assegnato)"
    res.SendJSON(eRsv)
    EXIT SUB
  END IF
  IF claim.name = "" THEN
    ' claim nuovo: chi pubblica per primo lo possiede
    DIM newClaim AS JSON
    newClaim = NEW JSON
    newClaim["name"] = pname
    newClaim["owner"] = ownerName
    newClaim["type"] = "official"
    DIM colls AS JSON
    colls = NEW JSON
    newClaim["collaborators"] = colls
    gClaims[pname] = newClaim
    ' aggiungi il pacchetto agli ownedPackages dell'utente
    AddOwned(ownerName, pname)
  ELSE
    ' claim esistente: verifica owner o collaboratore
    DIM isOwner AS LONG
    isOwner = 0
    IF claim.owner = ownerName THEN isOwner = 1
    DIM isCollab AS LONG
    isCollab = 0
    IF isOwner = 0 THEN
      DIM ck AS STRING
      FOR EACH ck IN JSON.KEYS(claim.collaborators)
        IF ck = ownerName THEN isCollab = 1
      NEXT
    END IF
    IF isOwner = 0 AND isCollab = 0 THEN
      res.StatusCode = 403
      DIM eA3 AS JSON
      eA3 = NEW JSON
      eA3["error"] = "Non sei owner/collaboratore di '" + pname + "'"
      res.SendJSON(eA3)
      EXIT SUB
    END IF
  END IF

  ' --- Fase A4: immutabilità versione ---
  ' Rifiuta il ri-publish della stessa versione (HTTP 409).
  IF IO.DirExists(registryRoot + "/" + pname + "/" + pver) THEN
    res.StatusCode = 409
    DIM eA4 AS JSON
    eA4 = NEW JSON
    eA4["error"] = "Versione già esistente: " + pname + "@" + pver + " (immutabile)"
    res.SendJSON(eA4)
    EXIT SUB
  END IF

  DIM stamp AS STRING
  stamp = STR(TI())
  DIM tmpZip AS STRING
  tmpZip = "./.publish-" + stamp + ".gmbpkg"

  DIM raw() AS BYTE
  raw = CRYPTOGRAPHY.FROMBASE64(b64)
  OPEN tmpZip FOR BINARY AS #1
  PUT #1, , raw
  CLOSE #1

  DIM staging AS STRING
  staging = "./.publish-x-" + stamp
  MKDIR staging
  COMPRESSION.UNZIP(tmpZip, staging)

  ' --- Verifica firma (#3): se l'owner ha una chiave pubblica in config, il pacchetto DEVE
  ' essere firmato con quella chiave (.gmbpkg.sig + .gmbpkg.pub dentro il gmbpkg). ---
  DIM ownerPub AS STRING
  ownerPub = UserPublicKey(ownerName)
  IF ownerPub <> "" THEN
    DIM sigPath AS STRING
    sigPath = staging + "/.gmbpkg.sig"
    DIM pubPath AS STRING
    pubPath = staging + "/.gmbpkg.pub"
    DIM manPath AS STRING
    manPath = staging + "/gmb-package.json"
    IF IO.FileExists(sigPath) = 0 OR IO.FileExists(pubPath) = 0 THEN
      res.StatusCode = 403
      DIM eSig0 AS JSON
      eSig0 = NEW JSON
      eSig0["error"] = "Pacchetto non firmato ma l'utente '" + ownerName + "' richiede la firma"
      res.SendJSON(eSig0)
      EXIT SUB
    END IF
    ' chiave pubblica dentro il pacchetto deve corrispondere a quella dell'owner in config
    DIM pkIn AS STRING
    pkIn = ""
    OPEN pubPath FOR INPUT AS #6
    INPUT #6, pkIn
    CLOSE #6
    pkIn = LTRIM$(RTRIM$(pkIn))
    IF pkIn <> ownerPub THEN
      res.StatusCode = 403
      DIM eSig1 AS JSON
      eSig1 = NEW JSON
      eSig1["error"] = "Chiave pubblica del pacchetto non corrisponde a quella dell'utente '" + ownerName + "'"
      res.SendJSON(eSig1)
      EXIT SUB
    END IF
    ' verifica Ed25519: VERIFYRAW(signature, manifestBytes, chiavePubblicaB64)
    ' (KEYIMPORT accetta solo PEM, non la chiave Ed25519 raw usata da gmb pack)
    DIM sigTxt AS STRING
    sigTxt = ""
    OPEN sigPath FOR INPUT AS #5
    INPUT #5, sigTxt
    CLOSE #5
    DIM manifestBytes() AS BYTE
    manifestBytes = READBYTES(manPath)
    DIM sigBytes() AS BYTE
    sigBytes = CRYPTOGRAPHY.FROMBASE64(LTRIM$(RTRIM$(sigTxt)))
    DIM sigOk AS LONG
    sigOk = 0
    IF CRYPTOGRAPHY.VERIFYRAW(sigBytes, manifestBytes, pkIn) THEN sigOk = 1
    IF sigOk = 0 THEN
      res.StatusCode = 403
      DIM eSig2 AS JSON
      eSig2 = NEW JSON
      eSig2["error"] = "Firma non valida per '" + pname + "'"
      res.SendJSON(eSig2)
      EXIT SUB
    END IF
  END IF

  DIM destDir AS STRING
  destDir = registryRoot + "/" + pname + "/" + pver
  IF IO.DirExists(registryRoot + "/" + pname) = 0 THEN MKDIR registryRoot + "/" + pname
  MKDIR destDir
  COMPRESSION.UNZIP(tmpZip, destDir)
  KILL tmpZip
  RMDIR staging

  ' salva claims.json e la config (persistenza ownership)
  DIM claimsStr AS STRING
  claimsStr = JSON.STRINGIFY(gClaims)
  OPEN claimsPath FOR OUTPUT AS #8
  PRINT #8, claimsStr
  CLOSE #8
  cfg["users"] = gUsers
  SaveConfig()

  ' Aggiorna l'indice in-memory (gPackages) così il registry VIVO serve SUBITO la nuova
  ' versione, senza attendere un riavvio (prima /api/packages restava vuoto fino al restart).
  DIM gp AS JSON
  gp = gPackages[pname]
  IF gp.name = "" THEN
    gp = NEW JSON
    gp["name"] = pname
    DIM vlNew AS JSON
    vlNew = NEW JSON
    gp["versionList"] = vlNew
    gp["versions"] = 0
  END IF
  DIM vl AS JSON
  vl = gp.versionList
  DIM nver AS LONG
  nver = 0
  DIM seen AS LONG
  seen = 0
  DIM vk AS STRING
  FOR EACH vk IN JSON.KEYS(vl)
    nver = nver + 1
    IF vl[vk] = pver THEN seen = 1
  NEXT
  IF seen = 0 THEN
    nver = nver + 1
    vl[LTRIM$(STR(nver))] = pver
  END IF
  gp["versionList"] = vl
  gp["versions"] = nver
  gp["version"] = pver
  gPackages[pname] = gp

  DIM out AS JSON
  out = NEW JSON
  out["ok"] = "1"
  out["name"] = pname
  out["version"] = pver
  out["owner"] = gClaims[pname].owner
  out["path"] = destDir

  ' dist-tag: publish imposta latest sul nuovo pacchetto
  DIM tagsPath AS STRING
  tagsPath = registryRoot + "/" + pname + "/tags.json"
  DIM tags AS JSON
  tags = NEW JSON
  IF IO.FileExists(tagsPath) THEN
    DIM th AS LONG
    th = 0
    JSON OPEN tagsPath FOR "READ" AS #th
    INPUT #th, tags
    CLOSE #th
  END IF
  tags["latest"] = pver
  DIM tagsStr AS STRING
  tagsStr = JSON.STRINGIFY(tags)
  OPEN tagsPath FOR OUTPUT AS #9
  PRINT #9, tagsStr
  CLOSE #9

  res.SendJSON(out)
END SUB

SUB HandleApiTags(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM pname AS STRING
  pname = req.Params["name"]
  DIM tagsPath AS STRING
  tagsPath = registryRoot + "/" + pname + "/tags.json"
  IF IO.FileExists(tagsPath) = 0 THEN
    DIM e AS JSON
    e = NEW JSON
    res.SendJSON(e)
    EXIT SUB
  END IF
  DIM th AS LONG
  th = 0
  JSON OPEN tagsPath FOR "READ" AS #th
  DIM tags AS JSON
  tags = NEW JSON
  INPUT #th, tags
  CLOSE #th
  res.SendJSON(tags)
END SUB

SUB HandleApiSetTag(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM token AS STRING
  token = cfg.token
  IF token <> "" THEN
    IF req.Query["token"] <> token THEN
      res.StatusCode = 401
      DIM e AS JSON
      e = NEW JSON
      e["error"] = "Unauthorized"
      res.SendJSON(e)
      EXIT SUB
    END IF
  END IF

  DIM pname AS STRING
  pname = req.Params["name"]
  IF req.Body = "" THEN
    res.StatusCode = 400
    DIM e0 AS JSON
    e0 = NEW JSON
    e0["error"] = "Body JSON mancante"
    res.SendJSON(e0)
    EXIT SUB
  END IF

  DIM body AS JSON
  body = JSON.PARSE(req.Body)
  DIM tagName AS STRING
  tagName = body.tag
  DIM tagVer AS STRING
  tagVer = body.version
  IF tagName = "" OR tagVer = "" THEN
    res.StatusCode = 400
    DIM e1 AS JSON
    e1 = NEW JSON
    e1["error"] = "tag/version mancanti"
    res.SendJSON(e1)
    EXIT SUB
  END IF

  ' carica tags.json esistente (o vuoto) e aggiorna il tag
  DIM tagsPath AS STRING
  tagsPath = registryRoot + "/" + pname + "/tags.json"
  DIM tags AS JSON
  tags = NEW JSON
  IF IO.FileExists(tagsPath) THEN
    DIM th AS LONG
    th = 0
    JSON OPEN tagsPath FOR "READ" AS #th
    INPUT #th, tags
    CLOSE #th
  END IF
  tags[tagName] = tagVer

  ' salva (build JSON stringa + write)
  DIM tagsStr AS STRING
  tagsStr = JSON.STRINGIFY(tags)
  OPEN tagsPath FOR OUTPUT AS #9
  PRINT #9, tagsStr
  CLOSE #9

  DIM out AS JSON
  out = NEW JSON
  out["ok"] = "1"
  out["name"] = pname
  out[tagName] = tagVer
  res.SendJSON(out)
END SUB

SUB HandleIndex(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.Redirect("/index.bsp")
END SUB
