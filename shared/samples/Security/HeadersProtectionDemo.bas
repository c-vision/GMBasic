' FILE: Security/HeadersProtectionDemo.bas
' =================================================================================================
'       SECURITY — protezione del server HTTP (header, IP, body, rate, TLS)
' =================================================================================================
' Eseguire con:  dotnet run --project REPL/REPL.csproj -- HeadersProtectionDemo.bas
' Poi nel browser/curl su http://localhost:8093/api/health :
'   - le risposte portano gli header di sicurezza (HSTS/CSP/X-Frame-Options/...);
'   - una richiesta da un IP bloccato (o fuori dall'allow-list) -> 403;
'   - un body oltre MaxRequestBodySize -> 413; troppe richieste -> 429.
' Per HTTPS (facoltativo): scommentare CertificatePath/Password (il server parte in TLS).
' Eseguibile su IL, PCodeVM e VM/C (header/IP/413/429/security).

IMPORT HTTP

SUB HealthRoute(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    res.SendJSON({ status = "ok", service = "headers-protection" })
END SUB

' --- MAIN ---
DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
cfg.Port = 8093

' Header di sicurezza automatici su ogni risposta (HSTS/CSP/X-Frame-Options/...)
cfg.SecurityHeaders = TRUE

' Chi può accedere: solo localhost + LAN privata (CIDR); mai 203.0.113.7 (TEST-NET-3)
cfg.AllowedIPs = "127.0.0.1, 192.168.0.0/16, 10.0.0.0/8"
cfg.BlockedIPs = "203.0.113.7"

' Limiti anti-abuso/DoS
cfg.MaxRequestBodySize = 1 * 1024 * 1024      ' body > 1 MB -> 413
cfg.RateLimitRequestsPerMinute = 600          ' oltre -> 429
cfg.RateLimitBurstSize = 1000
cfg.RequestTimeoutMs = 15000

' TLS/HTTPS (facoltativo, richiede un certificato .pfx):
' cfg.CertificatePath = "server.pfx"
' cfg.CertificatePassword = "pw"

HTTP CONFIG cfg

HTTP ROUTE "GET", "/api/health", HealthRoute()
HTTP SERVER cfg.Port
