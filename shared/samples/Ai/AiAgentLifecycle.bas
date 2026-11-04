' FILE: Ai/AiAgentLifecycle.bas
' =================================================================================================
' COMPLESSO: cicli di vita agenti, code, canali, brain — ~15 funzioni AI senza sample.
' Solo operazioni deterministiche locali (nessuna rete esterna).
'
' COPERTURA (14 funzioni):
'   AGENTPAUSE, AGENTRESUME, AGENTSEND, AGENTRECEIVE, AGENTSTOP,
'   AGENTQUEUECLEAR, AGENTQUEUESTATS,
'   CHANNELCREATE, CHANNELPUBLISH, CHANNELSUBSCRIBE, CHANNELUNSUBSCRIBE,
'   BRAINDETACH, BRAINUSE.
' Solo operazioni deterministiche locali (nessuna rete): gli output con Id/Timestamp casuali
' (AGENTRECEIVE) sono verificati con proprietà stabili.
' =================================================================================================
IMPORT AI

SUB Main()
    ' NB: niente CLS — emette un escape ANSI solo sulle gambe C (gmb_cls) e non su IL/PCode
    ' headless → output non identico fra backend. La pulizia schermo non è parte del test.
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ AI Agent Lifecycle + Channels + Brain — Copertura Completa             ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- 1. AGENT CREATE (prerequisito) ---
    AI AGENT CREATE "demoAgent"
    PRINT "1. AGENT CREATE 'demoAgent'"

    ' --- 2. AGENT SEND / AGENT RECEIVE (coda locale) ---
    DIM ok = AI.AGENTSEND("demoAgent", "task1", "test message")
    PRINT "2. AGENTSEND -> "; ok
    DIM rx$ = AI.AGENTRECEIVE("demoAgent")
    ' Il JSON del messaggio contiene Id/Timestamp CASUALI → stampo una proprietà stabile.
    PRINT "   AGENTRECEIVE ricevuto -> "; IIF(LEN(rx$) > 0, 1, 0)

    ' --- 3. AGENT QUEUESTATS ---
    DIM qs$ = AI.AGENTQUEUESTATS("demoAgent")
    PRINT "3. AGENTQUEUESTATS -> "; qs$

    ' --- 4. AGENT PAUSE ---
    DIM p = AI.AGENTPAUSE("demoAgent")
    PRINT "4. AGENTPAUSE -> "; p

    ' --- 5. AGENT RESUME ---
    DIM r = AI.AGENTRESUME("demoAgent")
    PRINT "5. AGENTRESUME -> "; r

    ' --- 6. AGENT STOP ---
    DIM s = AI.AGENTSTOP("demoAgent")
    PRINT "6. AGENTSTOP -> "; s

    ' --- 7. AGENT QUEUECLEAR ---
    AI AGENT CREATE "queueAgent"
    DIM x = AI.AGENTSEND("queueAgent", "t1", "msg1")
    DIM c = AI.AGENTQUEUECLEAR("queueAgent")
    PRINT "7. AGENTQUEUECLEAR -> "; c
    DIM qs2$ = AI.AGENTQUEUESTATS("queueAgent")
    PRINT "   QueueStats after clear -> "; qs2$

    ' --- 8. CHANNEL CREATE ---
    DIM ch = AI.CHANNELCREATE("demoChan")
    PRINT "8. CHANNELCREATE 'demoChan' -> "; ch

    ' --- 9. CHANNEL SUBSCRIBE ---
    DIM chanSub = AI.CHANNELSUBSCRIBE("demoChan", "demoAgent")
    PRINT "9. CHANNELSUBSCRIBE -> "; chanSub

    ' --- 10. CHANNEL PUBLISH ---
    DIM pub = AI.CHANNELPUBLISH("demoChan", "hello from channel")
    PRINT "10. CHANNELPUBLISH -> "; pub

    ' --- 11. CHANNEL UNSUBSCRIBE ---
    DIM chanUnsub = AI.CHANNELUNSUBSCRIBE("demoChan", "demoAgent")
    PRINT "11. CHANNELUNSUBSCRIBE -> "; chanUnsub

    ' --- 12. BRAIN USE ---
    DIM bu = AI.BRAINUSE("default")
    PRINT "12. BRAINUSE('default') -> "; bu

    ' --- 13. BRAIN DETACH ---
    DIM bd = AI.BRAINDETACH("default")
    PRINT "13. BRAINDETACH('default') -> "; bd

    ' --- 14. AGENT DELETE ---
    ' NB: AI.BRAINUSED() NON è coperto qui: ha un bug di tipo pre-esistente (la StdLib lo
    ' dichiara `string[]` ma il runtime ritorna `string` → InvalidCastException su IL). Da
    ' sistemare a parte (vedi session log 2026-09-16).
    AI AGENT DELETE "demoAgent"
    AI AGENT DELETE "queueAgent"
    PRINT "14. Agent cleanup completato."

    PRINT
    PRINT "Test completato."
END SUB