' FILE: shared/APPS/RuleFlow/src/RuleFlow.bas
' ============================================================================
' RuleFlow (GMBasic port) — modellare, evolvere e analizzare sistemi discreti
' con un GRAFO CAUSALE esplicito.
'
' Port fedele dell'OSSATURA headless di https://github.com/RuleFlow-OSS/RuleFlow (MIT):
'   script FlowLang -> universo (celle) -> applicazione regole -> DELTA per evento
'   -> grafo causale (archi origine -> evento) -> metriche (nodi, archi, densità,
'      cammino massimo nel DAG).
'
' M1 (headless), operatori di riscrittura:
'   pat -> dst     substitution (il selettore è RIMPIAZZATO dal target)
'   pat --> dst    overwrite    (sovrascrive le celle del selettore, stessa lunghezza)
'   pat > dst      insertion    (inserisce dst PRIMA del selettore, selettore intatto)
'   pat ><         deletion     (rimuove il selettore)
' Direttive: @init("TEST0"), @evolve(n). Commenti: #.
'
' NON implementati in M1 (falliscono con ERRORE ESPLICITO, mai ignorati in silenzio):
'   flag -pl/-bl/-cmp/-crp/-p_rule/-p_space/-life/-sr, branch multiway,
'   @regress/@merge/@compress/@macro, bootstrap Python/Wolfram (.pflow/.wpflow).
'   Vedi TODO.md (M2/M3).
'
' MODI (simboli compile-time; BASIC non ha argv):
'   (default)                 status: banner + config + contatori
'   -DRULEFLOW_MODE_RUN       evolvi `steps` tick, stampa gli eventi e lo spazio finale
'   -DRULEFLOW_MODE_GRAPH     evolvi e stampa le metriche del grafo causale
'   -DRULEFLOW_MODE_CHECK     diagnostica: config, regole, evoluzione, DAG, metriche
'
' RUN (dalla ROOT del monorepo):
'   tools/sh/harness.sh pcode shared/APPS/RuleFlow/src/RuleFlow.bas
'   # modo diverso: aggiungere in testa al file  #DEF RULEFLOW_MODE_GRAPH
' ============================================================================
IMPORT IO
IMPORT STRINGS

' --- Limiti M1 (array fissi: parità 4 gambe, nessun contenitore dinamico) ----
CONST RF_MAX_CELLS = 512
CONST RF_MAX_RULES = 16
CONST RF_MAX_EVENTS = 4000
CONST RF_MAX_EDGES = 12000
CONST RF_MAX_HIT = 64

' --- Stato della app --------------------------------------------------------
DIM g_appDir AS STRING
DIM g_projectFile AS STRING
DIM g_cfgMode AS STRING
DIM g_flowFile AS STRING
DIM g_steps AS INTEGER
DIM g_verbose AS STRING

' --- Universo: la stringa È lo spazio (1 carattere = 1 cella); ID univoco e
'     "origine" (evento che l'ha creata, 0 = generazione zero) sono array
'     paralleli alla stringa, indice 1-based. --------------------------------
DIM g_space AS STRING
DIM g_cId(RF_MAX_CELLS) AS LONG
DIM g_cOrg(RF_MAX_CELLS) AS LONG
DIM g_nextCellId AS LONG

' --- Regole ----------------------------------------------------------------
DIM g_rPat(RF_MAX_RULES) AS STRING
DIM g_rDst(RF_MAX_RULES) AS STRING
DIM g_rOp(RF_MAX_RULES) AS INTEGER      ' 1=-> 2=--> 3=> 4=><
DIM g_nRules AS INTEGER

' --- Eventi e archi causali -------------------------------------------------
DIM g_evTick(RF_MAX_EVENTS) AS INTEGER
DIM g_evRule(RF_MAX_EVENTS) AS INTEGER
DIM g_evPos(RF_MAX_EVENTS) AS INTEGER
DIM g_nEvents AS INTEGER
DIM g_eFrom(RF_MAX_EDGES) AS INTEGER
DIM g_eTo(RF_MAX_EDGES) AS INTEGER
DIM g_nEdges AS INTEGER

