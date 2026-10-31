// ReSharper disable MemberCanBePrivate.Global
// ReSharper disable CheckNamespace

namespace CommonLib;

/// <summary>
/// Matrice densa 4x4 a doppia precisione, ideale per grafica/simulazioni.
/// Offre operazioni di algebra lineare: somma, prodotto, trasposta, determinante, inversa.
/// Conversioni da/verso <see cref="SimdMatrix"/> incluse.
/// </summary>
/// <remarks>
/// Campi e metodi:
/// <list type="bullet">
///   <item><see cref="M11"/>…<see cref="M44"/> — Elementi della matrice</item>
///   <item><see cref="Matrix4X4(double, double, double, double, double, double, double, double, double, double, double, double, double, double, double, double)"/> — Costruttore</item>
///   <item><see cref="this[int,int]"/> — Accesso per indice</item>
///   <item><see cref="Identity"/> — Matrice identità</item>
///   <item><see cref="operator +(Matrix4X4, Matrix4X4)"/> — Somma</item>
///   <item><see cref="operator -(Matrix4X4, Matrix4X4)"/> — Sottrazione</item>
///   <item><see cref="operator *(Matrix4X4, Matrix4X4)"/> — Prodotto</item>
///   <item><see cref="operator *(Matrix4X4, double)"/> — Prodotto per scalare</item>
///   <item><see cref="Determinant"/> — Determinante</item>
///   <item><see cref="Minor(int, int)"/> — Minore 3x3</item>
///   <item><see cref="Transpose"/> — Trasposta</item>
///   <item><see cref="Inverse"/> — Inversa</item>
///   <item><see cref="ToSimdMatrix"/> — Conversione a SimdMatrix</item>
///   <item><see cref="FromSimdMatrix(SimdMatrix)"/> — Conversione da SimdMatrix</item>
///   <item><see cref="ToString"/> — Rappresentazione testuale</item>
/// </list>
/// </remarks>
/// <example>
/// var M = new Matrix4x4( ... ); // 16 valori
/// var I = Matrix4x4.Identity();
/// var S = new SimdMatrix(new double[] { ... }, 4, 4);
/// var M2 = Matrix4x4.FromSimdMatrix(S);
/// SimdMatrix S2 = M2.ToSimdMatrix();
/// </example>
public struct Matrix4X4
{
    /// <summary>Elemento [0,0]</summary>
    public double M11;
    /// <summary>Elemento [0,1]</summary>
    public double M12;
    /// <summary>Elemento [0,2]</summary>
    public double M13;
    /// <summary>Elemento [0,3]</summary>
    public double M14;
    /// <summary>Elemento [1,0]</summary>
    public double M21;
    /// <summary>Elemento [1,1]</summary>
    public double M22;
    /// <summary>Elemento [1,2]</summary>
    public double M23;
    /// <summary>Elemento [1,3]</summary>
    public double M24;
    /// <summary>Elemento [2,0]</summary>
    public double M31;
    /// <summary>Elemento [2,1]</summary>
    public double M32;
    /// <summary>Elemento [2,2]</summary>
    public double M33;
    /// <summary>Elemento [2,3]</summary>
    public double M34;
    /// <summary>Elemento [3,0]</summary>
    public double M41;
    /// <summary>Elemento [3,1]</summary>
    public double M42;
    /// <summary>Elemento [3,2]</summary>
    public double M43;
    /// <summary>Elemento [3,3]</summary>
    public double M44;

    /// <summary>
    /// Crea una matrice 4x4 con i valori specificati (row-major).
    /// </summary>
    /// <param name="m11">Elemento [0,0].</param>
    /// <param name="m12">Elemento [0,1].</param>
    /// <param name="m13">Elemento [0,2].</param>
    /// <param name="m14">Elemento [0,3].</param>
    /// <param name="m21">Elemento [1,0].</param>
    /// <param name="m22">Elemento [1,1].</param>
    /// <param name="m23">Elemento [1,2].</param>
    /// <param name="m24">Elemento [1,3].</param>
    /// <param name="m31">Elemento [2,0].</param>
    /// <param name="m32">Elemento [2,1].</param>
    /// <param name="m33">Elemento [2,2].</param>
    /// <param name="m34">Elemento [2,3].</param>
    /// <param name="m41">Elemento [3,0].</param>
    /// <param name="m42">Elemento [3,1].</param>
    /// <param name="m43">Elemento [3,2].</param>
    /// <param name="m44">Elemento [3,3].</param>
    public Matrix4X4(
        double m11, double m12, double m13, double m14,
        double m21, double m22, double m23, double m24,
        double m31, double m32, double m33, double m34,
        double m41, double m42, double m43, double m44)
    {
        M11 = m11; M12 = m12; M13 = m13; M14 = m14;
        M21 = m21; M22 = m22; M23 = m23; M24 = m24;
        M31 = m31; M32 = m32; M33 = m33; M34 = m34;
        M41 = m41; M42 = m42; M43 = m43; M44 = m44;
    }

