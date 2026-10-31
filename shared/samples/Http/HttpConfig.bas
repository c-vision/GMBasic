' ==============================================================================
' Sample: HTTPCONFIG — Configurazione del server HTTP
' ==============================================================================
' Dimostra l'uso dell'oggetto HTTPCONFIG per configurare il server HTTP.
' 
' Proprietà disponibili:
'   Port, RequestTimeoutMs, DefaultRequestTimeout, MaxRequestBodySize,
'   EnableKeepAlive, RateLimitRequestsPerMinute, RateLimitBurstSize,
'   CertificatePath, CertificatePassword
' ==============================================================================

' --- Crea un oggetto di configurazione ---
DIM config AS HTTPCONFIG = NEW HTTPCONFIG

' --- Legge i valori di default ---
PRINT "Valori di default HTTPCONFIG:"
PRINT "  Port: "; config.Port
PRINT "  RequestTimeoutMs: "; config.RequestTimeoutMs
PRINT "  DefaultRequestTimeout: "; config.DefaultRequestTimeout
PRINT "  MaxRequestBodySize: "; config.MaxRequestBodySize
PRINT "  EnableKeepAlive: "; config.EnableKeepAlive
PRINT "  RateLimitRequestsPerMinute: "; config.RateLimitRequestsPerMinute
PRINT "  RateLimitBurstSize: "; config.RateLimitBurstSize

' --- Modifica le proprietà ---
config.Port = 3000
config.RequestTimeoutMs = 10000
config.MaxRequestBodySize = 5242880  ' 5 MB
config.EnableKeepAlive = FALSE

PRINT ""
PRINT "Valori modificati:"
PRINT "  Port: "; config.Port
PRINT "  RequestTimeoutMs: "; config.RequestTimeoutMs
PRINT "  DefaultRequestTimeout: "; config.DefaultRequestTimeout
PRINT "  MaxRequestBodySize: "; config.MaxRequestBodySize
PRINT "  EnableKeepAlive: "; config.EnableKeepAlive

' --- Usa DefaultRequestTimeout come alias ---
config.DefaultRequestTimeout = 15000
PRINT ""
PRINT "  Dopo config.DefaultRequestTimeout = 15000:"
PRINT "  RequestTimeoutMs: "; config.RequestTimeoutMs
PRINT "  DefaultRequestTimeout: "; config.DefaultRequestTimeout

PRINT ""
PRINT "Test HTTPCONFIG completato."
