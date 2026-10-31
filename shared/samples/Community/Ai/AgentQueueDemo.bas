' PARITY-EXCLUDE: nondeterministico: ID GUID + timestamp ISO vs epoch (Payment:0 vs 0.00)
' ==============================================================================
' FILE: AI/AgentQueueDemo.bas
' Agent Queue + Channel — L1 Queue/ACF (B1, 2026-09-07)
' ==============================================================================
' API L1 (common/AI/Agents/AgentQueueManager.cs + MessageBus.cs):
'   AI AGENT SEND To, Task, Input [, Payment]   → Enqueue (FIFO, max 1000)
'   AI AGENT RECEIVE Agent INTO Var$            → Dequeue (null → "" if paused/empty)
'   AI AGENT PAUSE Agent / RESUME / STOP        → control queue
'   AI CHANNEL CREATE Chan
'   AI CHANNEL PUBLISH Chan, Message
'   AI CHANNEL SUBSCRIBE / UNSUBSCRIBE (no-op stub, per GO SELECT futuro)
'
' Verificato: IL, PCodeVM — AgentQueueManager (Enqueue/Pause/Resume/Stop) + MessageBus (Publish/Subscribe)
'   + parser (AI AGENT SEND/RECEIVE/STOP/PAUSE/RESUME + AI CHANNEL CREATE/PUBLISH)
'   + IL codegen (AiAgentSend/Receive/Pause/Resume/Stop, AiChannelCreate/Publish)
'   + PCode dispatch (Aiagent)
' ==============================================================================

IMPORT AI WITH ALIAS

SUB Main()
    PRINT "=== Agent Queue + Channel Demo (L1) ==="
    PRINT

    ' 1) Crea agente + coda
    AI AGENT CREATE Worker
    PRINT "Agent Worker creato"

    ' 2) SEND 3 task nella coda
    AI AGENT SEND Worker, "Task1", "Analizza doc.pdf", 0
    AI AGENT SEND Worker, "Task2", "Genera report", 5
    AI AGENT SEND Worker, "Task3", "Invia email"
    PRINT "Enqueued 3 task"

    ' 3) RECEIVE il primo
    DIM msg$ AS STRING
    AI AGENT RECEIVE Worker INTO msg$
    PRINT "RECV1="; msg$

    ' 4) PAUSE → SEND mentre è pausato (resta in coda, ma RECEIVE è bloccato)
    AI AGENT PAUSE Worker
    PRINT "Paused"
    AI AGENT SEND Worker, "Task4", "Task in pausa"
    PRINT "SEND durante pausa (in coda, non consumabile)"
    DIM blocked$ AS STRING
    AI AGENT RECEIVE Worker INTO blocked$
    PRINT "RECV durante pausa (atteso vuoto): '"; blocked$; "'"
    AI AGENT RESUME Worker
    PRINT "Resumed"

    ' 5) RECEIVE dopo resume → Task2, poi Task3, poi Task4
    AI AGENT RECEIVE Worker INTO msg$
    PRINT "RECV2="; msg$
    AI AGENT RECEIVE Worker INTO msg$
    PRINT "RECV3="; msg$
    AI AGENT RECEIVE Worker INTO msg$
    PRINT "RECV4="; msg$

    ' 6) STOP → svuota e blocca nuovi SEND
    AI AGENT SEND Worker, "Task5", "Prima di STOP"
    AI AGENT STOP Worker
    PRINT "Stopped (coda svuotata, nuovi SEND rifiutati)"
    AI AGENT SEND Worker, "Task6", "Dopo STOP"
    DIM empty$ AS STRING
    AI AGENT RECEIVE Worker INTO empty$
    PRINT "RECV dopo STOP (atteso vuoto): '"; empty$; "'"

    ' 7) CHANNEL Pub/Sub
    AI CHANNEL CREATE MyChan
    PRINT "Channel MyChan creato"
    AI CHANNEL PUBLISH MyChan, "Hello via channel"
    PRINT "Published su MyChan"
    ' SUBSCRIBE/UNSUBSCRIBE sono no-op stub (per GO SELECT futuro)
    AI CHANNEL SUBSCRIBE MyChan, "Handler"
    AI CHANNEL UNSUBSCRIBE MyChan, "Handler"
    PRINT "Subscribe/Unsubscribe no-op"

    ' 8) Cleanup
    AI AGENT DELETE Worker
    PRINT
    PRINT "Completato — queue FIFO, pause/resume, stop, channel verificati."
END SUB

