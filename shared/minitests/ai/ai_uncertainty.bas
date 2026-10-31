' EXPECT: 1|1|1|1|1|1|1|1
' MiniTest — Self-Consistency / Semantic Entropy: AI.LEXICALENTROPY(responses$) e
' AI.CONSISTENCYDETAIL(). Deterministico (nessun provider): entropia di Shannon (bit)
' sulle risposte separate da ';', con normalizzazione (filler prefix, numeri, case).
' Le asserzioni sono booleane per non dipendere dalla formattazione dei double.
IMPORT AI

SUB Main()
    ' unanime → entropia 0
    PRINT IIF(AI.LEXICALENTROPY("a;a;a") = 0, 1, 0)
    ' tre risposte distinte → log2(3) ≈ 1.58496
    DIM h AS DOUBLE = AI.LEXICALENTROPY("a;b;c")
    PRINT IIF(h > 1.58 AND h < 1.59, 1, 0)
    ' normalizzazione: "The answer is 42" e "answer: 42" sono lo stesso cluster
    PRINT IIF(AI.LEXICALENTROPY("The answer is 42;answer: 42") = 0, 1, 0)
    ' case + spazi + separatore newline
    PRINT IIF(AI.LEXICALENTROPY("YES ;  yes") = 0, 1, 0)
    ' dettaglio JSON dell'ultimo calcolo
    DIM d AS STRING = AI.CONSISTENCYDETAIL()
    PRINT IIF(INSTR(d, """n_buckets""") > 0, 1, 0)
    PRINT IIF(INSTR(d, """entropy""") > 0, 1, 0)
    PRINT IIF(INSTR(d, """collapse_warning""") > 0, 1, 0)
    ' due cluster → risk = H/log2(2) = 1.0
    DIM h2 AS DOUBLE = AI.LEXICALENTROPY("x;y")
    PRINT IIF(h2 > 0.99 AND h2 < 1.01, 1, 0)
END SUB