' --- Celle toccate dall'evento in corso (origine, per gli archi) ------------
DIM g_hitOrg(RF_MAX_HIT) AS LONG
DIM g_nHit AS INTEGER

' --- Contatori evoluzione ---------------------------------------------------
DIM g_tick AS INTEGER
DIM g_applied AS LONG

' --- Metriche del grafo causale --------------------------------------------
DIM g_nodes AS INTEGER
DIM g_longest AS INTEGER
DIM g_density AS SINGLE

' ============================================================================
SUB Main()
    BootPaths()
    ConfigureDefaults()
    PrintBanner()
    LoadSettings()

    DIM mode AS STRING
    mode = ComputeMode()
    IF mode = "STATUS" AND LEN(g_cfgMode) > 0 THEN mode = g_cfgMode

    IF mode = "RUN" THEN
        RunFlow()
    ELSEIF mode = "GRAPH" THEN
        RunGraph()
    ELSEIF mode = "CHECK" THEN
        RunCheck()
    ELSE
        PrintStatus()
    END IF
END SUB

FUNCTION ComputeMode() AS STRING
    DIM m AS STRING
    m = "STATUS"
#IFDEF RULEFLOW_MODE_RUN
    m = "RUN"
#ENDIF
#IFDEF RULEFLOW_MODE_GRAPH
    m = "GRAPH"
#ENDIF
#IFDEF RULEFLOW_MODE_CHECK
    m = "CHECK"
#ENDIF
    ComputeMode = m
END FUNCTION

SUB BootPaths()
    DIM srcDir AS STRING, appDir AS STRING
    srcDir = PROJECTFOLDER()
    IF LEN(srcDir) = 0 THEN srcDir = IO.CWD()
    appDir = IO.PARENTDIR(RTrimSlash(srcDir))
    IF LEN(appDir) = 0 THEN appDir = srcDir
    ' Nessun side effect su disco: la app è pura computazione in memoria (niente bin/,
    ' niente work/: vedi AGENTS §11.1).
    g_appDir = RTrimSlash(appDir)
END SUB

SUB ConfigureDefaults()
    g_cfgMode = ""
    g_flowFile = "flows/sss.flow"
    g_steps = 12
    g_verbose = "TRUE"
END SUB

SUB PrintBanner()
    PRINT "╔══════════════════════════════════════════════════════════════════╗"
    PRINT "║  RuleFlow (GMBasic port) — rewrite systems + causal graph           ║"
    PRINT "║  upstream: github.com/RuleFlow-OSS/RuleFlow  (MIT)                  ║"
    PRINT "╚══════════════════════════════════════════════════════════════════╝"
    PRINT ""
END SUB

' ============================================================================
' Config: .ruleflow.toml (sottoinsieme TOML, come le altre app del monorepo)
' ============================================================================
SUB LoadSettings()
    DIM dir AS STRING, candidate AS STRING
    DIM i AS INTEGER
    dir = IO.CWD()
    FOR i = 1 TO 8
        candidate = RTrimSlash(dir) + "/.ruleflow.toml"
        IF IO.FILEEXISTS(candidate) THEN
            g_projectFile = candidate
            ParseSettingsFile(candidate)
            i = 8
        ELSE
            dir = IO.PARENTDIR(dir)
            IF LEN(dir) = 0 THEN i = 8
        END IF
    NEXT i
END SUB

SUB ParseSettingsFile(path AS STRING)
    DIM lines
    DIM raw AS STRING, section AS STRING, key AS STRING, val AS STRING
    DIM eq AS INTEGER, i AS LONG
    lines = IO.READLINES(path)
    section = ""
    FOR i = 0 TO UBOUND(lines)
        raw = STRINGS.TRIM(lines(i))
        IF LEN(raw) > 0 AND NOT STRINGS.STARTSWITH(raw, "#") THEN
            IF STRINGS.STARTSWITH(raw, "[") THEN
                section = STRINGS.UPPER(BracketInner(raw))
            ELSE
                eq = INSTR(raw, "=")
                IF eq > 1 THEN
                    key = STRINGS.UPPER(STRINGS.TRIM(LEFT$(raw, eq - 1)))
                    val = Unquote(STRINGS.TRIM(MID$(raw, eq + 1, LEN(raw) - eq)))
                    IF section = "SETTINGS" AND key = "MODE" THEN g_cfgMode = STRINGS.UPPER(val)
                    IF section = "FLOW" THEN
                        IF key = "FILE" THEN g_flowFile = val
                        IF key = "STEPS" THEN g_steps = CINT(VAL(val))
                    END IF
                    IF section = "RUN" AND key = "VERBOSE" THEN g_verbose = STRINGS.UPPER(val)
                END IF
            END IF
        END IF
    NEXT i
