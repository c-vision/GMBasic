using System;
using Basic.Agents;
using Xunit;

namespace AI.Tests;

public sealed class AgentVerifierTests
{
    [Fact]
    public void SucceedsAfterRetries_WithFeedback()
    {
        // produce un risultato che fallisce la validate 2 volte, poi ok
        int attempts = 0;
        var r = AgentVerifier.Verify(
            (n, fb) => { attempts++; return ("r" + n, ""); },
            res => res == "r3",   // accetta solo al 3o
            maxRetries: 3);
        Assert.True(r.Ok);
        Assert.Equal(3, attempts);
        Assert.Equal("r3", r.Result);
        Assert.Equal(3, r.Attempts);
    }

    [Fact]
    public void ExplicitFailAfterMaxRetries_NotSilent()
    {
        int attempts = 0;
        var r = AgentVerifier.Verify(
            (n, fb) => { attempts++; return ("", "compile error sempre"); },
            null, 2);
        Assert.False(r.Ok);
        Assert.Equal(2, attempts);
        Assert.False(string.IsNullOrEmpty(r.LastError));  // errore esplicito, mai silenzioso
        Assert.Contains("compile error sempre", r.LastError);
    }
}
