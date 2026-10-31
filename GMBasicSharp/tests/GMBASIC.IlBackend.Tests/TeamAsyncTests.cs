// FILE: IlBackend.Tests.TeamAsyncTests
// TEAM item 3: RUN asincrono + WAIT. TeamRunStart lancia il team su Task.Run (non blocca) e
// ritorna un handle; TeamRunWait attende il risultato. Verifica che Start ritorni SUBITO.
using System;
using System.Diagnostics;
using Basic.Agents;
using Basic.IlBackend;
using Xunit;

namespace GMBasic.IlBackend.Tests;

public sealed class TeamAsyncTests
{
    [Fact]
    public void TeamRunStartIsNonBlocking_AndWaitReturnsResult()
    {
        var tag = Guid.NewGuid().ToString("N")[..6];
        var server = AgentServer.Instance;
        server.Create("afx" + tag + "1");
        server.Create("afx" + tag + "2");
        server.Set("afx" + tag + "1", "provider", "ghost");   // fallisce
        server.Set("afx" + tag + "2", "provider", "ghost");
        TeamServer.Instance.Create("asyncT" + tag);
        TeamServer.Instance.AddAgent("asyncT" + tag, "afx" + tag + "1");
        TeamServer.Instance.AddAgent("asyncT" + tag, "afx" + tag + "2");

        var sw = Stopwatch.StartNew();
        var handle = IlRuntime.TeamRunStart("asyncT" + tag, "task di prova");
        sw.Stop();
        Assert.False(string.IsNullOrEmpty(handle));
        Assert.StartsWith("team_", handle);
        // Start ritorna SUBITO (lancia su Task.Run): sotto 5s senza mai eseguire un run reale bloccante.
        Assert.True(sw.ElapsedMilliseconds < 5000, $"Start ha bloccato: {sw.ElapsedMilliseconds}ms");

        var result = IlRuntime.TeamRunWait(handle);
        Assert.NotNull(result);
        // errori membri gestiti nel run async → presenti nel result
        Assert.Contains("errore", result);
    }
}
