// FILE: Basic.Providers.ProviderCircuitBreaker
namespace Basic.Providers;

/// <summary>Simple circuit breaker for HTTP provider calls.</summary>
public sealed class ProviderCircuitBreaker
{
    private readonly int _failureThreshold;
    private readonly int _resetIntervalMs;
    private int _failureCount;
    private DateTime _lastFailure;
    private CircuitState _state = CircuitState.Closed;
    private readonly Lock _lock = new();

    public enum CircuitState { Closed, Open, HalfOpen }

    public ProviderCircuitBreaker(int failureThreshold = 3, int resetIntervalMs = 30_000)
    {
        _failureThreshold = failureThreshold;
        _resetIntervalMs = resetIntervalMs;
    }

    public bool IsOpen
    {
        get
        {
            lock (_lock)
            {
                if (_state == CircuitState.Open && (DateTime.UtcNow - _lastFailure).TotalMilliseconds > _resetIntervalMs)
                {
                    _state = CircuitState.HalfOpen;
                    return false;
                }
                return _state == CircuitState.Open;
            }
        }
    }

    public void RecordSuccess()
    {
        lock (_lock)
        {
            _failureCount = 0;
            _state = CircuitState.Closed;
        }
    }

    public void RecordFailure()
    {
        lock (_lock)
        {
            _failureCount++;
            _lastFailure = DateTime.UtcNow;
            if (_failureCount >= _failureThreshold)
                _state = CircuitState.Open;
        }
    }
}
