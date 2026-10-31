' ==============================================================================
' FILE: AI/McpCapabilityServer.bas
' McpCapabilityServer — capability server MCP GMBasic (pattern MCP-001 a 3 layer)
' ==============================================================================
' Portabile del pattern architetturale di mcp-001-fintech-data-server (NON del
' codice Python): DATA LAYER (funzioni fonte), ACCESS LAYER (repository con
' schemas), MCP LAYER (registrazione tool + serve). Il "tool che consulta più
' fonti e torna un profilo consolidato" è il portabile di get_customer_profile.
'
' Capability esposte (dominio GMBasic, NON fintech):
'   gmb_stats    — statistiche di un campione numerico (fonte 1: STATS)
'   gmb_savings  — piano di risparmio con interessi composti (fonte 2: aritmetica
'                  esatta intera — il capitale è in centesimi, così tutte le gambe
'                  concordano cifra per cifra)
'   gmb_report   — report consolidato multi-fonte (pattern get_customer_profile)
'
' Tool classificati READONLY (4° arg di AI MCP REGISTER TOOL): consultazione
' pura, nessuna mutazione di stato — il server in modalità read-only
' (AI MCP SERVE ..., TRUE) li accetta sempre; un tool mutante verrebbe rifiutato.
'
' Verificato: IL, PCodeVM (AI MCP SERVER* è solo C#; C99/C99Native registrano
' il nome nel registry C in-memory senza servire).
' ==============================================================================

IMPORT AI WITH ALIAS
IMPORT STATS
IMPORT STRINGS

' --- DATA LAYER: fonti interne (calcolo puro, come MCP-001 genera/consulta dati) ---

FUNCTION SampleData(id AS INTEGER) AS STRING
    ' Fonte 1 — metriche numeriche per id campione (dati sintetici deterministici)
    DIM v1 AS DOUBLE, v2 AS DOUBLE, v3 AS DOUBLE
    v1 = 10 + (id MOD 7)
    v2 = 20 + (id MOD 11)
    v3 = 30 + (id MOD 13)
    SampleData = STR$(v1) + "," + STR$(v2) + "," + STR$(v3)
END FUNCTION

' --- ACCESS LAYER: repository con schemas (input fisso, output testuale) ---

FUNCTION GmbStats(id AS INTEGER) AS STRING
    ' Tool 1 — statistiche del campione (query predefinita: il client sceglie
    ' QUALE tool chiamare, mai come calcolare — MCP-001 key decision)
    DIM parts() AS STRING
    parts = STRINGS.SPLIT(SampleData(id), ",")
    DIM n(3) AS DOUBLE
    n[0] = VAL(parts[0])
    n[1] = VAL(parts[1])
    n[2] = VAL(parts[2])
    GmbStats = "mean=" + STR$(STATS.MEAN(n)) + " max=" + STR$(MATH.MAX(n[0], n[1], n[2])) + " min=" + STR$(MATH.MIN(n[0], n[1], n[2]))
END FUNCTION

FUNCTION GmbSavings(id AS INTEGER, monthlyCents AS LONG, months AS INTEGER) AS STRING
    ' Tool 2 — piano di risparmio (fonte 2): capitale in CENTESIMI e tasso intero
    ' deterministico legato all'id (id MOD 5 %). Aritmetica intera esatta: il valore
    ' è identico su tutti i backend (nessuna divergenza double/decimal).
    DIM ratePct AS INTEGER
    ratePct = id MOD 5
    DIM totalCents AS LONG
    totalCents = 0
    DIM i AS INTEGER
    FOR i = 1 TO months
        totalCents = totalCents + monthlyCents
        totalCents = totalCents + (totalCents * ratePct) \ 1200
    NEXT i
    GmbSavings = "total=" + STR$(totalCents \ 100) + "." + STR$(totalCents MOD 100) + " rate=" + STR$(ratePct) + "%"
END FUNCTION

FUNCTION GmbReport(id AS INTEGER, monthlyCents AS LONG, months AS INTEGER) AS STRING
    ' Tool 3 — report consolidato MULTI-FONTE (pattern get_customer_profile:
    ' consulta fonte statistiche + fonte finanziaria e torna un risultato unitario)
    GmbReport = "id=" + STR$(id) + " " + GmbStats(id) + " " + GmbSavings(id, monthlyCents, months)
END FUNCTION

' --- MCP LAYER: registrazione tool (READONLY esplicito) + vetrina serve ---

SUB Main()
    ' Registrazione con classificazione MCP-001: questi tool consultano/calcolano,
    ' non mutano stato → 4° argomento TRUE.
    AI MCP REGISTER TOOL "gmb_stats", "GmbStats", "Sample statistics by id", TRUE
    AI MCP REGISTER TOOL "gmb_savings", "GmbSavings", "Savings plan (integer cents)", TRUE
    AI MCP REGISTER TOOL "gmb_report", "GmbReport", "Consolidated multi-source report", TRUE

    PRINT "Tools="; AI.MCPLISTTOOLS()

    ' Vetrina locale dei tool (il demo client di MCP-001, in-process):
    ' chiama ogni tool con argomenti di esempio e stampa il profilo consolidato.
    PRINT "stats   = "; GmbStats(42)
    PRINT "savings = "; GmbSavings(42, 50000, 12)
    PRINT "report  = "; GmbReport(42, 50000, 12)

    ' Server read-only: rifiuta qualunque tool NON dichiarato READONLY.
    ' AI MCP SERVE "stdio"             ' blocca su stdin (processo MCP del client)
    ' AI MCP SERVE "http", 8765, TRUE  ' HTTP read-only (richiede GMBASIC_MCP_API_KEY)
END SUB
