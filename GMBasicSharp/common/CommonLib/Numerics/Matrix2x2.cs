// ReSharper disable MemberCanBePrivate.Global
// ReSharper disable CheckNamespace

namespace CommonLib;

/// <summary>
/// Rappresenta una matrice densa 2x2 a doppia precisione, ottimizzata per velocità.
/// Offre operazioni di algebra lineare (somma, prodotto, inversa, determinante, trasposta).
/// Include conversioni da e verso <see cref="SimdMatrix"/> per interoperabilità.
/// </summary>
/// <remarks>
/// Campi e metodi:
/// <list type="bullet">
///   <item><see cref="M11"/> — Elemento [0,0]</item>
///   <item><see cref="M12"/> — Elemento [0,1]</item>
///   <item><see cref="M21"/> — Elemento [1,0]</item>
///   <item><see cref="M22"/> — Elemento [1,1]</item>
///   <item><see cref="Matrix2X2(double, double, double, double)"/> — Costruttore</item>
///   <item><see cref="this[int, int]"/> — Accesso per indice</item>
///   <item><see cref="Identity"/> — Matrice identità</item>
///   <item><see cref="operator +(Matrix2X2, Matrix2X2)"/> — Somma</item>
///   <item><see cref="operator -(Matrix2X2, Matrix2X2)"/> — Sottrazione</item>
///   <item><see cref="operator *(Matrix2X2, Matrix2X2)"/> — Prodotto</item>
///   <item><see cref="operator *(Matrix2X2, double)"/> — Prodotto per scalare</item>
///   <item><see cref="Determinant"/> — Determinante</item>
///   <item><see cref="Transpose"/> — Trasposta</item>
///   <item><see cref="Inverse"/> — Inversa</item>
///   <item><see cref="ToSimdMatrix"/> — Conversione a SimdMatrix</item>
///   <item><see cref="FromSimdMatrix(SimdMatrix)"/> — Conversione da SimdMatrix</item>
///   <item><see cref="ToString"/> — Rappresentazione testuale</item>
/// </list>
/// </remarks>
/// <example>
/// // Costruzione
/// var M = new Matrix2x2(1, 2, 3, 4);
/// // Somma
/// var N = M + Matrix2x2.Identity();
/// // Conversione da SimdMatrix
/// var S = new SimdMatrix(new double[] {1,2,3,4}, 2, 2);
/// var M2 = Matrix2x2.FromSimdMatrix(S);
/// // Conversione a SimdMatrix
/// SimdMatrix S2 = M2.ToSimdMatrix();
/// </example>
public struct Matrix2X2
{
    /// <summary>Elemento [0,0]</summary>
    public double M11;
    /// <summary>Elemento [0,1]</summary>
    public double M12;
    /// <summary>Elemento [1,0]</summary>
    public double M21;
    /// <summary>Elemento [1,1]</summary>
    public double M22;

    /// <summary>
    /// Crea una matrice 2x2 con i valori specificati.
    /// </summary>
    /// <param name="m11">Elemento [0,0].</param>
    /// <param name="m12">Elemento [0,1].</param>
    /// <param name="m21">Elemento [1,0].</param>
    /// <param name="m22">Elemento [1,1].</param>
    public Matrix2X2(double m11, double m12, double m21, double m22)
    {
        M11 = m11; M12 = m12;
        M21 = m21; M22 = m22;
    }

    /// <summary>
    /// Accesso agli elementi tramite indici riga e colonna.
    /// </summary>
    /// <param name="r">Indice di riga (0 o 1).</param>
    /// <param name="c">Indice di colonna (0 o 1).</param>
    /// <returns>Valore della cella nella posizione [r,c].</returns>
    /// <exception cref="IndexOutOfRangeException">Se r o c non sono 0 o 1.</exception>
    public double this[int r, int c]
    {
        get
        {
            return (r, c) switch
            {
                (0, 0) => M11,
                (0, 1) => M12,
                (1, 0) => M21,
                (1, 1) => M22,
                _ => throw new IndexOutOfRangeException()
            };
        }
        set
        {
            switch ((r, c))
            {
                case (0, 0): M11 = value; break;
                case (0, 1): M12 = value; break;
                case (1, 0): M21 = value; break;
                case (1, 1): M22 = value; break;
                default: throw new IndexOutOfRangeException();
            }
        }
    }

