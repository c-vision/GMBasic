' FILE: samples/Community/Ai/AgentObject.bas
' =================================================================================================
' AgentObject — CLASSE AGENT (include/Agent.inc): property access tipizzato su un agente (A1).
' =================================================================================================
' Invece di AI AGENT SET nome key value / AI.AGENTGET si usa un oggetto AGENTS.AgentHandle con
' PROPERTY tipizzate (Model, SystemPrompt, Provider, Temperature, MaxTokens, flag ON/OFF).
' Le PROPERTY fanno write-through sul record agente reale (delegano ai builtin AI AGENT SET /
' AI.AGENTGET, cross-backend). Verifica: SET scrive SUBITO (riletto via AI.AGENTGET) e GET
' rilegge quanto scritto.

IMPORT AI WITH ALIAS
IMPORT AGENTS

SUB Main()
    AI AGENT CREATE AgentObj

    DIM a AS AGENTS.AgentHandle
    a = NEW AGENTS.AgentHandle("AgentObj")

    ' --- SET via property (write-through su AgentServer) ---
    a.Model = "qwen3.5-probe"
    a.Temperature = 0.42
    a.MaxTokens = 512
    a.SystemPrompt = "Sei un agente di test A1."
    a.Provider = "fixture"
    a.MemoryEnabled = "ON"

    ' --- GET via property ---
    PRINT "Model="; a.Model
    PRINT "Temp="; a.Temperature
    PRINT "MaxTokens="; a.MaxTokens
    PRINT "SystemPrompt="; a.SystemPrompt
    PRINT "Provider="; a.Provider
    PRINT "Memory="; a.MemoryEnabled

    ' --- Verifica cross: riletto con le funzioni standard ---
    PRINT "CrossModel="; AI.AGENTGET("AgentObj", "model")
    PRINT "CrossTemp="; AI.AGENTGET("AgentObj", "temperature")
    PRINT "CrossMemory="; AI.AGENTGET("AgentObj", "memory")

    AI AGENT DELETE AgentObj
    PRINT "AGENT-OBJECT-OK"
END SUB
