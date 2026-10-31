using System.Text.Json.Nodes;

namespace CommonLib.Http;

/// <summary>
/// Sessioni server-side per route HTTP (engine approach, 2026-09-02): lo store è un
/// <see cref="BspSessionManager"/> condiviso di processo; la richiesta porta i dati come
/// <c>REQ("Session")</c> (JsonObject VIVO condiviso con lo store: le modifiche dell'handler
/// persistono automaticamente, nessun write-back esplicito). Cookie GMBSESSION impostato
/// solo alla PRIMA richiesta (nessun cookie = nuova sessione). In futuro i backend storage
/// (memory default / RDD file / ISAM file) vivranno dietro questo stesso punto.
/// </summary>
public static class HttpSessions
{
    /// <summary>Store di processo condiviso (memory di default; backend persistente se configurato).</summary>
    public static BspSessionManager Store => SessionStores.Default;

    /// <summary>Garantisce una sessione per l'header Cookie della richiesta.
    /// Ritorna i dati (JsonObject VIVO nello store) e, se è stata creata una NUOVA sessione,
    /// il Set-Cookie da aggiungere alla risposta (altrimenti null).</summary>
    public static (string? SetCookie, JsonObject Data) Ensure(string? cookieHeader)
    {
        var (_, setCookie, data) = Store.GetOrCreate(cookieHeader);
        return (setCookie, data);
    }
}
