' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
' FILE: Ai/AiFullSurfaceCoverage.bas
' AI — SUPERFICIE COMPLETA dei builtin coperti su emitter C# (era "builtin non implementato").
' Esercita in un unico programma: CATEGORY (set/list/pick/auto/fallback), HOOK
' (register/list/fire/events/unregister), ROTATE, ASKCHAIN, WORKFLOWLIST, TEAMLIST,
' SEARCH e COUNT.
'
' Determinismo: le funzioni dipendenti da provider/DB sono verificate per ESISTENZA e
' CHIAMABILITÀ (IIF su LEN ≥ 0), non sul loro contenuto — così le stesse righe girano con lo
' stesso output su IL, PCodeVM, VM/C, C99Native (i provider FIXTURE dell'harness
' IL non esistono nelle gambe C, che leggono il registry persistito).
'
' Guida: Guide/utente/AI-Namespace-Manual.md (§ Copertura per namespace).
IMPORT AI

SUB Main()
    PRINT "=== AI surface: CATEGORY / HOOK / ROTATE / ASKCHAIN / WORKFLOW / TEAM / VECTOR ==="

    ' --- CATEGORY: categorizzazione locale dei prompt ---
    PRINT "cat_set:      "; AI.CATEGORYSET("studio", "fixture-completion")
    PRINT "cat_list_ok:  "; IIF(LEN(AI.CATEGORYLIST()) >= 0, 1, 0)
    PRINT "cat_pick_ok:  "; IIF(LEN(AI.CATEGORYPICK("studio")) >= 0, 1, 0)
    PRINT "cat_auto:     "; AI.CATEGORYAUTO(1)
    PRINT "cat_fallback: "; AI.CATEGORYFALLBACK("studio", "fixture-completion")

    ' --- HOOK: eventi interni del runtime agente ---
    PRINT "hook_reg:     "; AI.HOOKREGISTER("pre", "Handler")
    PRINT "hook_list_ok: "; IIF(LEN(AI.HOOKLIST()) >= 0, 1, 0)
    PRINT "hook_fire_ok: "; IIF(LEN(AI.HOOKFIRE("pre", "{}")) >= 0, 1, 0)
    PRINT "hook_events:  "; IIF(LEN(AI.HOOKEVENTS()) >= 0, 1, 0)
    PRINT "hook_unreg:   "; AI.HOOKUNREGISTER("pre", "Handler")

    ' --- ROTATE / WORKFLOW / TEAM ---
    PRINT "rotate_ok:    "; IIF(LEN(AI.ROTATE("x")) >= 0, 1, 0)
    PRINT "workflow_ok:  "; IIF(LEN(AI.WORKFLOWLIST()) >= 0, 1, 0)
    PRINT "team_ok:      "; IIF(LEN(AI.TEAMLIST()) >= 0, 1, 0)

    ' --- Provider/DB-dependent: esistenza + chiamabilità (contenuto non deterministico) ---
    PRINT "askchain_ok:  "; IIF(LEN(AI.ASKCHAIN("x", "x", "x")) >= 0, 1, 0)
    ' ── vettoriale: serve un provider EMBEDDING (ruolo distinto da "completion", quindi
    ' non interferisce con la famiglia LLM-dipendente) + documenti indicizzati, come nei MiniTest ──
    AI SET ACTIVE %EMBEDDING, "fixture-embedding"
    AI.INDEXMETA(":memory:", "ixf", "d1", "vacation policy allows 25 days", "")
    PRINT "search_ok:    "; IIF(LEN(AI.SEARCH(":memory:", "ixf", "vacation policy", 3, 1)) >= 0, 1, 0)
    PRINT "count_ok:     "; IIF(AI.COUNT(":memory:", "c") >= -1, 1, 0)

' ── famiglia LLM-dipendente (blocco 1): senza provider l'oracolo IL è un errore
' esplicito e TUTTE le gambe devono concordare ──
    PRINT "enrich:      "; IIF(LEN(AI.ENRICH("x", "x")) >= 0, 1, 0)
    PRINT "suggesttags: "; IIF(LEN(AI.SUGGESTTAGS("x")) >= 0, 1, 0)
    PRINT "brainwrite:  "; IIF(LEN(AI.BRAINVAULTWRITE("x", 1)) >= 0, 1, 0)
    PRINT "qaanswer:    "; IIF(LEN(AI.QAANSWER("x", "x", 1)) >= 0, 1, 0)


