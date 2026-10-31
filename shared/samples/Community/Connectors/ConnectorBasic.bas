' FILE: Connectors/ConnectorBasic.bas
' Connector Runtime F0 — demo AI CONNECTOR (namespace AI, MCP SYSTEM spec §24-35)
' Mostra: LIST, REGISTER, STATUS, MARKETPLACE, SEARCH, POLICY, SECRET, CALL

IMPORT AI

SUB Main()
    PRINT "=== AI CONNECTOR Runtime F0 Demo ==="

    ' 1. Lista connettori registrati (via comando e funzione)
    AI CONNECTOR LIST
    DIM all$ AS STRING = AI.CONNECTORLIST()
    PRINT "REGISTERED: "; LEFT(all$, 200)

    ' 2. Marketplace: 52 connettori (T0-T4) — Gmail, GitHub, Slack etc.
    AI CONNECTOR MARKETPLACE LIST
    DIM market$ AS STRING = AI.CONNECTORMARKETPLACELIST()
    PRINT "MARKETPLACE size="; LEN(market$)

    ' 3. Registra un connettore dal marketplace (auto-discovery)
    DIM g$ AS STRING = AI.CONNECTORMARKETPLACEGET("gmail")
    PRINT "GET gmail: "; LEFT(g$, 200)

    ' 4. Registra un connettore custom (github)
    AI CONNECTOR REGISTER "my-github", "github"
    PRINT "REGISTER my-github OK"

    ' 5. Status e tools
    DIM st$ AS STRING = AI.CONNECTORSTATUS("my-github")
    PRINT "STATUS my-github: "; LEFT(st$, 200)
    DIM tools$ AS STRING = AI.CONNECTORTOOLS("my-github")
    PRINT "TOOLS: "; LEFT(tools$, 200)

    ' 6. Ricerca per capability (tool discovery dinamica)
    AI CONNECTOR SEARCH "mail"
    DIM found$ AS STRING = AI.CONNECTORSEARCH("mail")
    PRINT "SEARCH mail: "; LEFT(found$, 300)

    DIM found2$ AS STRING = AI.CONNECTORSEARCH("calendar")
    PRINT "SEARCH calendar: "; LEFT(found2$, 300)

    ' 7. Policy per-tool (READ/WRITE/DESTRUCTIVE + HIGH_RISK approval)
    AI CONNECTOR POLICY SET "my-github", "send", "approve"
    DIM pol$ AS STRING = AI.CONNECTORPOLICYGET("my-github", "send")
    PRINT "POLICY my-github.send="; pol$

    ' 8. Secret / OAuth (secure storage per-connector, Keychain/Secret Service/Credential Manager su host)
    AI CONNECTOR SECRET SET "my-github", "api_key", "ghp_test123"
    DIM sec$ AS STRING = AI.CONNECTORSECRETGET("my-github", "api_key")
    PRINT "SECRET api_key="; sec$

    ' OAuth: salva un token (simula PKCE flow via include/OAuthClient.inc)
    AI CONNECTOR AUTH "my-github", "{""access_token"":""tok123"",""refresh_token"":""ref456"",""scope"":""repo""}"
    PRINT "AUTH OK (Bearer tok123 stored)"

    ' 9. Chiamata tool (native stub per F0, delega a MCP client se backing MCP server)
    DIM res$ AS STRING = AI.CONNECTORCALL("my-github", "issues", "{""repo"":""GMBasic""}")
    PRINT "CALL my-github.issues: "; LEFT(res$, 300)

    ' 10. Abilita/disabilita e refresh (discovery)
    AI CONNECTOR DISABLE "my-github"
    PRINT "DISABLE OK"
    AI CONNECTOR ENABLE "my-github"
    PRINT "ENABLE OK"
    AI CONNECTOR REFRESH "my-github"
    PRINT "REFRESH OK"

    ' 11. Risorse e prompt (estensioni MCP resources/read, prompts/get, ping)
    DIM r$ AS STRING = AI.CONNECTORRESOURCES("my-github")
    PRINT "RESOURCES: "; LEFT(r$, 200)
    DIM p$ AS STRING = AI.CONNECTORPROMPTS("my-github")
    PRINT "PROMPTS: "; LEFT(p$, 200)

    ' 12. Alias legacy senza AI (compatibilità): CONNECTOR LIST
    CONNECTOR LIST
    PRINT "LEGACY CONNECTOR LIST OK"

    PRINT "=== Demo completata: F0 8 primitive (CONNECTOR/AUTH/TOOL/RESOURCE/POLICY/APPROVAL/SECRET/SESSION) OK ==="
END SUB
