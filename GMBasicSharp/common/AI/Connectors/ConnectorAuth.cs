// FILE: Basic.Connectors.ConnectorAuth
// OAuth2 + PKCE + secure storage per-connector (spec MCP 2026-07-28, §28-29 del doc SYSTEM.txt).

using System.Text.Json;

namespace Basic.Connectors;

/// <summary>Token OAuth per un connettore (persistito in config/connectors.json come secret, o in Keychain su macOS).</summary>
public sealed record ConnectorOAuthToken(
    string AccessToken,
    string? RefreshToken = null,
    string? TokenType = "Bearer",
    int ExpiresIn = 3600,
    string? Scope = null,
    DateTimeOffset? ObtainedAt = null)
{
    public bool IsExpired => ObtainedAt.HasValue && DateTimeOffset.Now > ObtainedAt.Value.AddSeconds(ExpiresIn - 60);
    public string EffectiveTokenType => TokenType ?? "Bearer";
}

/// <summary>Config OAuth per un connettore (authorization server discovery, PKCE).</summary>
public sealed record ConnectorOAuthConfig(
    string ClientId,
    string? ClientSecret = null,
    string? AuthorizeEndpoint = null,
    string? TokenEndpoint = null,
    string? Issuer = null,
    string? RedirectUri = null,
    string? Scope = null,
    bool UsePkce = true);

