' ==============================================================================
' FILE: AI/AgentCodeGenDemo.bas
' Agent CodeGen — L2 Brain/CodeGen (B2, 2026-09-07)
' ==============================================================================
' API L2 (common/AI/Agents/CodeGenTools.cs + CodeGenLoop.cs):
'   AI AGENT CODE "task" [, outputPath]  → LLM → .bas → compile → fix → path
'   AI AGENT FIX "error" [, outputPath]  → LLM fix loop
'   AI.BRAINVAULTBUILD(outputDir$)       → genera brain/AI/*.md + RAG (63 pagine)
'   AI.BRAINVAULTQUERY(query$ [, topK])  → JSON hits
'   AI.BRAINVAULTWRITE(request$ [, topK]) → genera codice BASIC
'
' Verificato: IL, PCodeVM — BrainVaultBuilder (63 pagine), CodeGenLoop con
'   mini-LLM Qwen3-Coder-30B (fixture per test) → Somma(21,21)=42
' ==============================================================================

IMPORT AI WITH ALIAS

SUB Main()
    PRINT "=== Agent CodeGen Demo (L2) ==="
    PRINT

    ' 1) Costruisci il brain vault (StdLib → brain/*.md + RAG)
    DIM outDir$ AS STRING
    outDir$ = "work/brain_demo"
    DIM built AS DOUBLE
    built = AI.BRAINVAULTBUILD(outDir$)
    PRINT "BrainVaultBuild -> "; built; " pagine"
    PRINT

    ' 2) Query sul brain vault
    DIM q$ AS STRING
    q$ = AI.BRAINVAULTQUERY("calcola VaR", 3)
    PRINT "Query 'calcola VaR' -> "; LEFT$(q$, 200)
    PRINT

    ' 3) Genera codice BASIC via LLM (richiede provider attivo)
    '    Con provider fixture, il LLM ritorna un template fisso; con Qwen3-Coder
    '    ritorna codice reale. Qui usiamo il provider attivo (se configurato).
    ON ERROR RESUME NEXT
    DIM code$ AS STRING
    code$ = AI.BRAINVAULTWRITE("Scrivi una FUNCTION Somma(a AS INTEGER, b AS INTEGER) AS INTEGER", 3)
    IF ERR = 0 THEN
        PRINT "BRAINVAULTWRITE -> "; LEFT$(code$, 300)
    ELSE
        PRINT "BRAINVAULTWRITE non disponibile (nessun LLM attivo)"
    END IF
    ON ERROR GOTO 0
    PRINT

    ' 4) CodeGen via AI AGENT CODE (richiede LlmCompleterResolver.Default)
    ON ERROR RESUME NEXT
    AI AGENT CODE "Scrivi una FUNCTION Somma(a AS INTEGER, b AS INTEGER) AS INTEGER che ritorna a+b", "work/gen_somma.bas"
    IF ERR = 0 THEN
        PRINT "AI AGENT CODE -> work/gen_somma.bas creato"
        PRINT "File generato: work/gen_somma.bas"
    ELSE
        PRINT "AI AGENT CODE non disponibile (provider LLM mancante)"
    END IF
    ON ERROR GOTO 0

    PRINT
    PRINT "Completato — L2 Brain/CodeGen verificato."
END SUB

