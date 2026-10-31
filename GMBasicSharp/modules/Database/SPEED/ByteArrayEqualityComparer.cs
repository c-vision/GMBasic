// FILE: Basic.Databases.Speed.ByteArrayEqualityComparer.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

namespace Basic.Databases.Speed;

/// <summary>
/// Implementa IEqualityComparer per array di byte, necessario per l'uso di byte[] come
/// chiavi in strutture dati basate su hash come Dictionary e HashSet.
/// Confronta gli array in base al loro contenuto, non al loro riferimento.
/// </summary>
internal sealed class ByteArrayEqualityComparer : IEqualityComparer<byte[]>
{
    /// <summary>
    /// Determina se due array di byte sono uguali confrontando il loro contenuto.
    /// </summary>
    /// <param name="x">Il primo array di byte da confrontare.</param>
    /// <param name="y">Il secondo array di byte da confrontare.</param>
    /// <returns>True se gli array sono uguali, altrimenti False.</returns>
    public bool Equals(byte[]? x, byte[]? y)
    {
        if (ReferenceEquals(x, y)) return true;
        if (x is null || y is null) return false;
        // Usa il metodo ottimizzato SequenceEqual per il confronto byte per byte.
        return x.SequenceEqual(y);
    }

    /// <summary>
    /// Restituisce un codice hash per un array di byte basato sul suo contenuto.
    /// </summary>
    /// <param name="obj">L'array di byte per cui calcolare l'hash code.</param>
    /// <returns>Un codice hash calcolato per l'array di byte.</returns>
    public int GetHashCode(byte[] obj)
    {
        ArgumentNullException.ThrowIfNull(obj);
        
        unchecked // L'overflow è previsto e fa parte dell'algoritmo di hashing.
        {
            var hash = 17;
            foreach (var b in obj)
            {
                // Combina l'hash corrente con il byte successivo usando un moltiplicatore primo.
                hash = hash * 31 + b;
            }
            return hash;
        }
    }
}