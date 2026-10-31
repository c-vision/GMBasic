' FILE: Connectors/ConnectorT2_StripeHubspot.bas
' T2 business (10) — stripe, hubspot, salesforce, twilio via HTTP generico

IMPORT AI

SUB Main()
    PRINT "=== T2 Stripe/HubSpot/Salesforce ==="

    AI CONNECTOR LOAD "stripe"
    AI CONNECTOR SECRET SET "stripe", "api_key", "sk_test_demo"
    DIM cu$ AS STRING = AI.CONNECTORCALL("stripe", "customers", "{}")
    PRINT "stripe.customers: "; LEFT(cu$, 300)

    AI CONNECTOR LOAD "hubspot"
    AI CONNECTOR SECRET SET "hubspot", "api_key", "hub_demo"
    DIM hs$ AS STRING = AI.CONNECTORCALL("hubspot", "contacts", "{}")
    PRINT "hubspot.contacts: "; LEFT(hs$, 300)

    AI CONNECTOR LOAD "salesforce"
    AI CONNECTOR SECRET SET "salesforce", "api_key", "sf_demo"
    DIM sf$ AS STRING = AI.CONNECTORCALL("salesforce", "contacts", "{}")
    PRINT "salesforce.contacts: "; LEFT(sf$, 300)

    AI CONNECTOR LOAD "twilio"
    AI CONNECTOR SECRET SET "twilio", "api_key", "tw_demo"
    DIM tw$ AS STRING = AI.CONNECTORCALL("twilio", "messages", "{}")
    PRINT "twilio.messages: "; LEFT(tw$, 300)

    PRINT "=== T2 OK ==="
END SUB