/// <summary>Manager auth per connettore — genera PKCE, auth URL, scambia code→token, refresh, storage.</summary>
public static class ConnectorAuth
{
    private static readonly JsonSerializerOptions JsonOpts = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
    };

    /// <summary>Crea verifier PKCE (RFC 7636, 43-128 char).</summary>
    public static string CreateVerifier(int length = 64)
    {
        if (length < 43 || length > 128) length = 64;
        const string charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
        var bytes = System.Security.Cryptography.RandomNumberGenerator.GetBytes(length);
        var sb = new System.Text.StringBuilder(length);
        for (var i = 0; i < length; i++) sb.Append(charset[bytes[i] % 64]);
        return sb.ToString();
    }

    /// <summary>Code challenge S256 = base64url(SHA256(verifier)).</summary>
    public static string CreateChallenge(string verifier)
    {
        var hash = System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.ASCII.GetBytes(verifier));
        return Base64UrlEncode(hash);
    }

    /// <summary>Costruisce l'authorization URL (Auth Code + PKCE, opzionale).</summary>
    public static string BuildAuthUrl(ConnectorOAuthConfig cfg, string state, string verifier)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(cfg.AuthorizeEndpoint);
        ArgumentException.ThrowIfNullOrWhiteSpace(cfg.ClientId);
        var url = cfg.AuthorizeEndpoint + "?response_type=" + Uri.EscapeDataString("code")
            + "&client_id=" + Uri.EscapeDataString(cfg.ClientId);
        if (!string.IsNullOrWhiteSpace(cfg.RedirectUri))
            url += "&redirect_uri=" + Uri.EscapeDataString(cfg.RedirectUri);
        if (!string.IsNullOrWhiteSpace(cfg.Scope))
            url += "&scope=" + Uri.EscapeDataString(cfg.Scope);
        url += "&state=" + Uri.EscapeDataString(state);
        if (cfg.UsePkce && !string.IsNullOrWhiteSpace(verifier))
        {
            url += "&code_challenge=" + Uri.EscapeDataString(CreateChallenge(verifier));
            url += "&code_challenge_method=S256";
        }
        return url;
    }

    /// <summary>Scambia authorization_code o refresh_token → token JSON (POST form-urlencoded).</summary>
    public static async Task<string> RequestTokenAsync(string tokenEndpoint, string body, CancellationToken ct = default)
    {
        var helper = new CommonLib.Http.HttpClientHelper();
        var opts = new CommonLib.Http.HttpOptions { Method = "POST", Body = body };
        opts.Headers.SetHeader("Content-Type", "application/x-www-form-urlencoded");
        var result = await helper.PerformRequestAsync(tokenEndpoint, opts, ct).ConfigureAwait(false);
        if (!string.IsNullOrEmpty(result.ErrorMessage))
            throw new InvalidOperationException($"OAuth token request failed: {result.ErrorMessage}");
        return result.Body ?? "{}";
    }

    /// <summary>Parsa il JSON del token endpoint → ConnectorOAuthToken.</summary>
    public static ConnectorOAuthToken ParseTokenJson(string json)
    {
        using var doc = JsonDocument.Parse(json);
        var root = doc.RootElement;
        var access = root.TryGetProperty("access_token", out var at) ? at.GetString() ?? "" : "";
        var refresh = root.TryGetProperty("refresh_token", out var rt) ? rt.GetString() : null;
        var type = root.TryGetProperty("token_type", out var tt) ? tt.GetString() : "Bearer";
        var expires = root.TryGetProperty("expires_in", out var ei) && ei.TryGetInt32(out var e) ? e : 3600;
        var scope = root.TryGetProperty("scope", out var sc) ? sc.GetString() : null;
        return new ConnectorOAuthToken(access, refresh, type, expires, scope, DateTimeOffset.Now);
    }

    /// <summary>Salva il token come secret del connettore (persistito in config/connectors.json).</summary>
    public static void StoreToken(string connectorName, ConnectorOAuthToken token)
    {
        var json = JsonSerializer.Serialize(token, JsonOpts);
        ConnectorRegistry.Instance.SetSecret(connectorName, "oauth_token", json);
        ConnectorRegistry.Instance.SetSecret(connectorName, "access_token", token.AccessToken);
        if (token.RefreshToken is not null)
            ConnectorRegistry.Instance.SetSecret(connectorName, "refresh_token", token.RefreshToken);
    }

    /// <summary>Carica il token dal connettore (se presente e non scaduto).</summary>
    public static ConnectorOAuthToken? LoadToken(string connectorName)
    {
        var json = ConnectorRegistry.Instance.GetSecret(connectorName, "oauth_token");
        if (string.IsNullOrWhiteSpace(json)) return null;
        try { return JsonSerializer.Deserialize<ConnectorOAuthToken>(json, JsonOpts); }
        catch { return null; }
    }

    /// <summary>Ritorna l'header Authorization per il connettore (Bearer <access_token> o ApiKey).</summary>
    public static string? GetAuthorizationHeader(string connectorName)
    {
        var token = LoadToken(connectorName);
        if (token is not null && !string.IsNullOrWhiteSpace(token.AccessToken) && !token.IsExpired)
            return $"{token.EffectiveTokenType} {token.AccessToken}";
        var apiKey = ConnectorRegistry.Instance.GetSecret(connectorName, "api_key")
            ?? ConnectorRegistry.Instance.GetSecret(connectorName, "apikey");
        if (!string.IsNullOrWhiteSpace(apiKey)) return $"Bearer {apiKey}";
        var direct = ConnectorRegistry.Instance.GetSecret(connectorName, "access_token");
        if (!string.IsNullOrWhiteSpace(direct)) return $"Bearer {direct}";
        return null;
    }

    /// <summary>Refresh automatico se il token è scaduto (best-effort, ritorna header aggiornato o null).</summary>
    public static async Task<string?> EnsureFreshTokenAsync(string connectorName, ConnectorOAuthConfig cfg, CancellationToken ct = default)
    {
        var token = LoadToken(connectorName);
        if (token is null || !token.IsExpired) return GetAuthorizationHeader(connectorName);
        if (string.IsNullOrWhiteSpace(token.RefreshToken) || string.IsNullOrWhiteSpace(cfg.TokenEndpoint))
            return GetAuthorizationHeader(connectorName);
        var body = $"grant_type=refresh_token&client_id={Uri.EscapeDataString(cfg.ClientId)}"
            + (string.IsNullOrWhiteSpace(cfg.ClientSecret) ? "" : $"&client_secret={Uri.EscapeDataString(cfg.ClientSecret)}")
            + $"&refresh_token={Uri.EscapeDataString(token.RefreshToken)}"
            + (string.IsNullOrWhiteSpace(cfg.Scope) ? "" : $"&scope={Uri.EscapeDataString(cfg.Scope)}");
        var json = await RequestTokenAsync(cfg.TokenEndpoint, body, ct).ConfigureAwait(false);
        var fresh = ParseTokenJson(json);
        StoreToken(connectorName, fresh);
        return $"{fresh.EffectiveTokenType} {fresh.AccessToken}";
    }

    private static string Base64UrlEncode(byte[] data) =>
        Convert.ToBase64String(data).TrimEnd('=').Replace('+', '-').Replace('/', '_');
}
