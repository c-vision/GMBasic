' FILE: Connectors/AgentConnectorGmailAuto.bas
' Agente Gmail senza configurazione esplicita — OAuth + policy HIGH_RISK

IMPORT AI
IMPORT CRYPTOGRAPHY
IMPORT STRINGS
IMPORT HTTP
IMPORT OAUTH

SUB Main()
    PRINT "=== Agent Gmail Auto ==="

    AI CONNECTOR LOAD "gmail"
    PRINT "LOAD gmail OK"

    ' Configura OAuth una volta (persistito in config/connectors.json, ConnectorAuth.cs:155)
    ' In produzione: PKCE flow con OAUTH.CreateVerifier/CreateChallenge/AuthUrl/RequestToken
    AI CONNECTOR SECRET SET "gmail", "client_id", "demo_client"
    AI CONNECTOR AUTH "gmail", "{""access_token"":""ya29.demo_gmail"",""refresh_token"":""1//refresh""}"
    PRINT "AUTH gmail (Bearer ya29.demo) OK"

    ' Policy HIGH_RISK: gmail.send richiede approval (SYSTEM.txt §27)
    AI CONNECTOR POLICY SET "gmail", "send", "approve"
    PRINT "POLICY gmail.send=approve (HIGH_RISK) OK"

    AI AGENT CREATE "MailAgent"
    AI AGENT SET "MailAgent" "systemPrompt" "You are an email assistant. Use gmail tools to search and read mail."
    PRINT "Agent MailAgent creato (auto-vede gmail.search/read/send)"

    ' Ciò che l'agente farebbe per "cerca fatture ACME":
    DIM s$ AS STRING = AI.CONNECTORCALL("gmail", "search", "{""query"":""from:ACME fatture""}")
    PRINT "gmail.search: "; LEFT(s$, 300)

    ' Lettura singolo messaggio
    DIM r$ AS STRING = AI.CONNECTORCALL("gmail", "read", "{""id"":""msg123""}")
    PRINT "gmail.read: "; LEFT(r$, 200)

    ' Tentativo send con policy approve → in YOLO passa, in Supervised richiede human
    ' L'agente in Supervised farebbe: AgentPermissionService.RequestApprovalAndWait → UI
    DIM pol$ AS STRING = AI.CONNECTORPOLICYGET("gmail", "send")
    PRINT "policy gmail.send="; pol$

    ' Con LLM reale: AI.AGENTASKSYNC("MailAgent", "cerca le ultime fatture da ACME e riassumi")
    ' → tool_call gmail.search → risultato → risposta

    AI AGENT DELETE "MailAgent"
    PRINT "=== Gmail Auto OK ==="
END SUB
