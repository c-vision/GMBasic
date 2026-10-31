// Sessioni route HTTP (engine, 2026-09-02): HttpSessions.Ensure crea alla prima richiesta
// (Set-Cookie), riusa alla seconda (stesso cookie) e i dati sono LIVE (JsonObject condiviso
// con lo store — nessun write-back esplicito).
using Xunit;
using CommonLib.Http;

namespace Basic.IlBackend.Tests;

public class HttpSessionsTests
{
    [Fact]
    public void Ensure_Creates_Then_Reuses_Same_Live_Data()
    {
        // Prima richiesta: nessun cookie -> nuova sessione con Set-Cookie
        var (setCookie1, data1) = HttpSessions.Ensure(null);
        Assert.NotNull(setCookie1);
        Assert.StartsWith("GMBSESSION=", setCookie1);

        // L'handler scrive nella sessione
        data1["user"] = "alice";

        // Seconda richiesta: il client manda il cookie ricevuto -> stessa sessione, niente cookie nuovo
        var id = setCookie1.Split(';')[0]["GMBSESSION=".Length..];
        var (setCookie2, data2) = HttpSessions.Ensure("GMBSESSION=" + id);
        Assert.Null(setCookie2);
        Assert.Same(data1, data2);          // JsonObject VIVO
        Assert.Equal("alice", data2["user"]?.GetValue<string>());

        // Mutazione viva persistita senza write-back
        data2["role"] = "admin";
        var (_, data3) = HttpSessions.Ensure("GMBSESSION=" + id);
        Assert.Same(data1, data3);
        Assert.Equal("admin", data3["role"]?.GetValue<string>());
    }

    [Fact]
    public void SessionCreate_Server_Mints_Session_With_User_Role()
    {
        var cookie = Basic.IlBackend.IlRuntime.HttpSessionCreate("alice", "admin");
        Assert.StartsWith("GMBSESSION=", cookie);
        var id = cookie.Split(';')[0]["GMBSESSION=".Length..];
        var data = HttpSessions.Store.GetSession(id);
        Assert.NotNull(data);
        Assert.Equal("alice", data["user"]?.GetValue<string>());
        Assert.Equal("admin", data["role"]?.GetValue<string>());
    }
}