' ── resto della superficie implementata: memory/brain, orchestrazione, vision
' (path inesistente = errore deterministico) e link del secondo cervello ──
    PRINT "memoryrank:       "; IIF(LEN(AI.MEMORYRANK("x", 1)) >= 0, 1, 0)
    PRINT "memoryconsolidate:"; IIF(AI.MEMORYCONSOLIDATE() >= -1, 1, 0)
    PRINT "orchload:         "; IIF(AI.ORCHLOAD("/tmp/gmb_orch_test.db") >= -1, 1, 0)
    PRINT "analyzeimage:     "; IIF(LEN(AI.ANALYZEIMAGE("/tmp/gmb_mt_analyzeimage_absent.png", "x")) >= 0, 1, 0)
    PRINT "noteunlink:       "; IIF(AI.NOTEUNLINK("x", "x", "x") >= -1, 1, 0)
    PRINT "notesetalias:     "; IIF(AI.NOTESETALIAS("x", "x", "x", "x") >= -1, 1, 0)

' ── secondo cervello: vault / note / link (stato su nomi inesistenti = deterministico) ──
    PRINT "linksout:      "; IIF(LEN(CSTR(AI.LINKSOUT("x", "x"))) >= 0, 1, 0)
    PRINT "linkjson:      "; IIF(LEN(CSTR(AI.LINKJSON("x"))) >= 0, 1, 0)
    PRINT "linkdangling:  "; IIF(LEN(CSTR(AI.LINKDANGLING("x"))) >= 0, 1, 0)
    PRINT "backlinks:     "; IIF(LEN(CSTR(AI.BACKLINKS("x", "x"))) >= 0, 1, 0)
    PRINT "notelink:      "; IIF(LEN(CSTR(AI.NOTELINK("x", "x", "x", "x"))) >= 0, 1, 0)
    PRINT "noteget:       "; IIF(LEN(CSTR(AI.NOTEGET("x", "x"))) >= 0, 1, 0)
    PRINT "noteexists:    "; IIF(LEN(CSTR(AI.NOTEEXISTS("x", "x"))) >= 0, 1, 0)
    PRINT "notetags:      "; IIF(LEN(CSTR(AI.NOTETAGS("x", "x"))) >= 0, 1, 0)
    PRINT "notetitle:     "; IIF(LEN(CSTR(AI.NOTETITLE("x", "x"))) >= 0, 1, 0)
    PRINT "notewordcount: "; IIF(LEN(CSTR(AI.NOTEWORDCOUNT("x", "x"))) >= 0, 1, 0)
    PRINT "notefolder:    "; IIF(LEN(CSTR(AI.NOTEFOLDER("x", "x"))) >= 0, 1, 0)

' ── Task Graph e Plan ──
    PRINT "taskdefine:    "; IIF(LEN(CSTR(AI.TASKDEFINE("x", "x"))) >= 0, 1, 0)
    PRINT "taskstatus:    "; IIF(LEN(CSTR(AI.TASKSTATUS("x"))) >= 0, 1, 0)
    PRINT "taskrun:       "; IIF(LEN(CSTR(AI.TASKRUN())) >= 0, 1, 0)
    PRINT "tasklist:      "; IIF(LEN(CSTR(AI.TASKLIST())) >= 0, 1, 0)
    PRINT "taskdepends:   "; IIF(LEN(CSTR(AI.TASKDEPENDS("x", "x"))) >= 0, 1, 0)
    PRINT "taskassign:    "; IIF(LEN(CSTR(AI.TASKASSIGN("x", "x"))) >= 0, 1, 0)
    PRINT "plandefine:    "; IIF(LEN(CSTR(AI.PLANDEFINE("x", "x"))) >= 0, 1, 0)
    PRINT "planstatus:    "; IIF(LEN(CSTR(AI.PLANSTATUS("x"))) >= 0, 1, 0)
    PRINT "planrun:       "; IIF(LEN(CSTR(AI.PLANRUN("x"))) >= 0, 1, 0)
    PRINT "planlist:      "; IIF(LEN(CSTR(AI.PLANLIST())) >= 0, 1, 0)
    PRINT "planadd:       "; IIF(LEN(CSTR(AI.PLANADD("x", "x"))) >= 0, 1, 0)
    PRINT "planapprove:   "; IIF(LEN(CSTR(AI.PLANAPPROVE("x"))) >= 0, 1, 0)
    PRINT "vsetprops:   "; IIF(AI.VAULTSETFOLDERPROPS("x", "x", "x") >= -1, 1, 0)
    PRINT "vaultwatch:  "; IIF(AI.VAULTWATCH("x", "x") >= -1, 1, 0)
    PRINT "skillsprompt:"; IIF(LEN(AI.SKILLSPROMPT()) > 0, 1, 0)
    PRINT "hfrepo:      "; IIF(LEN(AI.HFREPO()) >= 0, 1, 0)
END SUB
