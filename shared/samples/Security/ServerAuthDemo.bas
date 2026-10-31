' FILE: Security/ServerAuthDemo.bas
' =================================================================================================
'       SECURITY — login su server REALE (sessione + pagina BSP protetta AUTHORIZE)
' =================================================================================================
' Eseguire con:  dotnet run --project REPL/REPL.csproj -- ServerAuthDemo.bas
' Poi aprire nel browser:
'   http://localhost:8092/admin.bsp      -> 401 senza login
'   http://localhost:8092/login?user=admin&pw=admin-pw   -> risponde OK e imposta il cookie
'   http://localhost:8092/admin.bsp      -> 200 (pagina admin protetta, legge SESSION("user"))
'
' Come funziona:
'   1) HTTP CONFIG .Sessions = TRUE abilita le sessioni; .SessionFile persiste su SQLite.
'   2) La route /login valida le credenziali e chiama HTTP.SESSIONCREATE(user, role):
'      il SERVER crea la sessione nello store e restituisce il cookie GMBSESSION.
'   3) La pagina admin.bsp usa <%@ AUTHORIZE admin %>: senza cookie -> 401; ruolo errato -> 403.
' Eseguibile su IL, PCodeVM e VM/C.

IMPORT HTTP

' --- route di login ---
SUB LoginRoute(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM user$ = req.Query("user")
    DIM pw$ = req.Query("pw")

    IF user$ <> "admin" OR pw$ <> "admin-pw" THEN
        res.SetStatus(401)
        res.Send("Unauthorized")
        RETURN
    END IF

    ' il server crea la sessione e restituisce il cookie
    DIM cookie$ = HTTP.SESSIONCREATE(user$, "admin")

    ' separa il valore dal resto ("GMBSESSION=<id>; Path=/; ...")
    DIM eqPos AS LONG = INSTR(cookie$, "=")
    DIM semiPos AS LONG = INSTR(cookie$, ";")
    DIM name$ = LEFT(cookie$, eqPos - 1)
    DIM id$ = MID(cookie$, eqPos + 1, semiPos - eqPos - 1)

    res.SetCookie(name$, id$, "/", TRUE)
    res.Send("login ok, sessione creata per " + user$)
END SUB

' --- MAIN ---
DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
cfg.Port = 8092
cfg.Sessions = TRUE
cfg.SessionFile = "work/sessions.db"
cfg.SecurityHeaders = TRUE
HTTP CONFIG cfg

HTTP ROUTE "GET", "/login", LoginRoute()
HTTP SERVER cfg.Port
