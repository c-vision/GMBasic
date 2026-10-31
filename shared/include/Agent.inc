' FILE: include/Agent.inc
' =================================================================================================
'   CLASSE AGENT — proxy OOP per gli agenti GMBasic (A1: property access su AgentRecord)
' =================================================================================================
' Espone un agente registrato (AI AGENT CREATE) come oggetto con proprietà tipizzate in
' SCRITTURA/LETTURA DIRETTA sul record agente (write-through): le PROPERTY delegano ai builtin
' CROSS-BACKEND `AI AGENT SET <nome> "<key>" <val>` (comando legacy, forma a spazio — le virgole
' sono state rimosse dal parser il 2026-09-06, commit 82e8471f5) e `AI.AGENTGET(<nome>, "<key>")`.
' C99Native, e il programmatore può modificarla (es. aggiungere property/alias).
'
' Uso:
'   IMPORT AI WITH ALIAS              ' abilita il dispatch legacy "AI ..." nei comandi della classe
'   IMPORT AGENTS                     ' importa questa classe (namespace AGENTS)
'   DIM a AS AGENTS.AgentHandle
'   a = NEW AGENTS.AgentHandle("Support")    ' bind per nome agente (deve esistere)
'   a.Model = "qwen3"                          ' property SET -> scrive SUBITO sul record
'   PRINT a.Model                              ' property GET -> legge SUBITO dal record
'
' Chiavi SUPPORTATE (specchio di AgentServer.Set/Get):
'   Model | Temperature | MaxTokens | SystemPrompt | Provider | MemoryEnabled | ThinkingEnabled
'   SkillsEnabled | SessionStreamingEnabled | ClientToolsEnabled | PluginsEnabled | Permission
'   ThinkingTokens | Reflection | Critic
'
' NOTA PARSER/RUNTIME (2026-09-02):
' 1. Il valore del comando legacy `AI AGENT SET n k v` con v identifier nudo viene interpretato
'    come STRINGA LETTERALE (ParseAiValueToken), non come variabile. Per passare un valore
'    dinamico si usa la macro `&s` su una variabile STRING.
' 2. La macro applicata direttamente a un numero (`&value`) non e' uniforme tra PCodeVM e IL
'    (conversione double->string con cultura locale). Strada ROBUSTA: produrre la stringa con
'    `STR$(value)` (invariant, es. "0.42") in una variabile, poi passare `&s`. Le stringhe
'    (property Model/SystemPrompt/...) usano la forma `(value)` — espressione fra parentesi
'    (AGGIORNATO 2026-09-13: ParseAiValueToken tratta l'identifier nudo come STRINGA LETTERALE
'    e la macro & fa espansione TESTUALE — entrambe rompono i valori dinamici; la parentesi
'    cade nel fall-through ParseExpression, precedente canonico: ISAM CREATE INDEX KEY (V)).
' 3. Bug runtime fixato (2026-09-02): AgentServer.Set parsa ora i numerici con InvariantCulture
'    (prima double.TryParse senza cultura -> su macchine it-IT "0.42" diventava 42).
'
' NB sintassi: dentro le PROPERTY la chiave va SEMPRE QUOTATA ("model"). Il NAMESPACE termina a
' fine file (nessun END NAMESPACE).
' =================================================================================================

IMPORT AI WITH ALIAS

NAMESPACE AGENTS

