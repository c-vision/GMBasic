' samples/Ai/SemanticGraphDemo.bas
' ─────────────────────────────────────────────────────────────────────────────
' GMB SEMANTIC GRAPH (SEMAPRAX) — superficie BASIC completa (namespace `AI`).
'
' Il programma analizza SE STESSO (impact report + semantic-patch plan) su un modulo a strati
' (entry -> orchestratore -> servizi -> utility), usando TUTTE le funzioni della superficie:
'   LLM.SYMBOL(nome$)          definizione (kind + nome)
'   LLM.CALLMAP(nome$)         chi la chiama / chi chiama lei
'   LLM.IMPACT(nome$[,depth])  impatto transitivo all'indietro
'   LLM.CONTEXT(nome$[,depth]) contesto limitato (simbolo + vicini)
'   LLM.REVISION()             hash di revisione del grafo (revision-bound)
'   LLM.CHECKREV(rev$)         gate: 'match'/'mismatch' (expected==current)
'   LLM.PATCH(nome$)           receipt del semantic patch (target + impact + decision)
'   LLM.INSPECT(nome$)         hover: kind+label + callmap immediato
'
' NOTA (Opzione A): gli argomenti "nome"/"rev" devono essere LETTERALI noti a compile-time
' (il grafo è risolto al link-time). Le query sono COSTANTI a runtime, deterministiche.
' Disponibili su tutti e 5 i backend (IL, PCodeVM, VM/C, C99Native) con parità.
' ─────────────────────────────────────────────────────────────────────────────
IMPORT LLM

' ---- strato utility ----
FUNCTION BoundVal(v AS DOUBLE, lo AS DOUBLE, hi AS DOUBLE) AS DOUBLE
    IF v < lo THEN
        BoundVal = lo
    ELSE
        IF v > hi THEN
            BoundVal = hi
        ELSE
            BoundVal = v
        END IF
    END IF
END FUNCTION

FUNCTION UnitScale(v AS DOUBLE) AS DOUBLE
    UnitScale = BoundVal(v, 0.0, 1.0)
END FUNCTION

' ---- strato servizi ----
FUNCTION PointScore(x AS DOUBLE) AS DOUBLE
    PointScore = UnitScale(x) * 100.0
END FUNCTION

SUB EmitReport(x AS DOUBLE)
    PRINT "  score = "; PointScore(x)
END SUB

SUB AuditPass(x AS DOUBLE)
    EmitReport(x)
END SUB

' ---- strato orchestratore ----
SUB ExecPipeline(x AS DOUBLE)
    AuditPass(x)
END SUB

' ---- entry ----
SUB Main()
    PRINT "=== GMB Semantic Graph (SEMAPRAX) — impact report + patch plan ==="
    PRINT

    ' 1) Definizioni (kind + nome) — e una non esistente (stringa vuota).
    PRINT "SYMBOL   BoundVal     -> "; LLM.SYMBOL("BoundVal")
    PRINT "SYMBOL   PointScore   -> "; LLM.SYMBOL("PointScore")
    PRINT "SYMBOL   ExecPipeline -> "; LLM.SYMBOL("ExecPipeline")
    PRINT "SYMBOL   NonEsiste    -> "; LLM.SYMBOL("NonEsiste")

    PRINT
    ' 2) Mappa delle chiamate.
    PRINT "CALLMAP  UnitScale    -> "; LLM.CALLMAP("UnitScale")
    PRINT "CALLMAP  ExecPipeline -> "; LLM.CALLMAP("ExecPipeline")

    PRINT
    ' 3) Impatto (transitivo all'indietro) e con limite di profondità.
    PRINT "IMPACT   BoundVal     -> "; LLM.IMPACT("BoundVal")
    PRINT "IMPACT   BoundVal(2)  -> "; LLM.IMPACT("BoundVal", 2)

    PRINT
    ' 4) Contesto limitato (simbolo + vicini) a 1 e 2 livelli.
    PRINT "CONTEXT  UnitScale(1) -> "; LLM.CONTEXT("UnitScale", 1)
    PRINT "CONTEXT  UnitScale(2) -> "; LLM.CONTEXT("UnitScale", 2)

    PRINT
    ' 5) Revision hash + gate revision-bound (SEMAPRAX).
    PRINT "REVISION              -> "; LLM.REVISION()
    PRINT "CHECKREV(match)       -> "; LLM.CHECKREV("rev=eeb9e87611bf1779")
    PRINT "CHECKREV(zero)        -> "; LLM.CHECKREV("rev=eeb9e87611bf1779")

    PRINT
    ' 6) Semantic patch plan (receipt): target + impatto + decisione.
    PRINT "PATCH    UnitScale    -> "; LLM.PATCH("UnitScale")
    ' 6b) Hover: kind+label + callmap immediato (LLM.INSPECT).
    PRINT "INSPECT  UnitScale    -> "; LLM.INSPECT("UnitScale")

    PRINT
    ' 7) PARSEC (repo/symbol/call map): ricerca simboli + mappa delle chiamate.
    PRINT "FINDSYMBOL Unit    -> "; LLM.FINDSYMBOL("Unit")
    PRINT "FINDSYMBOL NonX    -> "; LLM.FINDSYMBOL("NonX")
    PRINT "REPOMAP            -> "; LLM.REPOMAP()

    PRINT
    ' 8) Esecuzione reale del pipeline analizzato (il programma è anche funzionante).
    ExecPipeline(1.7)

    PRINT
    PRINT "=== fine impact report ==="
END SUB