END SUB

FUNCTION BracketInner(s AS STRING) AS STRING
    BracketInner = STRINGS.TRIM(MID$(s, 2, LEN(s) - 2))
END FUNCTION

FUNCTION Unquote(s AS STRING) AS STRING
    DIM t AS STRING, q AS STRING
    q = CHR$(34)
    t = s
    IF LEN(t) >= 2 THEN
        IF STRINGS.STARTSWITH(t, q) AND STRINGS.ENDSWITH(t, q) THEN
            t = MID$(t, 2, LEN(t) - 2)
        END IF
    END IF
    Unquote = STRINGS.TRIM(t)
END FUNCTION

FUNCTION RTrimSlash(p AS STRING) AS STRING
    DIM s AS STRING
    s = p
    IF STRINGS.ENDSWITH(s, "/") THEN s = STRINGS.LEFT(s, LEN(s) - 1)
    RTrimSlash = s
END FUNCTION

FUNCTION FlowPath() AS STRING
    DIM p AS STRING
    p = g_flowFile
    IF STRINGS.STARTSWITH(p, "/") THEN
        FlowPath = p
    ELSE
        FlowPath = g_appDir + "/" + p
    END IF
END FUNCTION

SUB Fail(msg AS STRING)
    PRINT "errore: "; msg
    ERROR 5
END SUB

' ============================================================================
' MODE: STATUS
' ============================================================================
SUB PrintStatus()
    PRINT "project file : "; g_projectFile
    PRINT "app dir      : "; g_appDir
    PRINT "flow         : "; FlowPath(); "  ("; FlowStateText(); ")"
    PRINT "steps        : "; g_steps
    PRINT "verbose      : "; g_verbose
    PRINT "limiti       : celle "; RF_MAX_CELLS; " | regole "; RF_MAX_RULES; " | eventi "; RF_MAX_EVENTS
    PRINT ""
    PRINT "modi         : -DRULEFLOW_MODE_RUN | _GRAPH | _CHECK"
END SUB

FUNCTION FlowStateText() AS STRING
    IF IO.FILEEXISTS(FlowPath()) THEN
        FlowStateText = "presente"
    ELSE
        FlowStateText = "assente"
    END IF
END FUNCTION

' ============================================================================
' FlowLang M1 — caricamento dello script
' ============================================================================
SUB LoadFlow()
    DIM lines
    IF NOT IO.FILEEXISTS(FlowPath()) THEN Fail("flow assente: " + FlowPath())
    DIM raw AS STRING
    DIM i AS LONG
    g_nRules = 0
    g_space = ""
    lines = IO.READLINES(FlowPath())
    FOR i = 0 TO UBOUND(lines)
        raw = STRINGS.TRIM(lines(i))
        IF LEN(raw) > 0 AND NOT STRINGS.STARTSWITH(raw, "#") THEN
            IF STRINGS.STARTSWITH(raw, "@") THEN
                ParseDirective(raw)
            ELSE
                ParseRule(raw)
            END IF
        END IF
    NEXT i
    IF LEN(g_space) = 0 THEN g_space = "A"
    SpaceInit()
END SUB

SUB ParseDirective(line AS STRING)
    DIM up AS STRING, inner AS STRING
    up = STRINGS.UPPER(line)
    IF STRINGS.STARTSWITH(up, "@INIT") THEN
        inner = ParenInner(line)
        g_space = Unquote(STRINGS.TRIM(inner))
        IF LEN(g_space) = 0 THEN g_space = "A"
    ELSEIF STRINGS.STARTSWITH(up, "@EVOLVE") THEN
        inner = ParenInner(line)
        g_steps = CINT(VAL(STRINGS.TRIM(inner)))
        IF g_steps < 0 THEN g_steps = 0
    ELSE
        Fail("direttiva non supportata in M1: " + line + "  (ammesse @init e @evolve; vedi TODO M2)")
    END IF
