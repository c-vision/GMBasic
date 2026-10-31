// FILE: Basic.IlBackend.PCode.Http.PCodeHttpRateLimiter.cs
// Porta di Interpreter/HTTP/HttpAddons.cs::RateLimiter (POCO, nessuna dipendenza da libuv/rete —
// riusabile). A differenza dell'AST, dove e' configurato ma MAI interrogato nel flusso di
// richieste vivo (bug noto, vedi audit), qui viene realmente applicato in
// PCodeProcessor.DispatchHttpRequestAsync prima del routing — questo ha fatto emergere un bug
// REALE nell'algoritmo originale (mai scoperto perche' mai eseguito nell'AST): il bucket
// partiva a 0 token e si riempiva solo dopo un minuto intero trascorso, quindi rifiutava
// TUTTO il traffico di ogni client per il primo minuto. Corretto qui inizializzando il bucket
// pieno (maxTokens) alla prima richiesta di quel client — semantica standard di token-bucket.

using System.Collections.Concurrent;

namespace Basic.IlBackend.PCode.Http;

internal sealed class PCodeHttpRateLimiter
{
    private readonly ConcurrentDictionary<string, ClientBucket> _buckets = new();
    private int _requestsPerMinute = 60;
    private int _burstSize = 120;

    public void Configure(int requestsPerMinute, int burstSize)
    {
        _requestsPerMinute = requestsPerMinute;
        _burstSize = burstSize;
    }

    public bool IsAllowed(string clientId)
    {
        var bucket = _buckets.GetOrAdd(clientId, _ => new ClientBucket());
        return bucket.TryConsume(_requestsPerMinute, _burstSize);
    }

    private sealed class ClientBucket
    {
        private int _tokens = -1; // -1 = bucket non ancora inizializzato per questo client
        private DateTime _lastRefill = DateTime.UtcNow;
        private readonly object _lock = new();

        public bool TryConsume(int refillRate, int maxTokens)
        {
            lock (_lock)
            {
                if (_tokens < 0) { _tokens = maxTokens; _lastRefill = DateTime.UtcNow; }
                var now = DateTime.UtcNow;
                var timePassed = now - _lastRefill;
                if (timePassed.TotalMinutes >= 1)
                {
                    var tokensToAdd = (int)(timePassed.TotalMinutes * refillRate);
                    _tokens = Math.Min(maxTokens, _tokens + tokensToAdd);
                    _lastRefill = now;
                }
                if (_tokens > 0) { _tokens--; return true; }
                return false;
            }
        }
    }
}
