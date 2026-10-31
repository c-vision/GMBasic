// FILE: Basic.Runtime.IServices.ITimeService.cs

namespace Basic.Runtime;

/// <summary>Servizio data/ora/timer per il BASIC Runtime.</summary>
public interface ITimeService
{
    DateTime Now { get; }
    DateTime UtcNow { get; }
    string Format(DateTime dt, string format);
    DateTime Parse(string text);
    TimeSpan Diff(DateTime a, DateTime b);
    void Sleep(int milliseconds);
    long TickCount { get; }
    int Timer { get; }
}
