// ReSharper disable MemberCanBePrivate.Global
// ReSharper disable CheckNamespace

namespace CommonLib;

/// <summary>
/// Matrice densa 3x3 a doppia precisione, ottimizzata per calcoli 2D/3D.
/// Offre operazioni di algebra lineare: somma, prodotto, trasposta, determinante, inversa.
/// Conversioni da/verso <see cref="SimdMatrix"/> incluse.
/// </summary>
/// <remarks>
/// Campi e metodi:
/// <list type="bullet">
///   <item><see cref="M11"/>…<see cref="M33"/> — Elementi della matrice</item>
///   <item><see cref="Matrix3X3(double, double, double, double, double, double, double, double, double)"/> — Costruttore</item>
///   <item><see cref="this[int,int]"/> — Accesso per indice</item>
///   <item><see cref="Identity"/> — Matrice identità</item>
///   <item><see cref="operator +(Matrix3X3, Matrix3X3)"/> — Somma</item>
///   <item><see cref="operator -(Matrix3X3, Matrix3X3)"/> — Sottrazione</item>
///   <item><see cref="operator *(Matrix3X3, Matrix3X3)"/> — Prodotto</item>
///   <item><see cref="operator *(Matrix3X3, double)"/> — Prodotto per scalare</item>
///   <item><see cref="Determinant"/> — Determinante</item>
///   <item><see cref="Transpose"/> — Trasposta</item>
///   <item><see cref="Inverse"/> — Inversa</item>
///   <item><see cref="ToSimdMatrix"/> — Conversione a SimdMatrix</item>
///   <item><see cref="FromSimdMatrix(SimdMatrix)"/> — Conversione da SimdMatrix</item>
///   <item><see cref="ToString"/> — Rappresentazione testuale</item>
/// </list>
/// </remarks>
/// <example>
/// var M = new Matrix3x3(1,2,3,4,5,6,7,8,9);
/// var N = Matrix3x3.Identity();
/// var S = new SimdMatrix(new double[] {1,2,3,4,5,6,7,8,9}, 3, 3);
/// var M2 = Matrix3x3.FromSimdMatrix(S);
/// SimdMatrix S2 = M2.ToSimdMatrix();
/// </example>
public struct Matrix3X3
{
    /// <summary>Elemento [0,0]</summary>
    public double M11;

    /// <summary>Elemento [0,1]</summary>
    public double M12;

    /// <summary>Elemento [0,2]</summary>
    public double M13;

    /// <summary>Elemento [1,0]</summary>
    public double M21;

    /// <summary>Elemento [1,1]</summary>
    public double M22;

    /// <summary>Elemento [1,2]</summary>
    public double M23;

    /// <summary>Elemento [2,0]</summary>
    public double M31;

    /// <summary>Elemento [2,1]</summary>
    public double M32;

    /// <summary>Elemento [2,2]</summary>
    public double M33;

    /// <summary>
    /// Crea una matrice 3x3 con i valori specificati.
    /// </summary>
    /// <param name="m11">Elemento [0,0].</param>
    /// <param name="m12">Elemento [0,1].</param>
    /// <param name="m13">Elemento [0,2].</param>
    /// <param name="m21">Elemento [1,0].</param>
    /// <param name="m22">Elemento [1,1].</param>
    /// <param name="m23">Elemento [1,2].</param>
    /// <param name="m31">Elemento [2,0].</param>
    /// <param name="m32">Elemento [2,1].</param>
    /// <param name="m33">Elemento [2,2].</param>
    public Matrix3X3(
        double m11, double m12, double m13,
        double m21, double m22, double m23,
        double m31, double m32, double m33)
    {
        M11 = m11;
        M12 = m12;
        M13 = m13;
        M21 = m21;
        M22 = m22;
        M23 = m23;
        M31 = m31;
        M32 = m32;
        M33 = m33;
    }

