' P2 residuo (asse B) — la POLICY del Context Engine è EFFETTIVA, non solo memorizzata.
' Prima: budget/policy esistevano ma nessuno li applicava (la view restava il testo grezzo).
' Ora SET VIEW normalizza subito secondo budget+policy, quindi CONTEXTUSED/OVERFLOW/VIEW$ e il
' contesto usato dal tool loop riflettono la vista REALE.
' EXPECT: trunc= ok|tused=  10|tovf=  6|tview= 1234567890|comp= ok|cused=  40|covf=  68|cmark= si|eused=  0|eview= vuota|sel= ok|sbud=  10
IMPORT AI

SUB Main()
    DIM a AS STRING

    ' ── policy truncate: taglio netto ai primi `budget` char ──────────────────
    a = "t_trunc"
    AI CONTEXT SET BUDGET a, 10
    AI CONTEXT SET POLICY a, "truncate"
    AI CONTEXT VIEW a, "1234567890123456"
    PRINT "trunc= ok"
    PRINT "tused= "; AI.CONTEXTUSED(a)
    PRINT "tovf= "; AI.CONTEXTOVERFLOW(a)
    PRINT "tview= "; AI.CONTEXTVIEW$(a)

    ' ── policy compact (default): testa + marcatore + coda, lunghezza = budget ─
    a = "t_compact"
    AI CONTEXT SET BUDGET a, 40
    AI CONTEXT VIEW a, "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJ"
    PRINT "comp= ok"
    PRINT "cused= "; AI.CONTEXTUSED(a)
    PRINT "covf= "; AI.CONTEXTOVERFLOW(a)
    IF INSTR(AI.CONTEXTVIEW$(a), "compattato") > 0 THEN PRINT "cmark= si" ELSE PRINT "cmark= no"

    ' ── policy error: la vista viene RIFIUTATA (mai un taglio silenzioso) ─────
    ' (con policy error non esiste una funzione-comando "ritorna valore": l'effetto
    '  osservabile è che la vista NON viene aggiornata → resta vuota)
    a = "t_error"
    AI CONTEXT SET BUDGET a, 10
    AI CONTEXT SET POLICY a, "error"
    AI CONTEXT VIEW a, "1234567890123456"
    PRINT "eused= "; AI.CONTEXTUSED(a)
    IF AI.CONTEXTVIEW$(a) = "" THEN PRINT "eview= vuota" ELSE PRINT "eview= "; AI.CONTEXTVIEW$(a)

    ' ── AI CONTEXT SET SELECTOR: completa la superficie dichiarata (prima: "P3-P4") ─
    a = "t_sel"
    AI CONTEXT SET SELECTOR a, "task,deps,freshness"
    AI CONTEXT SET BUDGET a, 10
    PRINT "sel= ok"
    PRINT "sbud= "; AI.CONTEXTBUDGET(a)
END SUB
