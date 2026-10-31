' AI SET ACTIVE — forma capability (%CHAT) e forma stringa ("completion") → AI.ACTIVE riflette
' il provider attivo. Deterministico (provider fixture di default); nessun modello/rete.
' Copre la divergenza emitter C# (il ramo TokenKind scartava Args[1] e hardcodava "completion")
' e la normalizzazione del ruolo nel runtime C (%CHAT → "completion").
' EXPECT: fixture-completion|fixture-completion
IMPORT AI

SUB Main()
    AI SET ACTIVE %CHAT, "fixture-completion"
    PRINT AI.ACTIVE(%CHAT)
    AI SET ACTIVE "completion", "fixture-completion"
    PRINT AI.ACTIVE(%CHAT)
END SUB
