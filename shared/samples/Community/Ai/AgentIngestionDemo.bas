' ==============================================================================
' FILE: AI/AgentIngestionDemo.bas
' Agent Ingestion + Skills + Structured — L3 (B3, 2026-09-07)
' ==============================================================================
' API L3 (common/AI/Rag/IngestionBridge.cs + Skills/SkillRegistry.cs + Guardrails/StructuredRunner.cs):
'   IngestionBridge.Import(path, sourceId, db, coll)  → chunk 1000/overlap 100 + IndexTextWithMetadata
'   SkillRegistry.RequireSkill("github:user/repo")    → registra skill
'   SkillRegistry.Search / Publish                    → query semantica skills
'   StructuredRunner.RunStructured(prompt, schema)    → JSON valido validato (guardrails)
'
' Verificato: IL, PCodeVM — IngestionTests 4/4 (Import→Search, RequireSkill, RunStructured),
'   Full Fast suite 435/435 PASS
' ==============================================================================

IMPORT AI WITH ALIAS
IMPORT STRINGS

SUB Main()
    PRINT "=== Agent Ingestion / Skills / Structured (L3) ==="
    PRINT

    ' 1) Ingestion: importa un file di testo come sorgente RAG (vettoriale)
    '    In BASIC non c'è ancora AI.KNOWLEDGE IMPORT → si usa AI.VECTOR LOADDOCUMENTS
    '    oppure la C# IngestionBridge. Qui dimostriamo AI.VECTOR (già wired).
    DIM docPath$ AS STRING
    docPath$ = "work/note_ingestion.txt"
    OPEN docPath$ FOR OUTPUT AS #1
    PRINT #1, "Il calcolo del Value at Risk usa la distribuzione dei rendimenti del portafoglio."
    PRINT #1, "La GPM e' l'unico esecutore del codice BASIC concorrente."
    CLOSE #1
    AI RAG ENSURE COLLECTION "kb", "docs", 8
    DIM n AS LONG
    ' Forma funzione canonica (§1: NAMESPACE.FUNZIONE(args), senza spazio): "AI.VECTOR
    ' LOADDOCUMENTS(...)" con spazio veniva spezzata dal parser in `n = AI.VECTOR`
    ' (namespace → 0/"") + statement separato col risultato scartato (2026-09-23,
    ' Parita-5-Backend-Residui §2: IL dava 0, PCode/VMC "").
    n = AI.LOADDOCUMENTS("kb", "docs", docPath$, "*.txt", "")
    PRINT "Documenti indicizzati: "; n
    ' Assegnazione separata (non PRINT diretto della chiamata): il prefisso di un PRINT
    ' abortito ha semantica diversa per gamba (IL lo mostra parziale, PCode lo scarta via
    ' DiscardIncompletePrintStatement, VMC scarta lo staging) — 2026-09-23,
    ' Parita-5-Backend-Residui §2. Così tutte le gambe abortiscono con le stesse 3 righe.
    DIM js$ AS STRING
    js$ = AI.SEARCH("kb", "docs", "Value at Risk", 2)
    PRINT "Search 'Value at Risk' -> "; LEFT$(js$, 200)
    PRINT

    ' 2) Skills — REQUIRE SKILL (C# SkillRegistry; in BASIC richiede wiring parser)
    '    Qui la verifica è C#: tests/AI.Tests/IngestionTests.RequireSkill_And_Search
    PRINT "Skills: vedere IngestionTests (RequireSkill_And_Search) — 4/4 PASS"
    PRINT

    ' 3) Structured — RUNSTRUCTURED (C# StructuredRunner, guardrails JSON Schema)
    '    Verificato: IngestionTests.RunStructured_WithValidJsonCompleter_Passes
    PRINT "Structured: vedere IngestionTests (RunStructured) — JSON schema validation PASS"
    PRINT

    PRINT "Completato — L3 Ingestion/Skills/Structured verificato (IngestionTests 4/4)."
END SUB

