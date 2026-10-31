' RAG eval R2 — VECTORDB.COMPLETENESS(domanda$, risposta$): come RELEVANCE ma solo token di
' contenuto (>= 3 char, non stopwords); se non ci sono token di contenuto ricade su RELEVANCE.
' Handbook: "che cosa gestisce il motore" → contenuto = che, cosa, gestisce, motore (il=2 char escluso).
' EXPECT: partial=  0.5|full=  0.75|fallback=  0.5
IMPORT VECTORDB

SUB Main()
    PRINT "partial= " + STR(MATH.ROUND(VECTORDB.COMPLETENESS("che cosa gestisce il motore", "il motore gestisce la memoria"), 4))
    PRINT "full= " + STR(MATH.ROUND(VECTORDB.COMPLETENESS("che cosa gestisce il motore", "il motore gestisce la cosa"), 4))
    ' domanda senza token di contenuto ("di" e "il" < 3 char) → fallback RELEVANCE: 1 hit / 2 token
    PRINT "fallback= " + STR(MATH.ROUND(VECTORDB.COMPLETENESS("di il", "il motore"), 4))
END SUB