END SUB

FUNCTION ParenInner(line AS STRING) AS STRING
    DIM a AS INTEGER, b AS INTEGER
    a = INSTR(line, "(")
    b = INSTR(line, ")")
    IF a > 0 AND b > a THEN
        ParenInner = MID$(line, a + 1, b - a - 1)
    ELSE
        ParenInner = ""
    END IF
END FUNCTION

SUB ParseRule(line AS STRING)
    DIM s AS STRING, pat AS STRING, dst AS STRING
    DIM op AS INTEGER, cut AS INTEGER
    s = STRINGS.TRIM(line)
    IF STRINGS.ENDSWITH(s, ";") THEN s = STRINGS.TRIM(LEFT$(s, LEN(s) - 1))
    IF HasFlagSyntax(s) THEN
        Fail("flag FlowLang non supportate in M1: " + s + "  (vedi TODO M2)")
    END IF
    ' Ordine: prima gli operatori LUNGHI (--> e ><), poi -> e >
    cut = INSTR(s, "-->")
    IF cut > 0 THEN
        op = 2
        pat = STRINGS.TRIM(LEFT$(s, cut - 1))
        dst = STRINGS.TRIM(MID$(s, cut + 3, LEN(s) - cut - 2))
    ELSE
        cut = INSTR(s, "><")
        IF cut > 0 THEN
            op = 4
            pat = STRINGS.TRIM(LEFT$(s, cut - 1))
            dst = ""
        ELSE
            cut = INSTR(s, "->")
            IF cut > 0 THEN
                op = 1
                pat = STRINGS.TRIM(LEFT$(s, cut - 1))
                dst = STRINGS.TRIM(MID$(s, cut + 2, LEN(s) - cut - 1))
            ELSE
                cut = INSTR(s, ">")
                IF cut > 0 THEN
                    op = 3
                    pat = STRINGS.TRIM(LEFT$(s, cut - 1))
                    dst = STRINGS.TRIM(MID$(s, cut + 1, LEN(s) - cut))
                END IF
            END IF
        END IF
    END IF
    IF op = 0 THEN Fail("regola non riconosciuta (attesi -> , --> , > , ><): " + s)
    IF LEN(pat) = 0 THEN Fail("regola con selettore vuoto: " + s)
    IF op <> 4 AND LEN(dst) = 0 THEN Fail("regola con target vuoto (per cancellare usa ><): " + s)
    IF op = 2 AND LEN(pat) <> LEN(dst) THEN Fail("overwrite --> richiede target della stessa lunghezza del selettore: " + s)
    IF g_nRules >= RF_MAX_RULES THEN Fail("troppe regole (max " + RTrimNum(RF_MAX_RULES) + ")")
    g_nRules = g_nRules + 1
    g_rPat(g_nRules) = STRINGS.UPPER(pat)
    IF op = 4 THEN
        g_rDst(g_nRules) = ""
    ELSE
        g_rDst(g_nRules) = STRINGS.UPPER(dst)
    END IF
    g_rOp(g_nRules) = op
END SUB

FUNCTION HasFlagSyntax(s AS STRING) AS INTEGER
    ' Riconosce SOLO le forme di flag reali di FlowLang (`-nome[`): una scansione
    ' generica di "-" darebbe falsi positivi su simboli come `a-b` (minuscolo) dentro
    ' una regola. Case-insensitive.
    DIM low AS STRING, found AS INTEGER
    low = STRINGS.LOWER(s)
    found = 0
    IF INSTR(low, "-pl[") > 0 THEN found = 1
    IF INSTR(low, "-bl[") > 0 THEN found = 1
    IF INSTR(low, "-cmp[") > 0 THEN found = 1
    IF INSTR(low, "-crp[") > 0 THEN found = 1
    IF INSTR(low, "-p_rule[") > 0 THEN found = 1
    IF INSTR(low, "-p_space[") > 0 THEN found = 1
    IF INSTR(low, "-life[") > 0 THEN found = 1
    IF INSTR(low, "-sr[") > 0 THEN found = 1
    IF INSTR(low, "-g[") > 0 THEN found = 1
    IF INSTR(low, "-gb[") > 0 THEN found = 1
    HasFlagSyntax = found
