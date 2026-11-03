' DecideVerbalizers.bas — T1 del piano JEV: gate FAIL-CLOSED sui 24 verbalizzatori canonici.
'
' Verifica che `false`,`true`,`0`..`5`,`A`..`P` siano UN SOLO token per il tokenizer del modello
' (prima nella forma esatta, poi con spazio iniziale — la stessa regola del provider locale).
' Stampa gli id risolti e un frammento di `scoring_head.json` (`verbalizer_ids`) da incollare
' nell'artefatto di calibrazione. Se una label non è single-token il verdetto è FAIL: quel
' modello NON è decision-capable e il provider locale deve rifiutarlo (nessun fallback `unk`).
'
' Uso: DecideVerbalizers <cartella_modello_o_tokenizer.json>
' PARITY-EXCLUDE: richiede un tokenizer/modello reale (nessun fixture decision-capable).
IMPORT LLM

SUB Main()
    DIM model$ AS STRING
    IF COMMANDCOUNT() >= 1 THEN model$ = COMMAND$(1)
    IF LEN(model$) = 0 THEN
        PRINT "uso: DecideVerbalizers <cartella_modello_o_tokenizer.json>"
        EXIT SUB
    END IF

    DIM labels(23) AS STRING
    labels(0) = "false"
    labels(1) = "true"
    DIM i AS INTEGER
    FOR i = 0 TO 5
        labels(2 + i) = LTRIM$(STR$(i))
    NEXT
    FOR i = 0 TO 15
        labels(8 + i) = CHR$(65 + i)
    NEXT

    DIM ok AS BOOLEAN
    ok = TRUE
    DIM js AS STRING
    js = "{""verbalizer_ids"":{"
    DIM txt$ AS STRING
    DIM ids$ AS STRING
    DIM form$ AS STRING
    DIM n AS INTEGER
    FOR i = 0 TO 23
        txt$ = labels(i)
        n = LLM.TOKENCOUNT(model$, txt$)
        form$ = "exact"
        IF n <> 1 THEN
            txt$ = " " + labels(i)
            n = LLM.TOKENCOUNT(model$, txt$)
            form$ = "space"
        END IF
        ids$ = ""
        IF n = 1 THEN ids$ = LLM.TOKENS(model$, txt$)
        IF i > 0 THEN js = js + ","
        IF n = 1 THEN
            PRINT labels(i); " -> id="; ids$; " ("; form$; ")"
            js = js + CHR$(34) + labels(i) + CHR$(34) + ":" + ids$
        ELSE
            PRINT labels(i); " -> NON single-token (n="; n; ")"
            js = js + CHR$(34) + labels(i) + CHR$(34) + ":null"
            ok = FALSE
        END IF
    NEXT
    js = js + "}}"
    PRINT js
    IF ok THEN
        PRINT "DECIDE-VERBALIZERS PASS (24/24 single-token)"
    ELSE
        PRINT "DECIDE-VERBALIZERS FAIL (modello non decision-capable: vedi sopra)"
    END IF
END SUB
