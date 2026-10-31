// FILE: Basic.Agents.ACPProtocol
// B5.2 — ACP (Agent Communication Protocol): SEND/RECEIVE di un AgentMessage verso un agente
// remoto servito da un altro AI AGENT SERVE (endpoint HTTP, porta diversa). Trasporto via
// POST {baseUrl}/execute {"agent","prompt"} (route A5) — il server remoto fa AgentServer.Ask.
// Mai stub: errori espliciti; senza baseUrl raggiungibile → eccezione.

using System.Net.Http;
using System.Text;

namespace Basic.Agents;

public static class AcpProtocol
{
    private static readonly HttpClient Http = new() { Timeout = TimeSpan.FromSeconds(60) };

    /// <summary>Invia un messaggio agente→agente remoto: POST {baseUrl}/execute con
    /// {"agent": toAgent, "prompt": task+"\n"+input}. Ritorna la risposta dell'agente remoto.</summary>
    public static string Send(string fromAgent, string toAgent, string baseUrl, string task, string input)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(fromAgent);
        ArgumentException.ThrowIfNullOrWhiteSpace(toAgent);
        ArgumentException.ThrowIfNullOrWhiteSpace(baseUrl);
        ArgumentException.ThrowIfNullOrWhiteSpace(task);

        var msg = AgentMessage.Create(fromAgent, toAgent, task, input);
        var payload = "{\"agent\":\"" + toAgent + "\",\"prompt\":\"" + Escape(msg.Task) + "\\n" + Escape(msg.Input) + "\"}";
        var url = baseUrl.TrimEnd('/') + "/execute";
        var content = new StringContent(payload, Encoding.UTF8, "application/json");
        try
        {
            var resp = Http.PostAsync(url, content).GetAwaiter().GetResult();
            var body = resp.Content.ReadAsStringAsync().GetAwaiter().GetResult();
            if (!resp.IsSuccessStatusCode)
                throw new InvalidOperationException($"ACP Send fallito HTTP {(int)resp.StatusCode}: {body}");
            return body;
        }
        catch (HttpRequestException ex)
        {
            throw new InvalidOperationException($"ACP Send: endpoint remoto non raggiungibile ({baseUrl}): {ex.Message}");
        }
    }

    /// <summary>Ping: GET {baseUrl}/api/agents (200 → remoto vivo). Ritorna true se risponde.</summary>
    public static bool Ping(string baseUrl)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(baseUrl);
        try
        {
            var resp = Http.GetAsync(baseUrl.TrimEnd('/') + "/api/agents").GetAwaiter().GetResult();
            return resp.IsSuccessStatusCode;
        }
        catch { return false; }
    }

    private static string Escape(string s) => (s ?? "").Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("\n", "\\n").Replace("\r", "\\r");
}
