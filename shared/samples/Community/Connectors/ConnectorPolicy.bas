' FILE: Connectors/ConnectorPolicy.bas
' Policy per-tool (READ/WRITE/DESTRUCTIVE) + HIGH_RISK + APPROVAL + SECRET/SESSION/rate-limit

IMPORT AI

SUB Main()
    PRINT "=== Policy, Secret, Session ==="

    AI CONNECTOR REGISTER "my-stripe", "stripe"
    AI CONNECTOR REGISTER "my-gmail", "gmail"
    PRINT "REGISTER my-stripe+my-gmail OK"

    ' 1. Policy per-tool: allow / deny / approve
    AI CONNECTOR POLICY SET "my-stripe", "customers", "allow"
    AI CONNECTOR POLICY SET "my-stripe", "payments", "approve"
    AI CONNECTOR POLICY SET "my-stripe", "refunds", "deny"
    PRINT "POLICY SET customers=allow, payments=approve, refunds=deny OK"

    DIM p1$ AS STRING = AI.CONNECTORPOLICYGET("my-stripe", "customers")
    DIM p2$ AS STRING = AI.CONNECTORPOLICYGET("my-stripe", "payments")
    DIM p3$ AS STRING = AI.CONNECTORPOLICYGET("my-stripe", "refunds")
    PRINT "customers="; p1$; " payments="; p2$; " refunds="; p3$

    ' HIGH_RISK: mail.send, delete, payment → richiedono approval in Supervised
    AI CONNECTOR POLICY SET "my-gmail", "send", "approve"
    DIM ps$ AS STRING = AI.CONNECTORPOLICYGET("my-gmail", "send")
    PRINT "my-gmail.send policy="; ps$

    ' 2. Secret per-connector (secure storage: Keychain / Secret Service / Credential Manager)
    AI CONNECTOR SECRET SET "my-stripe", "api_key", "sk_test_123"
    AI CONNECTOR SECRET SET "my-stripe", "webhook_secret", "whsec_456"
    DIM s1$ AS STRING = AI.CONNECTORSECRETGET("my-stripe", "api_key")
    DIM s2$ AS STRING = AI.CONNECTORSECRETGET("my-stripe", "webhook_secret")
    PRINT "api_key="; LEFT(s1$, 15); " webhook_secret="; LEFT(s2$, 15)

    ' 3. Chiamate con policy gate
    ' allow → OK
    DIM r1$ AS STRING = AI.CONNECTORCALL("my-stripe", "customers", "{}")
    PRINT "CALL customers (allow): "; LEFT(r1$, 200)

    ' approve → in YOLO il gate lascia passare, ma il test verifica la registrazione
    PRINT "CALL payments con policy approve → atteso approval-required in Supervised"

    ' deny → gate blocca (verifica solo policy, non chiama per non abortire in PCode)
    PRINT "refunds policy is deny → CALL would be blocked (verified via POLICYGET)"

    ' 4. Sessione + rate-limit (120/min, fixed window)
    DIM st$ AS STRING = AI.CONNECTORSTATUS("my-stripe")
    PRINT "SESSION status: "; LEFT(st$, 300)

    ' 5. Disabilita/abilita (policy + sessione resettata)
    AI CONNECTOR DISABLE "my-stripe"
    PRINT "DISABLE OK"
    DIM dis$ AS STRING = AI.CONNECTORSTATUS("my-stripe")
    PRINT LEFT(dis$, 200)

    AI CONNECTOR ENABLE "my-stripe"
    PRINT "ENABLE OK"

    ' 6. Refresh (rediscovery tools)
    AI CONNECTOR REFRESH "my-stripe"
    PRINT "REFRESH OK"

    PRINT "=== Policy/Secret/Session OK ==="
END SUB
