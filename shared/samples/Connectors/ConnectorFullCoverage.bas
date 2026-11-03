' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
' FILE: Connectors/ConnectorFullCoverage.bas
' Copertura completa del dominio CONNECTOR: 11 funzioni + 1 comando AI CONNECTOR (tutti i verbi).
' Usa un connettore custom "demo-conn" per il ciclo di vita, con clean-up finale.

IMPORT AI

SUB Main()
    PRINT "=== CONNECTOR full coverage ==="

    ' --- 11 funzioni (lettura sul catalogo) ---
    PRINT "LIST len="; LEN(AI.CONNECTORLIST())
    PRINT "MARKETPLACELIST len="; LEN(AI.CONNECTORMARKETPLACELIST())
    PRINT "MARKETPLACEGET len="; LEN(AI.CONNECTORMARKETPLACEGET("gmail"))
    PRINT "STATUS len="; LEN(AI.CONNECTORSTATUS("gmail"))
    PRINT "TOOLS len="; LEN(AI.CONNECTORTOOLS("gmail"))
    PRINT "SEARCH len="; LEN(AI.CONNECTORSEARCH("mail"))
    PRINT "RESOURCES len="; LEN(AI.CONNECTORRESOURCES("gmail"))
    PRINT "PROMPTS="; AI.CONNECTORPROMPTS("gmail")
    PRINT "POLICYGET="; AI.CONNECTORPOLICYGET("gmail", "send")
    PRINT "SECRETGET="; AI.CONNECTORSECRETGET("gmail", "api_key")

    ' --- Comandi legacy a spazio (stessi TokenKind) ---
    AI CONNECTOR LIST
    AI CONNECTOR MARKETPLACE LIST
    AI CONNECTOR MARKETPLACE GET "gmail"
    AI CONNECTOR SEARCH "mail"
    AI CONNECTOR STATUS "gmail"
    AI CONNECTOR TOOLS "gmail"
    AI CONNECTOR RESOURCES "gmail"
    AI CONNECTOR PROMPTS "gmail"
    AI CONNECTOR POLICY GET "gmail", "send"
    AI CONNECTOR SECRET GET "gmail", "api_key"
    AI CONNECTOR LOAD "slack"
    PRINT "commands OK"

    ' --- Ciclo di vita su connettore custom (register/disable/enable/policy/secret/auth/call/refresh) ---
    AI CONNECTOR REGISTER "demo-conn", "demo"
    PRINT "registered="; LEN(AI.CONNECTORSTATUS("demo-conn"))
    AI CONNECTOR DISABLE "demo-conn"
    PRINT "disabled="; INSTR(AI.CONNECTORSTATUS("demo-conn"), "false")
    AI CONNECTOR ENABLE "demo-conn"
    AI CONNECTOR POLICY SET "demo-conn", "send", "approve"
    PRINT "policy="; AI.CONNECTORPOLICYGET("demo-conn", "send")
    AI CONNECTOR SECRET SET "demo-conn", "api_key", "sk_demo"
    PRINT "secret="; AI.CONNECTORSECRETGET("demo-conn", "api_key")
    DIM q AS STRING = CHR$(34)
    DIM j AS STRING = "{" + q + "access_token" + q + ":" + q + "tok" + q + "}"
    AI CONNECTOR AUTH "demo-conn", j
    PRINT "token="; AI.CONNECTORSECRETGET("demo-conn", "access_token")
    PRINT "call len="; LEN(AI.CONNECTORCALL("demo-conn", "ping", "{}"))
    AI CONNECTOR REFRESH "demo-conn"
    AI CONNECTOR UNREGISTER "demo-conn"
    PRINT "after unregister="; AI.CONNECTORSTATUS("demo-conn")

    PRINT "=== OK ==="
END SUB