    /// <summary>
    /// Accesso agli elementi tramite indici riga e colonna.
    /// </summary>
    /// <param name="r">Indice di riga (0-3).</param>
    /// <param name="c">Indice di colonna (0-3).</param>
    /// <returns>Valore della cella nella posizione [r,c].</returns>
    /// <exception cref="IndexOutOfRangeException">Se r o c non sono compresi tra 0 e 3.</exception>
    public double this[int r, int c]
    {
        get
        {
            return (r, c) switch
            {
                (0, 0) => M11, (0, 1) => M12, (0, 2) => M13, (0, 3) => M14,
                (1, 0) => M21, (1, 1) => M22, (1, 2) => M23, (1, 3) => M24,
                (2, 0) => M31, (2, 1) => M32, (2, 2) => M33, (2, 3) => M34,
                (3, 0) => M41, (3, 1) => M42, (3, 2) => M43, (3, 3) => M44,
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
                case (0, 3): M14 = value; break;
                case (1, 0): M21 = value; break;
                case (1, 1): M22 = value; break;
                case (1, 2): M23 = value; break;
                case (1, 3): M24 = value; break;
                case (2, 0): M31 = value; break;
                case (2, 1): M32 = value; break;
                case (2, 2): M33 = value; break;
                case (2, 3): M34 = value; break;
                case (3, 0): M41 = value; break;
                case (3, 1): M42 = value; break;
                case (3, 2): M43 = value; break;
                case (3, 3): M44 = value; break;
                default: throw new IndexOutOfRangeException();
            }
        }
    }

    /// <summary>
    /// Matrice identità 4x4.
    /// </summary>
    /// <returns>Matrice con 1 sulla diagonale principale e 0 altrove.</returns>
    public static Matrix4X4 Identity() => new(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1);

    /// <summary>
    /// Somma elemento per elemento di due matrici 4x4.
    /// </summary>
    /// <param name="a">Prima matrice.</param>
    /// <param name="b">Seconda matrice.</param>
    /// <returns>Nuova matrice risultante dalla somma.</returns>
    public static Matrix4X4 operator +(Matrix4X4 a, Matrix4X4 b) =>
        new(
            a.M11 + b.M11, a.M12 + b.M12, a.M13 + b.M13, a.M14 + b.M14,
            a.M21 + b.M21, a.M22 + b.M22, a.M23 + b.M23, a.M24 + b.M24,
            a.M31 + b.M31, a.M32 + b.M32, a.M33 + b.M33, a.M34 + b.M34,
            a.M41 + b.M41, a.M42 + b.M42, a.M43 + b.M43, a.M44 + b.M44);

    /// <summary>
    /// Sottrazione elemento per elemento di due matrici 4x4.
    /// </summary>
    /// <param name="a">Prima matrice.</param>
    /// <param name="b">Seconda matrice.</param>
    /// <returns>Nuova matrice risultante dalla sottrazione.</returns>
    public static Matrix4X4 operator -(Matrix4X4 a, Matrix4X4 b) =>
        new(
            a.M11 - b.M11, a.M12 - b.M12, a.M13 - b.M13, a.M14 - b.M14,
            a.M21 - b.M21, a.M22 - b.M22, a.M23 - b.M23, a.M24 - b.M24,
            a.M31 - b.M31, a.M32 - b.M32, a.M33 - b.M33, a.M34 - b.M34,
            a.M41 - b.M41, a.M42 - b.M42, a.M43 - b.M43, a.M44 - b.M44);

    /// <summary>
    /// Prodotto tra due matrici 4x4.
    /// </summary>
    /// <param name="a">Prima matrice.</param>
    /// <param name="b">Seconda matrice.</param>
    /// <returns>Nuova matrice risultante dal prodotto righe × colonne.</returns>
    public static Matrix4X4 operator *(Matrix4X4 a, Matrix4X4 b)
    {
        var r = new Matrix4X4();
        for (var i = 0; i < 4; i++)
        for (var j = 0; j < 4; j++)
            for (var k = 0; k < 4; k++)
                r[i, j] += a[i, k] * b[k, j];
        return r;
    }

