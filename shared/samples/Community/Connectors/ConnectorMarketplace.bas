' FILE: Connectors/ConnectorMarketplace.bas
' Marketplace 52 connettori (T0-T4) — browse, search e load

IMPORT AI

SUB Main()
    PRINT "=== Marketplace 52 Connettori ==="

    ' Lista completa (statico, 52 entry T0-T4)
    DIM all$ AS STRING = AI.CONNECTORMARKETPLACELIST()
    PRINT "MARKETPLACE entries="; LEN(all$)

    ' Dettaglio singolo connettore
    DIM gmail$ AS STRING = AI.CONNECTORMARKETPLACEGET("gmail")
    PRINT "gmail: "; LEFT(gmail$, 300)
    DIM gh$ AS STRING = AI.CONNECTORMARKETPLACEGET("github")
    PRINT "github: "; LEFT(gh$, 300)
    DIM pg$ AS STRING = AI.CONNECTORMARKETPLACEGET("postgres")
    PRINT "postgres: "; LEFT(pg$, 300)

    ' Ricerca per capability (tool discovery)
    DIM mail$ AS STRING = AI.CONNECTORSEARCH("mail")
    PRINT "SEARCH mail found len="; LEN(mail$)
    PRINT LEFT(mail$, 400)

    DIM cal$ AS STRING = AI.CONNECTORSEARCH("calendar")
    PRINT "SEARCH calendar len="; LEN(cal$)

    DIM db$ AS STRING = AI.CONNECTORSEARCH("db")
    PRINT "SEARCH db len="; LEN(db$)

    DIM ai$ AS STRING = AI.CONNECTORSEARCH("vector")
    PRINT "SEARCH vector len="; LEN(ai$)

    ' Ricerca per kind
    DIM gh2$ AS STRING = AI.CONNECTORSEARCH("github")
    PRINT "SEARCH github: "; LEFT(gh2$, 300)

    ' Load dal marketplace (registra + abilita)
    AI CONNECTOR LOAD "slack"
    PRINT "LOAD slack OK"
    DIM st$ AS STRING = AI.CONNECTORSTATUS("slack")
    PRINT "slack status: "; LEFT(st$, 200)

    AI CONNECTOR LOAD "notion"
    PRINT "LOAD notion OK"

    ' Lista registrati dopo load
    DIM reg$ AS STRING = AI.CONNECTORLIST()
    PRINT "REGISTERED after load: "; LEFT(reg$, 300)

    PRINT "=== Marketplace OK ==="
END SUB
