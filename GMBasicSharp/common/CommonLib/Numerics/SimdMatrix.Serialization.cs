// FILE: StdLib.SimdMatrix.Serialization.cs
// ReSharper disable CheckNamespace

using System.Globalization;
using System.Text;
using System.Text.Json;
// ReSharper disable MemberCanBePrivate.Global

namespace CommonLib;

/// <summary>
/// Classe per la gestione efficiente di matrici dense di double in puro C# e con supporto SIMD.
/// Supporta tutte le principali operazioni algebriche, prodotto con vettori, determinante, inversa e matrice identità.
/// </summary>
/// <remarks>
/// Metodi definiti in questo file:
/// <list type="bullet">
///   <item><see cref="ToCsv(string)"/> — Serializza in CSV</item>
///   <item><see cref="ToJson"/> — Serializza in JSON</item>
///   <item><see cref="SaveCsv(string, string)"/> — Salva in file CSV</item>
///   <item><see cref="SaveJson(string)"/> — Salva in file JSON</item>
///   <item><see cref="LoadCsv(string, string)"/> — Carica da file CSV</item>
///   <item><see cref="LoadJson(string)"/> — Carica da file JSON</item>
/// </list>
/// </remarks>
public partial class SimdMatrix
{
    /// <summary>
    /// Serializza la matrice come CSV (con separatore personalizzabile).
    /// </summary>
    /// <param name="separator">Separatore tra i valori (default ",").</param>
    /// <returns>Stringa in formato CSV.</returns>
    public string ToCsv(string separator = ",")
    {
        var sb = new StringBuilder();
        for (var i = 0; i < Rows; i++)
        {
            for (var j = 0; j < Cols; j++)
            {
                sb.Append(this[i, j].ToString(CultureInfo.InvariantCulture));
                if (j < Cols - 1) sb.Append(separator);
            }
            sb.AppendLine();
        }
        return sb.ToString();
    }

    /// <summary>
    /// Serializza la matrice come JSON (array di array).
    /// </summary>
    /// <returns>Stringa in formato JSON.</returns>
    public string ToJson()
    {
        var arr = new double[Rows][];
        for (var i = 0; i < Rows; i++)
        {
            arr[i] = new double[Cols];
            for (var j = 0; j < Cols; j++)
                arr[i][j] = this[i, j];
        }
        return JsonSerializer.Serialize(arr);
    }

    /// <summary>
    /// Salva la matrice in un file CSV.
    /// </summary>
    /// <param name="path">Percorso del file di destinazione.</param>
    /// <param name="separator">Separatore tra i valori (default ",").</param>
    public void SaveCsv(string path, string separator = ",") =>
        File.WriteAllText(path, ToCsv(separator));

    /// <summary>
    /// Salva la matrice in un file JSON.
    /// </summary>
    /// <param name="path">Percorso del file di destinazione.</param>
    public void SaveJson(string path) =>
        File.WriteAllText(path, ToJson());

    /// <summary>
    /// Carica una matrice da un file CSV.
    /// </summary>
    /// <param name="path">Percorso del file CSV.</param>
    /// <param name="separator">Separatore tra i valori (default ",").</param>
    /// <returns>Nuova matrice caricata dal file.</returns>
    public static SimdMatrix LoadCsv(string path, string separator = ",")
    {
        var lines = File.ReadAllLines(path);
        var rows = lines.Length;
        var cols = lines[0].Split(separator).Length;
        var mat = new SimdMatrix(rows, cols);
        for (var i = 0; i < rows; i++)
        {
            var vals = lines[i].Split(separator);
            for (var j = 0; j < cols; j++)
                mat[i, j] = double.Parse(vals[j], CultureInfo.InvariantCulture);
        }
        return mat;
    }

    /// <summary>
    /// Carica una matrice da un file JSON (formato array di array).
    /// </summary>
    /// <param name="path">Percorso del file JSON.</param>
    /// <returns>Nuova matrice caricata dal file.</returns>
    /// <exception cref="InvalidOperationException">Se il file JSON non contiene una matrice valida.</exception>
    public static SimdMatrix LoadJson(string path)
    {
        var arr = JsonSerializer.Deserialize<double[][]>(File.ReadAllText(path));
        if (arr == null || arr.Length == 0 || arr[0] == null)
            throw new InvalidOperationException("Il file JSON non contiene una matrice valida.");
        var rows = arr.Length;
        var cols = arr[0].Length;
        var mat = new SimdMatrix(rows, cols);
        for (var i = 0; i < rows; i++)
        for (var j = 0; j < cols; j++)
            mat[i, j] = arr[i][j];
        return mat;
    }
}
