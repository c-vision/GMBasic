' FILE: Ai/AiAskChainExample.bas
' =================================================================================================
'   AI ASKCHAIN <name>, <prompt>, <prefix> [INTO <var$>]   (comando, 2026-09-09, T1.2)
'   Ragionamento concatenato (Blackboard-as-Artifacts): ogni anello legge <prefix>_state dal
'   Blackboard, compone il prompt col RIASSUNTO dello stato precedente, genera SENZA transcript
'   (reset di contesto implicito, risparmio token) e appende l'artefatto (<prefix>_s1..sN).
'   Chiamate consecutive con lo stesso <prefix> = passi successivi della stessa catena.
'   Host C# (AgentServer.AskChainedStep): richiede un provider LLM configurato sull'agente;
'   VM/C/C99/C99Native N/A documentati.
' =================================================================================================
IMPORT AI WITH ALIAS

SUB Main()
    AI AGENT CREATE "Ragionatore"

    DIM esito AS STRING

    ' 1° anello: analisi parziale -> artefatto su blackboard "catena_s1"
    AI ASKCHAIN "Ragionatore", "Analizza il problema in 3 punti", "catena" INTO esito
    PRINT "anello1:"; esito

    ' 2° anello: sintesi che RIUSA lo stato accumulato (riassunto dal blackboard)
    AI ASKCHAIN "Ragionatore", "Sintetizza una soluzione", "catena" INTO esito
    PRINT "anello2:"; esito

    AI AGENT DELETE "Ragionatore"
    PRINT "askchain-done"
END SUB
