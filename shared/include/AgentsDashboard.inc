' FILE: include/AgentsDashboard.inc
' =================================================================================================
'   DASHBOARD — helper BASIC per la WEBAPP Agents Dashboard (C1b, 3 tab)
' =================================================================================================
' Libreria di sistema PURA (BASIC, cross-backend: IL / PCodeVM / VM/C / C99 / C99Native).
' Fornisce costanti editabili e helper di RENDERING per la dashboard agenti. NON contiene
' HTML obbligatorio: le pagine .bsp (webapp/pages/index.bsp) restano la sorgente di verità del
' markup e usano questi helper solo per generare frammenti. L'utente modifica qui colori,
' badge, intervalli, formati — senza toccare C#.
'
' Uso:
'   IMPORT DASHBOARD
'   DIM badge AS STRING = DASHBOARD.StatusBadgeHtml("Idle")
'   DIM row AS STRING   = DASHBOARD.AgentRowHtml("Support", "Idle", "YOLO", "TeamA")
'   DIM eRow AS STRING  = DASHBOARD.EventRowHtml("2026-09-08T07:00:00Z", "Asked", "Support", "prompt=ciao")
'
' Dipendenze: nessuna obbligatoria. Usa solo STRING/JSON/ARRAY.
'
' Shape JSON attesa (AgentStatusManager, IlRuntime.cs:7769):
'   GET /api/agents → [{name, status, permission}]  (V1, 3 campi)
'   GET /api/teams  → [{name, strategy, memberCount}]
'   POST /api/agent/{n}/prompt → {agent, answer}
' =================================================================================================

NAMESPACE DASHBOARD

' --- Costanti editabili (tema + timing) ---

PUBLIC CONST TITLE$ = "GMBasic — Agents Operating Center"
PUBLIC CONST SUBTITLE$ = "3 tab: Agents · Teams · Log · Canvas · WASM"
PUBLIC CONST REFRESH_MS% = 2000
PUBLIC CONST API_BASE$ = "/api"
PUBLIC CONST PROTOCOL_VERSION% = 1
PUBLIC CONST COLOR_HEADER_FROM$ = "#1e293b"
PUBLIC CONST COLOR_HEADER_TO$ = "#334155"
PUBLIC CONST COLOR_PRIMARY$ = "#2563eb"
PUBLIC CONST COLOR_BG$ = "#f4f6fb"
PUBLIC CONST COLOR_CARD$ = "#ffffff"
PUBLIC CONST COLOR_BORDER$ = "#e2e8f0"

' --- Rendering helper: badge ---

PUBLIC FUNCTION StatusBadgeHtml(status AS STRING) AS STRING
    IF status = "Idle" OR status = "idle" OR status = "IDLE" THEN RETURN "<span class='badge idle'>Idle</span>"
    IF status = "Working" OR status = "working" OR status = "WORKING" THEN RETURN "<span class='badge working'>Working</span>"
    IF status = "Planning" OR status = "planning" THEN RETURN "<span class='badge planning'>Planning</span>"
    IF status = "Waiting" OR status = "waiting" THEN RETURN "<span class='badge waiting'>Waiting</span>"
    IF status = "Executing" OR status = "executing" THEN RETURN "<span class='badge working'>Executing</span>"
    RETURN "<span class='badge'>" + status + "</span>"
END FUNCTION

PUBLIC FUNCTION PermissionBadgeHtml(perm AS STRING) AS STRING
    IF perm = "YOLO" OR perm = "yolo" THEN RETURN "<span class='badge perm-yolo'>YOLO</span>"
    IF perm = "Supervised" OR perm = "supervised" OR perm = "SUPERVISED" THEN RETURN "<span class='badge perm-supervised'>Supervised</span>"
    IF perm = "Sandbox" OR perm = "sandbox" OR perm = "SANDBOX" THEN RETURN "<span class='badge perm-sandbox'>Sandbox</span>"
    RETURN "<span class='badge'>" + perm + "</span>"
END FUNCTION

PUBLIC FUNCTION StrategyBadgeHtml(strat AS STRING) AS STRING
    RETURN "<span class='badge strategy'>" + strat + "</span>"
END FUNCTION

' --- Rendering helper: righe tabella ---

PUBLIC FUNCTION AgentRowHtml(name AS STRING, status AS STRING, permission AS STRING, teams AS STRING) AS STRING
    IF teams = "" THEN teams = "-"
    RETURN "<tr><td class='name'>" + name + "</td><td>" + StatusBadgeHtml(status) + "</td><td>" + PermissionBadgeHtml(permission) + "</td><td class='teams'>" + teams + "</td><td><button class='btn btn-ghost' onclick=""chat('" + name + "')"">Chat</button></td></tr>"
END FUNCTION

PUBLIC FUNCTION AgentRowRichHtml(name AS STRING, status AS STRING, permission AS STRING, teams AS STRING, model AS STRING) AS STRING
    IF teams = "" THEN teams = "-"
    IF model = "" THEN model = "-"
    RETURN "<tr><td class='name'>" + name + "</td><td>" + StatusBadgeHtml(status) + "</td><td>" + PermissionBadgeHtml(permission) + "</td><td class='teams'>" + teams + "</td><td class='model ver'>" + model + "</td><td><button class='btn btn-ghost' onclick=""chat('" + name + "')"">Chat</button></td></tr>"
END FUNCTION

PUBLIC FUNCTION TeamRowHtml(name AS STRING, strategy AS STRING, memberCount AS LONG) AS STRING
    RETURN "<tr><td class='name'>" + name + "</td><td>" + StrategyBadgeHtml(strategy) + "</td><td class='count'>" + LTRIM$(STR$(memberCount)) + "</td><td><button class='btn btn-ghost' onclick=""teamChat('" + name + "')"">Broadcast</button></td></tr>"
END FUNCTION

PUBLIC FUNCTION EventRowHtml(ts AS STRING, kind AS STRING, agent AS STRING, detail AS STRING) AS STRING
    RETURN "<tr><td class='ts'>" + ts + "</td><td><span class='badge'>" + kind + "</span></td><td>" + agent + "</td><td class='detail'>" + detail + "</td></tr>"
END FUNCTION

' --- Helper: celle singole ---

PUBLIC FUNCTION TeamsCellHtml(teams AS STRING) AS STRING
    IF teams = "" THEN RETURN "<span class='muted'>-</span>"
    RETURN teams
END FUNCTION

' --- Helper: tempo ---

PUBLIC FUNCTION TimeAgo(iso AS STRING) AS STRING
    RETURN iso
END FUNCTION

PUBLIC FUNCTION ShortTime(iso AS STRING) AS STRING
    DIM t AS LONG = INSTR(iso, "T")
    IF t > 0 THEN RETURN MID$(iso, t + 1, 8)
    RETURN iso
END FUNCTION

' --- Helper: JSON → HTML (per debug) ---

PUBLIC FUNCTION JsonToPreHtml(jsonStr AS STRING) AS STRING
    RETURN "<pre class='json'>" + jsonStr + "</pre>"
END FUNCTION

' --- Helper: empty state ---

PUBLIC FUNCTION EmptyRowHtml(colspan AS LONG, msg AS STRING) AS STRING
    RETURN "<tr><td colspan='" + LTRIM$(STR$(colspan)) + "' class='empty'>" + msg + "</td></tr>"
END FUNCTION
