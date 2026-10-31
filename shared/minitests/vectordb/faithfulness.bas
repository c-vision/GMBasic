' RAG eval R2 — VECTORDB.FAITHFULNESS(contesto$, risposta$): quota token risposta radicati nel contesto.
' Token ASCII lowercase (identico C#/C). Risposta vuota → 0 (verdetto). Case-insensitive.
' EXPECT: mixed=  0.75|all=  1|upper=  1|empty= empty-answer
IMPORT VECTORDB

SUB Main()
    DIM ctx$ = "il motore mlx gestisce la memoria"
    PRINT "mixed= " + STR(MATH.ROUND(VECTORDB.FAITHFULNESS(ctx$, "motore mlx memoria rapido"), 4))
    PRINT "all= " + STR(MATH.ROUND(VECTORDB.FAITHFULNESS(ctx$, "il motore mlx"), 4))
    PRINT "upper= " + STR(MATH.ROUND(VECTORDB.FAITHFULNESS(ctx$, "MOTORE MLX MEMORIA"), 4))
    IF VECTORDB.FAITHFULNESS(ctx$, "") = 0 THEN
        PRINT "empty= empty-answer"
    ELSE
        PRINT "empty= ERR"
    END IF
END SUB
