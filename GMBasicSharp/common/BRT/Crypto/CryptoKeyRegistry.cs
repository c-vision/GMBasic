// FILE: BRT.Crypto.CryptoKeyRegistry.cs
// Registry condiviso handle -> chiave asimmetrica BouncyCastle per le funzioni CRYPTOGRAPHY
// (KEYGEN/KEYEXPORT/KEYIMPORT/KEYCLOSE/SIGN/VERIFY/ENCRYPTASYM/DECRYPTASYM). Stesso design di
// IlBackend/IlRuntime.cs (CryptoKeys statico) — promosso qui in BRT/ per essere condiviso da
// qualunque motore (P-CODE VM, IL, futuro Island Bridge) senza duplicare lo stato per istanza.
// Solo il lifecycle dell'handle è stato; la logica di firma/verifica/cifratura resta pura
// (CommonLib.CryptoUtils, chiamata con la chiave già risolta).

using System.Collections.Concurrent;
using Org.BouncyCastle.Crypto;

namespace Basic.Runtime.Crypto;

public static class CryptoKeyRegistry
{
    private static readonly ConcurrentDictionary<long, AsymmetricKeyParameter> Keys = new();
    private static long _nextHandle;

    public static long Store(AsymmetricKeyParameter key)
    {
        var handle = System.Threading.Interlocked.Increment(ref _nextHandle);
        Keys[handle] = key;
        return handle;
    }

    public static AsymmetricKeyParameter? Get(long handle)
        => Keys.TryGetValue(handle, out var key) ? key : null;

    public static void Close(long handle) => Keys.TryRemove(handle, out _);
}