END FUNCTION

FUNCTION RTrimNum(n AS LONG) AS STRING
    RTrimNum = LTRIM$(STR$(n))
END FUNCTION

' ============================================================================
' Universo (stringa = spazio; array paralleli per id/origine)
' ============================================================================
SUB SpaceInit()
    DIM i AS INTEGER
    IF LEN(g_space) > RF_MAX_CELLS THEN
        g_space = LEFT$(g_space, RF_MAX_CELLS)
        PRINT "attenzione: @init troncato a "; RF_MAX_CELLS; " celle"
    END IF
    g_nextCellId = 0
    FOR i = 1 TO LEN(g_space)
        g_nextCellId = g_nextCellId + 1
        g_cId(i) = g_nextCellId
        g_cOrg(i) = 0
    NEXT i
END SUB

' Inserisce `ins` PRIMA della posizione pos. Le celle inserite sono nuove
' (id nuovo, origine = evento in corso). Il resto scala a destra.
SUB SpaceInsert(pos AS INTEGER, ins AS STRING)
    DIM k AS INTEGER, m AS INTEGER, add AS INTEGER
    DIM oldSpace AS STRING
    DIM oldId(RF_MAX_CELLS) AS LONG
    DIM oldOrg(RF_MAX_CELLS) AS LONG
    add = LEN(ins)
    IF add = 0 THEN EXIT SUB
    IF LEN(g_space) + add > RF_MAX_CELLS THEN Fail("universo oltre il limite di " + RTrimNum(RF_MAX_CELLS) + " celle")
    FOR k = 1 TO LEN(g_space)
        oldId(k) = g_cId(k)
        oldOrg(k) = g_cOrg(k)
    NEXT k
    oldSpace = g_space
    g_space = LEFT$(oldSpace, pos - 1) + ins + MID$(oldSpace, pos, LEN(oldSpace) - pos + 1)
    m = 1
    FOR k = 1 TO LEN(g_space)
        IF k >= pos AND k <= pos + add - 1 THEN
            g_nextCellId = g_nextCellId + 1
            g_cId(k) = g_nextCellId
            g_cOrg(k) = g_nEvents
        ELSE
            g_cId(k) = oldId(m)
            g_cOrg(k) = oldOrg(m)
            m = m + 1
        END IF
    NEXT k
END SUB

' Cancella `nDel` celle dalla posizione pos.
SUB SpaceDelete(pos AS INTEGER, nDel AS INTEGER)
    DIM k AS INTEGER, m AS INTEGER
    DIM oldSpace AS STRING
    DIM oldId(RF_MAX_CELLS) AS LONG
    DIM oldOrg(RF_MAX_CELLS) AS LONG
    IF nDel <= 0 THEN EXIT SUB
    FOR k = 1 TO LEN(g_space)
        oldId(k) = g_cId(k)
        oldOrg(k) = g_cOrg(k)
    NEXT k
    oldSpace = g_space
    g_space = LEFT$(oldSpace, pos - 1) + MID$(oldSpace, pos + nDel, LEN(oldSpace) - pos - nDel + 1)
    m = 1
    FOR k = 1 TO LEN(g_space)
        IF m = pos THEN m = pos + nDel
        g_cId(k) = oldId(m)
        g_cOrg(k) = oldOrg(m)
        m = m + 1
    NEXT k
END SUB

' Sovrascrive le celle [pos .. pos+len-1] con `txt`: sono DISTRUTTE e ricreate
' (id nuovo, origine = evento in corso).
SUB SpaceOverwrite(pos AS INTEGER, txt AS STRING)
    DIM k AS INTEGER, idx AS INTEGER, ch AS STRING
    idx = pos
    FOR k = 1 TO LEN(txt)
        IF idx <= LEN(g_space) THEN
            ch = MID$(txt, k, 1)
            g_space = LEFT$(g_space, idx - 1) + ch + MID$(g_space, idx + 1, LEN(g_space) - idx)
            g_nextCellId = g_nextCellId + 1
            g_cId(idx) = g_nextCellId
            g_cOrg(idx) = g_nEvents
        END IF
        idx = idx + 1
    NEXT k