    /// <summary>
    /// Prodotto per uno scalare.
    /// </summary>
    /// <param name="a">Matrice da moltiplicare.</param>
    /// <param name="s">Valore scalare.</param>
    /// <returns>Nuova matrice con tutti gli elementi moltiplicati per s.</returns>
    public static Matrix4X4 operator *(Matrix4X4 a, double s) =>
        new(
            a.M11 * s, a.M12 * s, a.M13 * s, a.M14 * s,
            a.M21 * s, a.M22 * s, a.M23 * s, a.M24 * s,
            a.M31 * s, a.M32 * s, a.M33 * s, a.M34 * s,
            a.M41 * s, a.M42 * s, a.M43 * s, a.M44 * s);

    /// <summary>
    /// Determinante della matrice 4x4 calcolato per espansione di Laplace sulla prima riga.
    /// </summary>
    /// <returns>Determinante della matrice.</returns>
    public double Determinant()
    {
        // Espansione su prima riga
        double det = 0;
        for (var i = 0; i < 4; i++)
            det += (i % 2 == 0 ? 1 : -1) * this[0, i] * Minor(0, i).Determinant();
        return det;
    }

    /// <summary>
    /// Minore 3x3 escludendo riga row e colonna col.
    /// </summary>
    /// <param name="row">Indice di riga da escludere.</param>
    /// <param name="col">Indice di colonna da escludere.</param>
    /// <returns>Matrice 3x3 ottenuta rimuovendo la riga e la colonna specificate.</returns>
    private Matrix3X3 Minor(int row, int col)
    {
        var vals = new double[9];
        var idx = 0;
        for (var i = 0; i < 4; i++)
            for (var j = 0; j < 4; j++)
                if (i != row && j != col)
                    vals[idx++] = this[i, j];
        return new Matrix3X3(
            vals[0], vals[1], vals[2],
            vals[3], vals[4], vals[5],
            vals[6], vals[7], vals[8]);
    }

    /// <summary>
    /// Trasposta della matrice.
    /// </summary>
    /// <returns>Nuova matrice con righe e colonne scambiate.</returns>
    public Matrix4X4 Transpose()
    {
        var r = new Matrix4X4();
        for (var i = 0; i < 4; i++)
        for (var j = 0; j < 4; j++)
            r[i, j] = this[j, i];
        return r;
    }

    /// <summary>
    /// Inversa della matrice 4x4 (se non singolare).
    /// Calcolata tramite matrice dei cofattori divisa per il determinante.
    /// </summary>
    /// <returns>Nuova matrice inversa.</returns>
    /// <exception cref="InvalidOperationException">Se il determinante è zero (matrice singolare).</exception>
    public Matrix4X4 Inverse()
    {
        var det = Determinant();
        if (Math.Abs(det) < 1e-12) throw new InvalidOperationException("Singular matrix");
        var inv = new Matrix4X4();
        for (var i = 0; i < 4; i++)
        for (var j = 0; j < 4; j++)
        {
            // Cofattore
            double sign = ((i + j) % 2 == 0) ? 1 : -1;
            inv[j, i] = sign * Minor(i, j).Determinant() / det;
        }
        return inv;
    }

    /// <summary>
    /// Conversione verso <see cref="SimdMatrix"/>.
    /// </summary>
    /// <returns>Nuova SimdMatrix 4x4 con i valori della matrice.</returns>
    public SimdMatrix ToSimdMatrix() =>
        new([
            M11, M12, M13, M14,
            M21, M22, M23, M24,
            M31, M32, M33, M34,
            M41, M42, M43, M44
        ], 4, 4);

    /// <summary>
    /// Conversione da <see cref="SimdMatrix"/> 4x4.
    /// </summary>
    /// <param name="s">Matrice SimdMatrix 4x4.</param>
    /// <returns>Nuova Matrix4X4 con i valori della SimdMatrix.</returns>
    /// <exception cref="ArgumentException">Se la SimdMatrix non è 4x4.</exception>
    public static Matrix4X4 FromSimdMatrix(SimdMatrix s)
    {
        if (s.Rows != 4 || s.Cols != 4)
            throw new ArgumentException("SimdMatrix non è 4x4");
        return new Matrix4X4(
            s[0, 0], s[0, 1], s[0, 2], s[0, 3],
            s[1, 0], s[1, 1], s[1, 2], s[1, 3],
            s[2, 0], s[2, 1], s[2, 2], s[2, 3],
            s[3, 0], s[3, 1], s[3, 2], s[3, 3]
        );
    }

    /// <summary>
    /// Rappresentazione testuale della matrice.
    /// </summary>
    /// <returns>Stringa con i valori separati da tabulazione, righe separate da newline.</returns>
    public override string ToString() =>
        $"{M11}\t{M12}\t{M13}\t{M14}\n" +
        $"{M21}\t{M22}\t{M23}\t{M24}\n" +
        $"{M31}\t{M32}\t{M33}\t{M34}\n" +
        $"{M41}\t{M42}\t{M43}\t{M44}";
}
