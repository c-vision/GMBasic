// FILE: CommonLib.Numerics.MatrixNativeDecomposition.cs
// Decomposizioni matriciali (LU/QR/SVD/EVD/Cholesky) eseguite dal runtime C.
//
// PERCHÉ. Il backend IL/PCode le calcolava con Math.NET: due implementazioni diverse
// (Math.NET nel managed, Jacobi/LAPACK nel runtime C) producevano basi con SEGNI e
// normalizzazioni diversi. Esempio reale (2026-11-19): la `U` della SVD C era scalata per
// colonna (norma = σ) mentre Math.NET restituisce `U` ortonormale, e il fattore chiamato
// "VT" sul runtime C restituiva V (non la trasposta). Nessun test lo vedeva perché nessun
// sample stampava quei fattori: la divergenza era silenziosa.
//
// Qui l'unica fonte è il kernel C (`gmb_matrix_{lu,qr,svd,evd,cholesky}` +
// `gmb_matrix_decomp_get_factor_named_ex`), materializzata una volta in un dizionario di
// fattori: le gambe gestite e quelle C espongono gli stessi valori.

using System;
using System.Collections.Generic;
using MathNet.Numerics.LinearAlgebra;

namespace CommonLib;

/// <summary>Decomposizioni supportate dal ponte nativo.</summary>
public enum NativeDecompKind
{
    Lu,
    Qr,
    Svd,
    Evd,
    Cholesky
}

/// <summary>
/// Fattori di una decomposizione calcolati dal runtime C. I fattori sono materializzati
/// all'atto della creazione (l'handle della decomposizione C viene rilasciato subito):
/// nessun handle C attraversa il confine né resta appeso a un oggetto gestito.
/// </summary>
public sealed class NativeDecomposition
{
    private readonly Dictionary<string, Matrix<double>> _matrices =
        new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, Vector<double>> _vectors =
        new(StringComparer.OrdinalIgnoreCase);

    public void AddMatrix(string name, Matrix<double> value) => _matrices[name] = value;
    public void AddVector(string name, Vector<double> value) => _vectors[name] = value;

    public bool TryMatrix(string name, out Matrix<double> value) => _matrices.TryGetValue(name, out value!);
    public bool TryVector(string name, out Vector<double> value) => _vectors.TryGetValue(name, out value!);

    /// <summary>Nomi dei fattori disponibili (per diagnostica e per il valore a dizionario).</summary>
    public IEnumerable<string> MatrixNames => _matrices.Keys;

    /// <summary>
    /// Calcola la decomposizione richiesta con il kernel C. `null` se la lib runtime non è
    /// caricabile: il chiamante DEVE trattarlo come errore esplicito (niente fallback Math.NET).
    /// </summary>
    public static NativeDecomposition? Create(Matrix<double> a, NativeDecompKind kind)
        => MatrixNativeKernels.TryDecompose(a, kind, out var result) ? result : null;
}