END SUB

' ============================================================================
' Motore: 1 applicazione di regola = 1 evento con il suo DELTA
' ============================================================================
FUNCTION ApplyRuleAt(r AS INTEGER, pos AS INTEGER) AS INTEGER
    DIM pat AS STRING, dst AS STRING, op AS INTEGER
    DIM k AS INTEGER
    pat = g_rPat(r)
    dst = g_rDst(r)
    op = g_rOp(r)

    ' Origini delle celle toccate (prima della mutazione) = padri causali.
    g_nHit = 0
    FOR k = pos TO pos + LEN(pat) - 1
        IF k <= LEN(g_space) AND g_nHit < RF_MAX_HIT THEN
            g_nHit = g_nHit + 1
            g_hitOrg(g_nHit) = g_cOrg(k)
        END IF
    NEXT k
    IF g_nHit = 0 THEN
        ApplyRuleAt = 0
        EXIT FUNCTION
    END IF

    g_nEvents = g_nEvents + 1
    IF g_nEvents > RF_MAX_EVENTS THEN Fail("troppi eventi (max " + RTrimNum(RF_MAX_EVENTS) + ")")
    g_evTick(g_nEvents) = g_tick
    g_evRule(g_nEvents) = r
    g_evPos(g_nEvents) = pos
    AddCausalEdges(g_nEvents)

    IF op = 1 THEN
        SpaceDelete(pos, LEN(pat))
        SpaceInsert(pos, dst)
    ELSEIF op = 2 THEN
        SpaceOverwrite(pos, dst)
    ELSEIF op = 3 THEN
        SpaceInsert(pos, dst)
    ELSE
        SpaceDelete(pos, LEN(pat))
    END IF
    g_applied = g_applied + 1
    ApplyRuleAt = 1
END FUNCTION

' Archi causali: tutte le ORIGINI delle celle toccate -> evento corrente.
' (origine 0 = "generazione zero": è il nodo radice del grafo)
SUB AddCausalEdges(toEvent AS INTEGER)
    DIM k AS INTEGER, b AS INTEGER, a AS INTEGER, dup AS INTEGER
    FOR k = 1 TO g_nHit
        a = g_hitOrg(k)
        IF a <> toEvent THEN
            dup = 0
            FOR b = 1 TO g_nEdges
                IF g_eFrom(b) = a AND g_eTo(b) = toEvent THEN dup = 1
            NEXT b
            IF dup = 0 THEN
                IF g_nEdges >= RF_MAX_EDGES THEN Fail("troppi archi causali (max " + RTrimNum(RF_MAX_EDGES) + ")")
                g_nEdges = g_nEdges + 1
                g_eFrom(g_nEdges) = a
                g_eTo(g_nEdges) = toEvent
            END IF
        END IF
    NEXT k
END SUB

' Un tick: ogni regola si applica UNA volta, al primo match da sinistra.
' (È la semantica M1 documentata; il parallelismo/multiway di FlowLang è in M2.)
SUB RunEvolution()
    DIM t AS INTEGER, r AS INTEGER, pos AS INTEGER, progressed AS INTEGER
    g_tick = 0
    g_nEvents = 0
    g_nEdges = 0
    g_applied = 0
    LoadFlow()
    FOR t = 1 TO g_steps
        g_tick = t
        progressed = 0
        FOR r = 1 TO g_nRules
            pos = INSTR(g_space, g_rPat(r))
            IF pos > 0 THEN
                IF ApplyRuleAt(r, pos) = 1 THEN progressed = 1
            END IF
        NEXT r
        IF progressed = 0 THEN
            IF g_verbose = "TRUE" AND g_steps > 0 THEN
                PRINT "tick "; t; ": nessuna regola applicabile (evoluzione conclusa)"
            END IF
            EXIT FOR
        END IF
    NEXT t
