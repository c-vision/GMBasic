/* FILE: native/gmb_db_c/src/SPEED/Helpers.c */
/* Port 1:1 di modules/Database/SPEED/Helpers.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Speed.Helpers.cs
//
//using System.Collections.Immutable;
//
//namespace Basic.SPEED;
//
//
///// <summary>
///// Fornisce metodi di estensione per le operazioni di aggregazione LINQ,
///// aggiungendo il supporto per sequenze di tipi numerici nullable.
///// </summary>
//internal static class EnumerableExtensions
//{
//    /// <summary>
//    /// Calcola la somma di una sequenza di valori <see cref="decimal"/> nullable,
//    /// ignorando i valori null.
//    /// </summary>
//    /// <param name="source">La sequenza di <see cref="decimal"/> nullable da sommare.</param>
//    /// <returns>La somma dei valori non nulli, o <c>null</c> se la sequenza è vuota o contiene solo valori null.</returns>
//    public static decimal? Sum(this IEnumerable<decimal?> source)
//    {
//        var nonNullValues = source.Where(v => v.HasValue).Select(v => v.GetValueOrDefault()).ToList();
//        return nonNullValues.Count != 0 ? nonNullValues.Sum() : null;
//    }
//
//    /// <summary>
//    /// Calcola la media di una sequenza di valori <see cref="decimal"/> nullable,
//    /// ignorando i valori null.
//    /// </summary>
//    /// <param name="source">La sequenza di <see cref="decimal"/> nullable di cui calcolare la media.</param>
//    /// <returns>La media dei valori non nulli, o <c>null</c> se la sequenza è vuota o contiene solo valori null.</returns>
//    public static decimal? Average(this IEnumerable<decimal?> source)
//    {
//        var values = source.Where(v => v.HasValue).Select(v => v.GetValueOrDefault()).ToList();
//        return values.Count != 0 ? values.Average() : null;
//    }
//}
//
///// <summary>
///// Implementa <see cref="IEqualityComparer{T}"/> per <see cref="ImmutableArray"/>,
///// permettendo di usare array immutabili come chiavi in strutture dati basate su hash come
///// <see cref="Dictionary{TKey, TValue}"/> e <see cref="HashSet{T}"/>.
///// Il confronto e il calcolo dell'hash code si basano sul contenuto dell'array, non sul riferimento.
///// </summary>
///// <typeparam name="T">Il tipo degli elementi nell'array immutabile.</typeparam>
//internal class ImmutableArrayEqualityComparer<T> : IEqualityComparer<ImmutableArray<T?>>
//{
//    /// <summary>
//    /// Un'istanza singleton, thread-safe e riutilizzabile del comparer.
//    /// </summary>
//    public static readonly ImmutableArrayEqualityComparer<T> Instance = new();
//
//    /// <summary>
//    /// Determina se due array immutabili sono uguali confrontando i loro elementi in sequenza.
//    /// </summary>
//    /// <param name="x">Il primo array da confrontare.</param>
//    /// <param name="y">Il secondo array da confrontare.</param>
//    /// <returns><c>true</c> se gli array contengono gli stessi elementi nello stesso ordine; altrimenti, <c>false</c>.</returns>
//    public bool Equals(ImmutableArray<T?> x, ImmutableArray<T?> y) => x.SequenceEqual(y);
//
//    /// <summary>
//    /// Calcola un codice hash per un array immutabile basandosi sul contenuto di tutti i suoi elementi.
//    /// </summary>
//    /// <param name="obj">L'array immutabile per cui calcolare l'hash code.</param>
//    /// <returns>Un codice hash calcolato per l'array.</returns>
//    public int GetHashCode(ImmutableArray<T?> obj)
//    {
//        unchecked // L'overflow è previsto e fa parte dell'algoritmo di hashing.
//        {
//            var hash = 17;
//            foreach (var item in obj)
//            {
//                // Combina l'hash corrente con l'hash dell'elemento successivo usando un moltiplicatore primo.
//                hash = hash * 31 + (item?.GetHashCode() ?? 0);
//            }
//            return hash;
//        }
//    }
//}
//

/* --- C ---
 * Sum/Average/array comparer C# sono LINQ helpers. In C: loop espliciti nei caller.
 * I nomi fill_1to1 `int Foo(void)` non sono in gmb_db_port.h. */
