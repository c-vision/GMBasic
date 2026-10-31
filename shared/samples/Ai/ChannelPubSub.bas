' FILE: Ai/ChannelPubSub.bas
' Canali L1 (pub/sub) — comandi `AI CHANNEL CREATE/PUBLISH/SUBSCRIBE/UNSUBSCRIBE` e
' funzioni fuse `AI.CHANNELSUBSCRIBE` / `AI.CHANNELUNSUBSCRIBE`.
'
' SUBSCRIBE/UNSUBSCRIBE sono NO-OP di PARITÀ: in BASIC il "handler" è un nome agente e il
' routing a callback non è esposto dal runtime → tutti i backend ritornano 1 (IL/PCode
' `AiChannelSubscribe => 1.0`, VM/C, C99Native). Output DETERMINISTICO (a
' differenza di AgentQueueDemo.bas, che usa UUID/timestamp casuali nei messaggi).
'
' Verificato su 4 backend: parità IL+PCode+VM/C, gate driver C99Native.
IMPORT AI

SUB Main()
    PRINT "=== Channel pub/sub ==="

    ' --- comandi (forma legacy a spazio, §2.0) ---
    AI CHANNEL CREATE Chan1
    PRINT "create:   "; 1

    AI CHANNEL PUBLISH Chan1, "Hello via channel"
    PRINT "publish:  "; 1

    AI CHANNEL SUBSCRIBE Chan1, "Handler1"
    PRINT "sub:      "; 1

    AI CHANNEL UNSUBSCRIBE Chan1, "Handler1"
    PRINT "unsub:    "; 1

    ' --- forma FUNZIONE fusa (valore di ritorno numerico: 1 = ok) ---
    DIM r1 AS INTEGER
    r1 = AI.CHANNELSUBSCRIBE("Chan2", "Handler2")
    PRINT "fnsub:    "; r1

    DIM r2 AS INTEGER
    r2 = AI.CHANNELUNSUBSCRIBE("Chan2", "Handler2")
    PRINT "fnunsub:  "; r2

    PRINT "=== done ==="
END SUB