END SUB

' ============================================================================
' MODE: RUN — traccia degli eventi + spazio finale
' ============================================================================
SUB RunFlow()
    DIM i AS INTEGER
    DIM initSpace AS STRING
    initSpace = "?"
    RunEvolution()
    ' Lo spazio iniziale è quello del flow PRIMA di evolvere: si rilegge dal file
    ' (RunEvolution lo ha già sovrascritto con lo stato finale).
    initSpace = InitialSpaceOfFlow()
    PRINT "flow         : "; FlowPath()
    PRINT "init         : "; initSpace
    PRINT "steps        : "; g_steps
    PRINT "regole       : "; g_nRules
    PrintRules()
    PRINT "---"
    PRINT "eventi       : "; g_nEvents; "  applicazioni: "; g_applied; "  archi causali: "; g_nEdges
    PRINT "celle finali : "; LEN(g_space)
    PRINT "spazio finale: "; g_space
    PRINT "---"
    FOR i = 1 TO g_nEvents
        PRINT "ev "; Pad4(i); "  tick "; Pad3(g_evTick(i)); "  regola "; Pad3(g_evRule(i)); "  pos "; Pad4(g_evPos(i))
    NEXT i
END SUB

SUB PrintRules()
    DIM i AS INTEGER, opName AS STRING
    FOR i = 1 TO g_nRules
        IF g_rOp(i) = 1 THEN
            opName = "->"
        ELSEIF g_rOp(i) = 2 THEN
            opName = "-->"
        ELSEIF g_rOp(i) = 3 THEN
            opName = ">"
        ELSE
            opName = "><"
        END IF
        PRINT "  r"; i; ": "; g_rPat(i); " "; opName; " "; g_rDst(i)
    NEXT i
END SUB

' Spazio iniziale dichiarato da @init nel flow ("" se assente).
FUNCTION InitialSpaceOfFlow() AS STRING
    DIM lines
    DIM raw AS STRING, up AS STRING
    DIM i AS LONG
    InitialSpaceOfFlow = ""
    lines = IO.READLINES(FlowPath())
    FOR i = 0 TO UBOUND(lines)
        raw = STRINGS.TRIM(lines(i))
        up = STRINGS.UPPER(raw)
        IF STRINGS.STARTSWITH(up, "@INIT") THEN
            InitialSpaceOfFlow = Unquote(STRINGS.TRIM(ParenInner(raw)))
        END IF
    NEXT i
END FUNCTION

' ============================================================================
' MODE: GRAPH — metriche del grafo causale (come EventCausalityGraph upstream)
' ============================================================================
SUB RunGraph()
    DIM nodes AS INTEGER, edges AS INTEGER
    DIM longest AS INTEGER
    DIM density AS SINGLE
    RunEvolution()
    ComputeMetrics()
    nodes = g_nodes
    edges = g_nEdges
    longest = g_longest
    density = g_density
    PRINT "flow         : "; FlowPath()
    PRINT "eventi       : "; g_nEvents
    PRINT "nodi         : "; nodes; "  (eventi + nodo radice generazione 0)"
    PRINT "archi        : "; edges
    PRINT "densità      : "; Fmt4(density)
    PRINT "cammino max  : "; longest
    PRINT "spazio finale: "; g_space
END SUB

' Metriche del DAG: nodi, archi, densità, cammino massimo.
' Gli eventi hanno archi SOLO da eventi precedenti (le celle distrutte sono state
' create da eventi già avvenuti) → l'ordine crescente degli id è topologico.
SUB ComputeMetrics()
    DIM e AS INTEGER, k AS INTEGER, best AS INTEGER
    DIM lens(RF_MAX_EVENTS) AS INTEGER
    g_nodes = g_nEvents + 1
    g_longest = 0
    FOR e = 1 TO g_nEvents
        best = 0
        FOR k = 1 TO g_nEdges
            IF g_eTo(k) = e THEN
                IF lens(g_eFrom(k)) + 1 > best THEN best = lens(g_eFrom(k)) + 1
            END IF
        NEXT k
        lens(e) = best
        IF best > g_longest THEN g_longest = best
    NEXT e
    IF g_nodes > 1 THEN
        g_density = g_nEdges / (g_nodes * (g_nodes - 1))
    ELSE
        g_density = 0
    END IF