    /// <summary>
    /// Accesso agli elementi tramite indici riga e colonna.
    /// </summary>
    /// <param name="r">Indice di riga (0-2).</param>
    /// <param name="c">Indice di colonna (0-2).</param>
    /// <returns>Valore della cella nella posizione [r,c].</returns>
    /// <exception cref="IndexOutOfRangeException">Se r o c non sono compresi tra 0 e 2.</exception>
    public double this[int r, int c]
    {
        get
        {
            return (r, c) switch
            {
                (0, 0) => M11, (0, 1) => M12, (0, 2) => M13,
                (1, 0) => M21, (1, 1) => M22, (1, 2) => M23,
                (2, 0) => M31, (2, 1) => M32, (2, 2) => M33,
                _ => throw new IndexOutOfRangeException()
            };
        }
        set
        {
            switch ((r, c))
            {
                case (0, 0): M11 = value; break;
                case (0, 1): M12 = value; break;
                case (0, 2): M13 = value; break;
                case (1, 0): M21 = value; break;
                case (1, 1): M22 = value; break;
                case (1, 2): M23 = value; break;
                case (2, 0): M31 = value; break;
                case (2, 1): M32 = value; break;
                case (2, 2): M33 = value; break;
                default: throw new IndexOutOfRangeException();
            }
        }
    }

    /// <summary>
    /// Matrice identità 3x3.
    /// </summary>
    /// <returns>Matrice con 1 sulla diagonale principale e 0 altrove.</returns>
    public static Matrix3X3 Identity() => new(
        1, 0, 0,
        0, 1, 0,
        0, 0, 1);

    /// <summary>
    /// Somma elemento per elemento di due matrici 3x3.
    /// </summary>
    /// <param name="a">Prima matrice.</param>
    /// <param name="b">Seconda matrice.</param>
    /// <returns>Nuova matrice risultante dalla somma.</returns>
    public static Matrix3X3 operator +(Matrix3X3 a, Matrix3X3 b) =>
        new(
            a.M11 + b.M11, a.M12 + b.M12, a.M13 + b.M13,
            a.M21 + b.M21, a.M22 + b.M22, a.M23 + b.M23,
            a.M31 + b.M31, a.M32 + b.M32, a.M33 + b.M33);

    /// <summary>
    /// Sottrazione elemento per elemento di due matrici 3x3.
    /// </summary>
    /// <param name="a">Prima matrice.</param>
    /// <param name="b">Seconda matrice.</param>
    /// <returns>Nuova matrice risultante dalla sottrazione.</returns>
    public static Matrix3X3 operator -(Matrix3X3 a, Matrix3X3 b) =>
        new(
            a.M11 - b.M11, a.M12 - b.M12, a.M13 - b.M13,
            a.M21 - b.M21, a.M22 - b.M22, a.M23 - b.M23,
            a.M31 - b.M31, a.M32 - b.M32, a.M33 - b.M33);

    /// <summary>
    /// Prodotto tra due matrici 3x3.
    /// </summary>
    /// <param name="a">Prima matrice.</param>
    /// <param name="b">Seconda matrice.</param>
    /// <returns>Nuova matrice risultante dal prodotto righe × colonne.</returns>
    public static Matrix3X3 operator *(Matrix3X3 a, Matrix3X3 b)
    {
        return new Matrix3X3(
            a.M11 * b.M11 + a.M12 * b.M21 + a.M13 * b.M31,
            a.M11 * b.M12 + a.M12 * b.M22 + a.M13 * b.M32,
            a.M11 * b.M13 + a.M12 * b.M23 + a.M13 * b.M33,
            a.M21 * b.M11 + a.M22 * b.M21 + a.M23 * b.M31,
            a.M21 * b.M12 + a.M22 * b.M22 + a.M23 * b.M32,
            a.M21 * b.M13 + a.M22 * b.M23 + a.M23 * b.M33,
            a.M31 * b.M11 + a.M32 * b.M21 + a.M33 * b.M31,
            a.M31 * b.M12 + a.M32 * b.M22 + a.M33 * b.M32,
            a.M31 * b.M13 + a.M32 * b.M23 + a.M33 * b.M33
        );
    }

