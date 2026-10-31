' EXPECT: 8|8
' MiniTest — VectorDB EMBED dimensioni reali su IL (oracolo per la parità).
' AI.EMBED(testo, 8) → array di 8 elementi: UBOUND(v) = 7 per array 0-based.
' Provider deterministico: fixture ESPLICITO (vedi indextext_outcomes.bas).
' Nessun db: embedding puro, deterministico.
IMPORT AI

SUB Main()
    AI SET ACTIVE %EMBEDDING, "fixture-embedding"
    DIM v = AI.EMBED("testo di prova", 8)
    PRINT UBOUND(v) + 1
    DIM w = AI.EMBED("un altro testo", 8)
    PRINT UBOUND(w) + 1
END SUB
