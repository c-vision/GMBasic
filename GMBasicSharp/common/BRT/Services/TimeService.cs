// FILE: Basic.Runtime.Services.TimeService.cs

namespace Basic.Runtime.Services;

public class TimeService : ITimeService
{
    public DateTime Now => DateTime.Now;
    public DateTime UtcNow => DateTime.UtcNow;
    public string Format(DateTime dt, string format) => dt.ToString(format);
    public DateTime Parse(string text) => DateTime.Parse(text);
    public TimeSpan Diff(DateTime a, DateTime b) => a - b;
    public void Sleep(int milliseconds) => Thread.Sleep(milliseconds);
    public long TickCount => Environment.TickCount64;
    public int Timer => (int)(Environment.TickCount64 % 86400000);
}