PUBLIC CLASS AgentHandle
    PRIVATE DIM _name AS STRING

    ' Bind: si collega a un agente GIA' registrato (AI AGENT CREATE) per nome.
    PUBLIC SUB NEW(nome AS STRING)
        SELF._name = nome
    END SUB

    ' -- Proprietà stringa (GET/SET su chiavi AgentServer) --

    PUBLIC PROPERTY Model AS STRING
        GET
            RETURN AI.AGENTGET(SELF._name, "model")
        END GET
        SET(BYVAL value AS STRING)
            AI AGENT SET SELF._name "model" (value)
        END SET
    END PROPERTY

    PUBLIC PROPERTY SystemPrompt AS STRING
        GET
            RETURN AI.AGENTGET(SELF._name, "systemPrompt")
        END GET
        SET(BYVAL value AS STRING)
            AI AGENT SET SELF._name "systemPrompt" (value)
        END SET
    END PROPERTY

    PUBLIC PROPERTY Provider AS STRING
        GET
            RETURN AI.AGENTGET(SELF._name, "provider")
        END GET
        SET(BYVAL value AS STRING)
            AI AGENT SET SELF._name "provider" (value)
        END SET
    END PROPERTY

    PUBLIC PROPERTY Permission AS STRING
        GET
            RETURN AI.AGENTGET(SELF._name, "permission")
        END GET
        SET(BYVAL value AS STRING)
            AI AGENT SET SELF._name "permission" (value)
        END SET
    END PROPERTY

    PUBLIC PROPERTY Reflection AS STRING
        GET
            RETURN AI.AGENTGET(SELF._name, "reflection")
        END GET
        SET(BYVAL value AS STRING)
            AI AGENT SET SELF._name "reflection" (value)
        END SET
    END PROPERTY

    PUBLIC PROPERTY Critic AS STRING
        GET
            RETURN AI.AGENTGET(SELF._name, "critic")
        END GET
        SET(BYVAL value AS STRING)
            AI AGENT SET SELF._name "critic" (value)
        END SET
    END PROPERTY

    ' -- Proprietà numeriche (SET con macro &(STR$(...)) per forzare il valore) --

    PUBLIC PROPERTY Temperature AS DOUBLE
        GET
            RETURN VAL(AI.AGENTGET(SELF._name, "temperature"))
        END GET
        SET(BYVAL value AS DOUBLE)
            DIM s AS STRING
            s = STR$(value)
            AI AGENT SET SELF._name "temperature" (s)
        END SET
    END PROPERTY

    PUBLIC PROPERTY MaxTokens AS LONG
        GET
            RETURN VAL(AI.AGENTGET(SELF._name, "maxTokens"))
        END GET
        SET(BYVAL value AS LONG)
            DIM s AS STRING
            s = STR$(value)
            AI AGENT SET SELF._name "maxTokens" (s)
        END SET
    END PROPERTY

    PUBLIC PROPERTY ThinkingTokens AS LONG
        GET
            RETURN VAL(AI.AGENTGET(SELF._name, "thoughtTokens"))
        END GET
        SET(BYVAL value AS LONG)
            DIM s AS STRING
            s = STR$(value)
            AI AGENT SET SELF._name "thoughtTokens" (s)
        END SET
    END PROPERTY

    ' -- Proprietà booleane (valori ON/OFF) --

    PUBLIC PROPERTY MemoryEnabled AS STRING
        GET
            RETURN AI.AGENTGET(SELF._name, "memory")
        END GET
        SET(BYVAL value AS STRING)
            AI AGENT SET SELF._name "memory" (value)
        END SET
    END PROPERTY

    PUBLIC PROPERTY ThinkingEnabled AS STRING
        GET
            RETURN AI.AGENTGET(SELF._name, "thinking")
        END GET
        SET(BYVAL value AS STRING)
            AI AGENT SET SELF._name "thinking" (value)
        END SET
    END PROPERTY

    PUBLIC PROPERTY SkillsEnabled AS STRING
        GET
            RETURN AI.AGENTGET(SELF._name, "skills")
        END GET
        SET(BYVAL value AS STRING)
            AI AGENT SET SELF._name "skills" (value)
        END SET
    END PROPERTY

    PUBLIC PROPERTY SessionStreamingEnabled AS STRING
        GET
            RETURN AI.AGENTGET(SELF._name, "sessionStreaming")
        END GET
        SET(BYVAL value AS STRING)
            AI AGENT SET SELF._name "sessionStreaming" (value)
        END SET
    END PROPERTY

    PUBLIC PROPERTY ClientToolsEnabled AS STRING
        GET
            RETURN AI.AGENTGET(SELF._name, "clientTools")
        END GET
        SET(BYVAL value AS STRING)
            AI AGENT SET SELF._name "clientTools" (value)
        END SET
    END PROPERTY

    PUBLIC PROPERTY PluginsEnabled AS STRING
        GET
            RETURN AI.AGENTGET(SELF._name, "plugins")
        END GET
        SET(BYVAL value AS STRING)
            AI AGENT SET SELF._name "plugins" (value)
        END SET
    END PROPERTY
END CLASS