END SUB

' Formattazione a 4 decimali senza printf (parità 4 gambe).
FUNCTION Fmt4(x AS SINGLE) AS STRING
    DIM v AS LONG, s AS STRING, neg AS INTEGER
    v = CLNG(x * 10000 + 0.5)
    neg = 0
    IF v < 0 THEN
        neg = 1
        v = -v
    END IF
    s = LTRIM$(STR$(v))
    DO WHILE LEN(s) < 5
        s = "0" + s
    LOOP
    s = LEFT$(s, LEN(s) - 4) + "." + MID$(s, LEN(s) - 3, 4)
    IF neg = 1 THEN s = "-" + s
    Fmt4 = s
END FUNCTION

FUNCTION Pad3(n AS INTEGER) AS STRING
    DIM s AS STRING
    s = LTRIM$(STR$(n))
    DO WHILE LEN(s) < 3
        s = " " + s
    LOOP
    Pad3 = s
END FUNCTION

FUNCTION Pad4(n AS INTEGER) AS STRING
    DIM s AS STRING
    s = LTRIM$(STR$(n))
    DO WHILE LEN(s) < 4
        s = " " + s
    LOOP
    Pad4 = s
END FUNCTION

' ============================================================================
' MODE: CHECK — diagnostica completa (config, regole, evoluzione, DAG)
' ============================================================================
SUB RunCheck()
    DIM ok AS INTEGER, i AS INTEGER
    DIM maxFrom AS INTEGER, maxTo AS INTEGER
    ok = 1
    IF NOT IO.FILEEXISTS(FlowPath()) THEN
        PRINT "check-flow  : FAIL (flow assente: "; FlowPath(); ")"
        ok = 0
    ELSE
        RunEvolution()
        IF g_nRules = 0 THEN
            PRINT "check-rules : FAIL (nessuna regola nel flow)"
            ok = 0
        ELSE
            PRINT "check-rules : OK ("; g_nRules; " regole, "; g_nEvents; " eventi)"
        END IF
    END IF
    IF ok = 1 THEN
        IF g_nEvents = 0 THEN
            PRINT "check-evolve: FAIL (nessun evento prodotto)"
            ok = 0
        ELSE
            PRINT "check-evolve: OK ("; g_applied; " applicazioni, spazio "; LEN(g_space); " celle)"
        END IF
    END IF
    IF ok = 1 THEN
        maxFrom = 0
        maxTo = 0
        FOR i = 1 TO g_nEdges
            IF g_eFrom(i) > g_eTo(i) THEN ok = 0
            IF g_eTo(i) > g_nEvents THEN ok = 0
            IF g_eFrom(i) > maxFrom THEN maxFrom = g_eFrom(i)
            IF g_eTo(i) > maxTo THEN maxTo = g_eTo(i)
        NEXT i
        IF ok = 0 THEN
            PRINT "check-dag   : FAIL (arco con sorgente >= destinazione o evento inesistente)"
        ELSE
            PRINT "check-dag   : OK (da 0.."; maxFrom; " a 1.."; maxTo; ", "; g_nEdges; " archi)"
        END IF
    END IF
    IF ok = 1 THEN
        ComputeMetrics()
        IF g_longest < 0 OR g_longest > g_nEvents THEN
            PRINT "check-metric: FAIL (cammino massimo "; g_longest; " fuori da [0, "; g_nEvents; "])"
            ok = 0
        ELSE
            PRINT "check-metric: OK (nodi "; g_nodes; ", densità "; Fmt4(g_density); ", cammino "; g_longest; ")"
        END IF
    END IF
    IF LEN(g_space) = 0 THEN
        PRINT "check-space : FAIL (spazio vuoto)"
        ok = 0
    ELSE
        PRINT "check-space : OK ("; LEN(g_space); " celle)"
    END IF
    IF ok = 1 THEN
        PRINT "RULEFLOW-CHECK-OK"
    ELSE
        PRINT "RULEFLOW-CHECK-FAIL"
        ERROR 5
    END IF
END SUB
