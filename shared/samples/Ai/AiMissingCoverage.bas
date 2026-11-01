' FILE: samples/Ai/AiMissingCoverage.bas
' Copertura per 2 simboli AI senza sample reale (ASKMulti, MemoryAsk) e
' documentazione delle esclusioni per BlackboardWait (bug IL), Discover/SetCredits ([impura]).
' EXPECT: ask= [insufficient evidence] No relevant documents found.|mem=
IMPORT AI

SUB Main()
    PRINT "ask="; AI.ASKMULTI("x", 1, 1, 1)
    PRINT "mem="; AI.MEMORYASK("x", 1)
END SUB