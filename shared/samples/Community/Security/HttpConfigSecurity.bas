' FILE: Security/HttpConfigSecurity.bas
' =================================================================================================
'       SECURITY — HTTP CONFIG: SecurityHeaders / AllowedIPs / BlockedIPs (+ MaxRequestBodySize,
'       RateLimitRequestsPerMinute). Verifica dello schema e dei setters (i valori applicati dal
'       server sono enforce: 403 IP non ammesso, 413 body oltre cap, 429 rate-limit, header di
'       sicurezza su ogni risposta — vedi Security-Best-Practices).
' Eseguibile su IL, PCodeVM e VM/C (2026-09-02).
' =================================================================================================

IMPORT HTTP

PRINT "=== HTTP CONFIG security ==="

DIM config AS HTTPCONFIG = NEW HTTPCONFIG
config.Port = 8080
config.SecurityHeaders = TRUE
config.AllowedIPs = "127.0.0.1, 10.0.0.0/8"
config.BlockedIPs = "203.0.113.7"
config.MaxRequestBodySize = 1048576
config.RateLimitRequestsPerMinute = 120
config.RateLimitBurstSize = 200

IF config.SecurityHeaders THEN
    PRINT "SEC_HEADERS OK"
ELSE
    PRINT "SEC_HEADERS FAIL"
END IF

IF config.AllowedIPs = "127.0.0.1, 10.0.0.0/8" THEN
    PRINT "ALLOW OK"
ELSE
    PRINT "ALLOW FAIL"
END IF

IF config.BlockedIPs = "203.0.113.7" THEN
    PRINT "BLOCK OK"
ELSE
    PRINT "BLOCK FAIL"
END IF

' Applica la configurazione al server corrente (no-op senza HTTP SERVER attivo)
HTTP CONFIG config

PRINT "HttpConfigSecurity demo done"