    /// <summary>
    /// Prodotto per uno scalare.
    /// </summary>
    /// <param name="a">Matrice da moltiplicare.</param>
    /// <param name="s">Valore scalare.</param>
    /// <returns>Nuova matrice con tutti gli elementi moltiplicati per s.</returns>
    public static Matrix3X3 operator *(Matrix3X3 a, double s) =>
        new(
            a.M11 * s, a.M12 * s, a.M13 * s,
            a.M21 * s, a.M22 * s, a.M23 * s,
            a.M31 * s, a.M32 * s, a.M33 * s);

    /// <summary>
    /// Determinante della matrice 3x3.
    /// </summary>
    /// <returns>Determinante calcolato tramite regola di Sarrus.</returns>
    public double Determinant() =>
        M11 * (M22 * M33 - M23 * M32) -
        M12 * (M21 * M33 - M23 * M31) +
        M13 * (M21 * M32 - M22 * M31);

    /// <summary>
    /// Trasposta della matrice 3x3.
    /// </summary>
    /// <returns>Nuova matrice con righe e colonne scambiate.</returns>
    public Matrix3X3 Transpose() => new(
        M11, M21, M31,
        M12, M22, M32,
        M13, M23, M33);

    /// <summary>
    /// Inversa della matrice 3x3 (se non singolare).
    /// </summary>
    /// <returns>Nuova matrice inversa calcolata tramite matrice dei cofattori.</returns>
    /// <exception cref="InvalidOperationException">Se il determinante è zero (matrice singolare).</exception>
    public Matrix3X3 Inverse()
    {
        var det = Determinant();
        if (Math.Abs(det) < 1e-12) throw new InvalidOperationException("Singular matrix");

        return new Matrix3X3(
            (M22 * M33 - M23 * M32) / det,
            (M13 * M32 - M12 * M33) / det,
            (M12 * M23 - M13 * M22) / det,
            (M23 * M31 - M21 * M33) / det,
            (M11 * M33 - M13 * M31) / det,
            (M13 * M21 - M11 * M23) / det,
            (M21 * M32 - M22 * M31) / det,
            (M12 * M31 - M11 * M32) / det,
            (M11 * M22 - M12 * M21) / det
        );
    }

    /// <summary>
    /// Conversione verso <see cref="SimdMatrix"/>.
    /// </summary>
    /// <returns>Nuova SimdMatrix 3x3 con i valori della matrice.</returns>
    public SimdMatrix ToSimdMatrix() =>
        new([
            M11, M12, M13,
            M21, M22, M23,
            M31, M32, M33
        ], 3, 3);

    /// <summary>
    /// Conversione da <see cref="SimdMatrix"/> 3x3.
    /// </summary>
    /// <param name="s">Matrice SimdMatrix 3x3.</param>
    /// <returns>Nuova Matrix3X3 con i valori della SimdMatrix.</returns>
    /// <exception cref="ArgumentException">Se la SimdMatrix non è 3x3.</exception>
    public static Matrix3X3 FromSimdMatrix(SimdMatrix s)
    {
        if (s.Rows != 3 || s.Cols != 3)
            throw new ArgumentException("SimdMatrix non è 3x3");
        return new Matrix3X3(
            s[0, 0], s[0, 1], s[0, 2],
            s[1, 0], s[1, 1], s[1, 2],
            s[2, 0], s[2, 1], s[2, 2]
        );
    }

    /// <summary>
    /// Rappresentazione testuale della matrice.
    /// </summary>
    /// <returns>Stringa con i valori separati da tabulazione, righe separate da newline.</returns>
    public override string ToString() =>
        $"{M11}\t{M12}\t{M13}\n{M21}\t{M22}\t{M23}\n{M31}\t{M32}\t{M33}";
}
