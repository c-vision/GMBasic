/* FILE: native/gmb_db_c/src/SPEED/ByteArrayEqualityComparer.c */
/* Port 1:1 di modules/Database/SPEED/ByteArrayEqualityComparer.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Speed.ByteArrayEqualityComparer.cs
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//namespace Basic.Databases.Speed;
//
///// <summary>
///// Implementa IEqualityComparer per array di byte, necessario per l'uso di byte[] come
///// chiavi in strutture dati basate su hash come Dictionary e HashSet.
///// Confronta gli array in base al loro contenuto, non al loro riferimento.
///// </summary>
//internal sealed class ByteArrayEqualityComparer : IEqualityComparer<byte[]>
//{
//    /// <summary>
//    /// Determina se due array di byte sono uguali confrontando il loro contenuto.
//    /// </summary>
//    /// <param name="x">Il primo array di byte da confrontare.</param>
//    /// <param name="y">Il secondo array di byte da confrontare.</param>
//    /// <returns>True se gli array sono uguali, altrimenti False.</returns>
//    public bool Equals(byte[]? x, byte[]? y)
//    {
//        if (ReferenceEquals(x, y)) return true;
//        if (x is null || y is null) return false;
//        // Usa il metodo ottimizzato SequenceEqual per il confronto byte per byte.
//        return x.SequenceEqual(y);
//    }
//
//    /// <summary>
//    /// Restituisce un codice hash per un array di byte basato sul suo contenuto.
//    /// </summary>
//    /// <param name="obj">L'array di byte per cui calcolare l'hash code.</param>
//    /// <returns>Un codice hash calcolato per l'array di byte.</returns>
//    public int GetHashCode(byte[] obj)
//    {
//        ArgumentNullException.ThrowIfNull(obj);
//        
//        unchecked // L'overflow è previsto e fa parte dell'algoritmo di hashing.
//        {
//            var hash = 17;
//            foreach (var b in obj)
//            {
//                // Combina l'hash corrente con il byte successivo usando un moltiplicatore primo.
//                hash = hash * 31 + b;
//            }
//            return hash;
//        }
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <pthread.h>

int ByteArrayEqualityComparer_Equals(const uint8_t* x, int xLen, const uint8_t* y, int yLen)
{
    if (x == y) return 1;
    if (!x || !y) return 0;
    if (xLen != yLen) return 0;
    return memcmp(x, y, (size_t)xLen) == 0;
}

int ByteArrayEqualityComparer_GetHashCode(const uint8_t* obj, int len)
{
    int hash = 17, i;
    for (i = 0; i < len; i++) hash = hash * 31 + obj[i];
    return hash;
}

int ByteArrayComparer_Compare(const uint8_t* x, int xLen, const uint8_t* y, int yLen)
{
    int min, i;
    if (x == y) return 0;
    if (!x) return -1;
    if (!y) return 1;
    min = xLen < yLen ? xLen : yLen;
    for (i = 0; i < min; i++) {
        if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    }
    if (xLen == yLen) return 0;
    return xLen < yLen ? -1 : 1;
}
