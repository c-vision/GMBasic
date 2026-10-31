' FILE: Connectors/ConnectorOpenApiPetstore.bas
' F4 OpenAPI auto-gen — genera connettore da openapi.json (petstore)

IMPORT AI

SUB Main()
    PRINT "=== OpenAPI Petstore F4 ==="

    ' Registra connettore da openapi.json (fetch + parse paths → tools)
    AI CONNECTOR REGISTER "petstore", "openapi", "https://petstore.swagger.io/v2/swagger.json"
    PRINT "REGISTER petstore openapi OK"

    DIM st$ AS STRING = AI.CONNECTORSTATUS("petstore")
    PRINT "STATUS petstore: "; LEFT(st$, 400)

    DIM tools$ AS STRING = AI.CONNECTORTOOLS("petstore")
    PRINT "TOOLS petstore: "; LEFT(tools$, 400)

    ' Chiama un'operazione generata (es. getPetById)
    DIM res$ AS STRING = AI.CONNECTORCALL("petstore", "getPetById", "{""petId"":1}")
    PRINT "CALL getPetById: "; LEFT(res$, 400)

    ' Marketplace + discovery
    DIM m$ AS STRING = AI.CONNECTORMARKETPLACELIST()
    PRINT "MARKETPLACE size="; LEN(m$)

    PRINT "=== OpenAPI F4 OK ==="
END SUB