    /// <summary>
    /// Restituisce la matrice identità 2x2.
    /// </summary>
    /// <returns>Matrice con 1 sulla diagonale principale e 0 altrove.</returns>
    public static Matrix2X2 Identity() => new(1, 0, 0, 1);

    /// <summary>
    /// Somma elemento per elemento di due matrici 2x2.
    /// </summary>
    /// <param name="a">Prima matrice.</param>
    /// <param name="b">Seconda matrice.</param>
    /// <returns>Nuova matrice risultante dalla somma.</returns>
    public static Matrix2X2 operator +(Matrix2X2 a, Matrix2X2 b) =>
        new(a.M11 + b.M11, a.M12 + b.M12, a.M21 + b.M21, a.M22 + b.M22);

    /// <summary>
    /// Sottrazione elemento per elemento di due matrici 2x2.
    /// </summary>
    /// <param name="a">Prima matrice.</param>
    /// <param name="b">Seconda matrice.</param>
    /// <returns>Nuova matrice risultante dalla sottrazione.</returns>
    public static Matrix2X2 operator -(Matrix2X2 a, Matrix2X2 b) =>
        new(a.M11 - b.M11, a.M12 - b.M12, a.M21 - b.M21, a.M22 - b.M22);

    /// <summary>
    /// Prodotto tra due matrici 2x2.
    /// </summary>
    /// <param name="a">Prima matrice.</param>
    /// <param name="b">Seconda matrice.</param>
    /// <returns>Nuova matrice risultante dal prodotto righe × colonne.</returns>
    public static Matrix2X2 operator *(Matrix2X2 a, Matrix2X2 b) =>
        new(
            a.M11 * b.M11 + a.M12 * b.M21, a.M11 * b.M12 + a.M12 * b.M22,
            a.M21 * b.M11 + a.M22 * b.M21, a.M21 * b.M12 + a.M22 * b.M22
        );

    /// <summary>
    /// Prodotto per uno scalare.
    /// </summary>
    /// <param name="a">Matrice da moltiplicare.</param>
    /// <param name="s">Valore scalare.</param>
    /// <returns>Nuova matrice con tutti gli elementi moltiplicati per s.</returns>
    public static Matrix2X2 operator *(Matrix2X2 a, double s) =>
        new(a.M11 * s, a.M12 * s, a.M21 * s, a.M22 * s);

    /// <summary>
    /// Determinante della matrice.
    /// </summary>
    /// <returns>Determinante (M11*M22 - M12*M21).</returns>
    public double Determinant() => M11 * M22 - M12 * M21;

    /// <summary>
    /// Trasposta della matrice.
    /// </summary>
    /// <returns>Nuova matrice con righe e colonne scambiate.</returns>
    public Matrix2X2 Transpose() => new(M11, M21, M12, M22);

    /// <summary>
    /// Inversa della matrice (se non singolare).
    /// </summary>
    /// <returns>Nuova matrice inversa.</returns>
    /// <exception cref="InvalidOperationException">Se il determinante è zero (matrice singolare).</exception>
    public Matrix2X2 Inverse()
    {
        var det = Determinant();
        if (Math.Abs(det) < 1e-12) throw new InvalidOperationException("Singular matrix");
        return new Matrix2X2(M22, -M12, -M21, M11) * (1.0 / det);
    }

    /// <summary>
    /// Conversione verso <see cref="SimdMatrix"/>.
    /// </summary>
    /// <returns>Nuova SimdMatrix 2x2 con i valori della matrice.</returns>
    public SimdMatrix ToSimdMatrix() =>
        new([M11, M12, M21, M22], 2, 2);

    /// <summary>
    /// Conversione da <see cref="SimdMatrix"/> 2x2.
    /// </summary>
    /// <param name="s">Matrice SimdMatrix 2x2.</param>
    /// <returns>Nuova Matrix2X2 con i valori della SimdMatrix.</returns>
    /// <exception cref="ArgumentException">Se la SimdMatrix non è 2x2.</exception>
    public static Matrix2X2 FromSimdMatrix(SimdMatrix s)
    {
        if (s.Rows != 2 || s.Cols != 2)
            throw new ArgumentException("SimdMatrix non è 2x2");
        return new Matrix2X2(s[0, 0], s[0, 1], s[1, 0], s[1, 1]);
    }

    /// <summary>
    /// Rappresentazione testuale della matrice.
    /// </summary>
    /// <returns>Stringa con i valori separati da tabulazione, righe separate da newline.</returns>
    public override string ToString() =>
        $"{M11}\t{M12}\n{M21}\t{M22}";
}
