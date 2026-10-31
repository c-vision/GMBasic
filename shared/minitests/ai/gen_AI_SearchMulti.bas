' EXPECT: ERROR: SearchMulti: explain non supportato sul merge multi-brain (usare AIVECTOR.SEARCH single-collection).
IMPORT AI

SUB Main()
    PRINT AI.SEARCHMULTI("x", 1, "x", "x", 1, 1, 1)
END SUB
