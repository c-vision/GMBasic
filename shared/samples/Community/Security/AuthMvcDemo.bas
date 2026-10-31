' FILE: Security/AuthMvcDemo.bas
' =================================================================================================
'       SECURITY — login MVC class-based (controller + sessione server-side)
' =================================================================================================
' Variante "framework MVC" di ServerAuthDemo.bas: un controller che INHERITS MVC.HttpController
' espone gli endpoint (verb-first, route auto da Linker.Mvc):
'   GET  /api/auth   -> info
'   POST /api/auth   -> crea la sessione (HTTP.SESSIONCREATE) e imposta il cookie GMBSESSION
' Dopo il login apri http://localhost:8094/admin.bsp (pagina BSP protetta <%@ AUTHORIZE admin %>).
' Eseguire con:  dotnet run --project REPL/REPL.csproj -- AuthMvcDemo.bas
' Eseguibile su IL, PCodeVM e VM/C.

IMPORT HTTP
IMPORT JSON
IMPORT MVC

' --- controller (route automatica /api/auth) ---
PUBLIC CLASS AuthController INHERITS MVC.HttpController
    PUBLIC SUB Get()
        SELF.Response.SendJSON({ service = "AuthMvcDemo", login = "POST /api/auth" })
    END SUB

    PUBLIC SUB Post()
        ' il SERVER crea la sessione per l'utente autenticato (demo: credenziali fisse)
        DIM cookie$ = HTTP.SESSIONCREATE("demo", "admin")

        ' imposta il cookie della sessione (GMBSESSION=<id>; ...)
        DIM eqPos AS LONG = INSTR(cookie$, "=")
        DIM semiPos AS LONG = INSTR(cookie$, ";")
        DIM name$ = LEFT(cookie$, eqPos - 1)
        DIM id$ = MID(cookie$, eqPos + 1, semiPos - eqPos - 1)
        SELF.Response.SetCookie(name$, id$, "/", TRUE)

        SELF.Response.SendJSON({ login = "ok", user = "demo", role = "admin" })
    END SUB
END CLASS

' --- MAIN ---
DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
cfg.Port = 8094
cfg.Sessions = TRUE
cfg.SessionFile = "work/sessions.db"
cfg.SecurityHeaders = TRUE
HTTP CONFIG cfg

HTTP SERVER cfg.Port
