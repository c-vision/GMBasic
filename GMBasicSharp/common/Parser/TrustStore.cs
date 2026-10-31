// FILE: Interpreter.Parser.TrustStore.cs
// Trust anchor per le firme dei pacchetti: keyring locale (~/.gmbasic/trusted-keys.json).
// Senza questo, la verifica della firma usava la chiave EMBEDDED nel pacchetto stesso
// (prova solo "non manomesso", non "firmato da un'identità fidata").
using System.Text.Json;

namespace Basic;

/// <summary>Politica di verifica della firma applicata a install/restore/update.</summary>
public enum TrustPolicy
{
    Allow,   // verifica la firma se presente; chiavi sconosciute non bloccano (solo avviso)
    Tofu,    // DEFAULT: la prima chiave vista per un pacchetto viene registrata; un CAMBIO di chiave avvisa/blocca
    Strict   // richiede che il pacchetto sia firmato da una chiave GIÀ fidata (--require-signed)
}

/// <summary>Esito della verifica di fiducia di un pacchetto installato.</summary>
public enum TrustResult
{
    Unsigned,   // non firmato
    Trusted,    // firma valida + chiave già fidata
    FirstUse,   // firma valida, chiave registrata ora (TOFU) o accettata (Allow)
    KeyChanged, // la chiave non combacia con quella fidata per questo pacchetto
    Invalid,    // firma crittograficamente non valida
    Untrusted   // firma valida ma chiave non fidata (policy Strict)
}

/// <summary>
/// Keyring locale delle chiavi pubbliche fidate (~/.gmbasic/trusted-keys.json): nome (pacchetto o
/// account) → chiave pubblica (b64 Ed25519 o PEM ECDSA). È il trust anchor delle firme.
/// </summary>
public static class TrustStore
{
    /// <summary>Percorso del keyring (override nei test).</summary>
    public static string StorePath { get; set; } = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".gmbasic", "trusted-keys.json");

    /// <summary>Lettura del keyring — delegata al riferimento C (gmb_pkg_keyring_*).</summary>
    public static Dictionary<string, string> Load()
    {
        var d = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        var n = NativePkg.KeyringCount(StorePath);
        for (var i = 0; i < n; i++)
            if (NativePkg.KeyringItem(StorePath, i, out var name, out var key)) d[name] = key;
        return d;
    }

    public static void Save(IDictionary<string, string> keys)
    {
        var existing = Load();
        foreach (var k in existing.Keys)
            if (!keys.ContainsKey(k)) NativePkg.KeyringRemove(StorePath, k);
        foreach (var kv in keys) NativePkg.KeyringSet(StorePath, kv.Key, kv.Value);
    }

    public static string? Get(string name)
    {
        var v = NativePkg.KeyringGet(StorePath, name);
        return v.Length > 0 ? v : null;
    }

    public static void Add(string name, string publicKey) => NativePkg.KeyringSet(StorePath, name, publicKey.Trim());

    public static bool Remove(string name) => NativePkg.KeyringRemove(StorePath, name);

    /// <summary>Fingerprint SHA256 (16 hex) di una chiave — delegato al C.</summary>
    public static string Fingerprint(string publicKey) => NativePkg.KeyFingerprint(publicKey.Trim());
}
