// Funzioni matriciali scipy.linalg in MATRIX (EXPM, KRONECKER).
// Oracoli: scipy.linalg.expm / numpy.kron 1.18.
using System.IO;
using MathNet.Numerics.LinearAlgebra.Double;
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class SciPyLinalgTests
{
    private static void AssertClose(double e, double a, double tol = 1e-9)
        => Assert.True(Math.Abs(e - a) <= tol, $"atteso {e} ottenuto {a}");

    [Fact]
    public void CommonLib_MatrixExp_Matches_SciPy()
    {
        var a = DenseMatrix.OfArray(new double[,] { { 1, 2 }, { 3, 4 } });
        var r = CommonLib.Numerics.MatrixExp(a);
        AssertClose(51.96895619870756, r[0, 0], 1e-6);
        AssertClose(74.73656456700694, r[0, 1], 1e-6);
        AssertClose(112.10484685051044, r[1, 0], 1e-6);
        AssertClose(164.07380304921801, r[1, 1], 1e-6);
        // expm(I) = e·I
        var i = DenseMatrix.CreateIdentity(2);
        var ei = CommonLib.Numerics.MatrixExp(i);
        AssertClose(Math.E, ei[0, 0]); AssertClose(0, ei[0, 1]);
    }

    [Fact]
    public void CommonLib_Kronecker_Matches_Numpy()
    {
        var a = DenseMatrix.OfArray(new double[,] { { 1, 2 }, { 3, 4 } });
        var b = DenseMatrix.OfArray(new double[,] { { 0, 1 }, { 1, 0 } });
        var k = CommonLib.Numerics.MatrixKron(a, b);
        double[,] expected = { { 0, 1, 0, 2 }, { 1, 0, 2, 0 }, { 0, 3, 0, 4 }, { 3, 0, 4, 0 } };
        for (var i = 0; i < 4; i++)
            for (var j = 0; j < 4; j++)
                AssertClose(expected[i, j], k[i, j]);
    }

    private static double ParseLabeled(string output, string label)
    {
        var m = System.Text.RegularExpressions.Regex.Match(output, System.Text.RegularExpressions.Regex.Escape(label) + @"\s*=\s*(-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)");
        Assert.True(m.Success, $"etichetta '{label}' non trovata in:\n{output}");
        return double.Parse(m.Groups[1].Value, System.Globalization.CultureInfo.InvariantCulture);
    }

    [Fact]
    public void PCode_Dispatches_Matrix_Exp_And_Kron()
    {
        var src = Path.Combine(Path.GetTempPath(), "gmb_mfunc_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(src, """
            IMPORT MATRIX
            SUB Main()
              h = MATRIX.CREATE(2, 2)
              e = MATRIX.EXPM(h)
              PRINT MATRIX.DETERMINANT(e)
              k = MATRIX.KRONECKER(h, h)
              PRINT MATRIX.DETERMINANT(k)
            END SUB
            """);
        try
        {
            var program = Basic.Orchestrator.Compile(src, null, false, "il");
            var chunk = new global::Compiler.Compiler().Compile(program);
            var originalOut = Console.Out;
            var sw = new StringWriter();
            try
            {
                Console.SetOut(sw);
                Basic.IlBackend.IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
                var r = global::Basic.IlBackend.PCode.PCodeVmRunner.Run(chunk);
                Assert.True(r.Success, "MATRIX.EXPM/KRONECKER dispatch fallito:\n" + r.Output);
                var nums = System.Text.RegularExpressions.Regex.Matches(r.Output, @"-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?")
                    .Select(m => double.Parse(m.Value, System.Globalization.CultureInfo.InvariantCulture)).ToList();
                Assert.True(nums.Count >= 2, $"output: {r.Output}");
                AssertClose(Math.Exp(2), nums[0]); // det(expm(I)) = e^2
                AssertClose(1.0, nums[1]);         // det(kron(I,I)) = 1
            }
            finally { Console.SetOut(originalOut); }
        }
        finally { try { File.Delete(src); } catch { } }
    }

    [Fact]
    public void MatrixSqrt_Matches_SciPy()
    {
        var a = DenseMatrix.OfArray(new double[,] { { 2, 1 }, { 1, 2 } });
        var s = CommonLib.Numerics.MatrixSqrt(a);
        AssertClose(1.36602540378444, s[0, 0], 1e-9);
        AssertClose(0.36602540378444, s[0, 1], 1e-9);
        AssertClose(0.36602540378444, s[1, 0], 1e-9);
        AssertClose(1.36602540378444, s[1, 1], 1e-9);
        // S·S = A
        var ss = s * s;
        AssertClose(2, ss[0, 0], 1e-6); AssertClose(1, ss[0, 1], 1e-6);
        AssertClose(1, ss[1, 0], 1e-6); AssertClose(2, ss[1, 1], 1e-6);
        // 3x3
        var b = DenseMatrix.OfArray(new double[,] { { 4, 1, 0 }, { 1, 3, 0.5 }, { 0, 0.5, 2 } });
        var s3 = CommonLib.Numerics.MatrixSqrt(b);
        AssertClose(1.9813751386, s3[0, 0], 1e-6);
        AssertClose(1.7028691474, s3[1, 1], 1e-6);
        AssertClose(1.4048405999, s3[2, 2], 1e-6);
    }

    [Fact]
    public void MatrixLogm_Matches_SciPy()
    {
        var a = DenseMatrix.OfArray(new double[,] { { 2, 1 }, { 1, 2 } });
        var l = CommonLib.Numerics.MatrixLogm(a);
        AssertClose(0.549306144334, l[0, 0], 1e-6);
        AssertClose(0.549306144334, l[0, 1], 1e-6);
        AssertClose(0.549306144334, l[1, 0], 1e-6);
        AssertClose(0.549306144334, l[1, 1], 1e-6);
        // expm(logm(A)) = A
        var back = CommonLib.Numerics.MatrixExp(l);
        AssertClose(2, back[0, 0], 1e-5); AssertClose(1, back[0, 1], 1e-5);
        AssertClose(1, back[1, 0], 1e-5); AssertClose(2, back[1, 1], 1e-5);
    }

    [Fact]
    public void MatrixHessenberg_Matches_SciPy()
    {
        var h = DenseMatrix.OfArray(new double[,] { { 1, 2, 3 }, { 4, 5, 6 }, { 7, 8, 10 } });
        var hh = CommonLib.Numerics.MatrixHessenberg(h);
        // forma superiore di Hessenberg: zeri sotto la sottodiagonale
        Assert.True(Math.Abs(hh[2, 0]) < 1e-9, $"hh[2,0]={hh[2,0]}");
        // stessi autovalori di H
        var evH = h.Evd().EigenValues;
        var evHh = hh.Evd().EigenValues;
        for (var i = 0; i < 3; i++)
            Assert.True(Math.Abs(evH[i].Real - evHh[i].Real) < 1e-6, $"autovalore {i}");
}

    // ---- scipy.linalg completato: funzioni matriciali via EVD ----
    [Fact]
    public void MatrixFunctions_CosmSinmTanm_Matches_SciPy()
    {
        var a = DenseMatrix.OfArray(new double[,] { { 2, 1 }, { 1, 2 } });
        var c = CommonLib.Numerics.MatrixCos(a);
        AssertClose(-0.2248450953661529, c[0, 0], 1e-9);
        AssertClose(-0.7651474012342924, c[0, 1], 1e-9);
        var s = CommonLib.Numerics.MatrixSin(a);
        AssertClose(-0.35017548837401463, s[0, 1], 1e-9);
        var t = CommonLib.Numerics.MatrixTan(a);
        AssertClose(0.707430590790312, t[0, 0], 1e-6); // scipy: sinm·inv(cosm)
    }

    [Fact]
    public void MatrixSignmFractional_Matches_SciPy()
    {
        var a = DenseMatrix.OfArray(new double[,] { { 2, 1 }, { 1, 2 } });
        var sg = CommonLib.Numerics.MatrixSignm(a);
        AssertClose(1, sg[0, 0], 1e-9);
        AssertClose(0, sg[0, 1], 1e-9);
        var fp = CommonLib.Numerics.MatrixFractionalPower(a, 0.5);
        AssertClose(1.36602540378444, fp[0, 0], 1e-9);
        AssertClose(0.366025403784439, fp[0, 1], 1e-9);
    }

    [Fact]
    public void OrthSubspaceAnglesPolarCondNuclear_Matches_SciPy()
    {
        var g = DenseMatrix.OfArray(new double[,] { { 1, 2 }, { 2, 3 } });
        var o = CommonLib.Numerics.Orth(g);
        AssertClose(-0.5257311121191336, o[0, 0], 1e-9);
        var a2 = DenseMatrix.OfArray(new double[,] { { 2, 1 }, { 1, 2 } });
        var ang = CommonLib.Numerics.SubspaceAngles(a2, g);
        Assert.Equal(2, ang.Length);
        AssertClose(0, ang[0], 1e-9);
        var pol = CommonLib.Numerics.Polar(g);
        AssertClose(-0.4472135954999579, pol.U[0, 0], 1e-9);
        AssertClose(1.3416407864998738, pol.P[0, 0], 1e-9);
        AssertClose(17.94427190999916, CommonLib.Numerics.ConditionNumber(g), 1e-9);
        AssertClose(4.47213595499958, CommonLib.Numerics.NuclearNorm(g), 1e-9);
    }

    [Fact]
    public void ToeplitzHankelCirculant_Matches_SciPy()
    {
        var tp = CommonLib.Numerics.Toeplitz(new double[] { 1, 2, 3 }, new double[] { 1, 4, 5 });
        AssertClose(4, tp[0, 1]); AssertClose(2, tp[1, 0]);
        var hk = CommonLib.Numerics.Hankel(new double[] { 1, 2, 3 }, new double[] { 3, 4, 5 });
        AssertClose(4, hk[2, 1]); AssertClose(5, hk[2, 2]);
        var cc = CommonLib.Numerics.Circulant(new double[] { 1, 2, 3 });
        AssertClose(3, cc[0, 1]); AssertClose(2, cc[1, 0]);
    }

    [Fact]
    public void SolveTriangular_Matches_SciPy()
    {
        var l = DenseMatrix.OfArray(new double[,] { { 2, 0 }, { 1, 2 } });
        var x = CommonLib.Numerics.SolveTriangular(l, MathNet.Numerics.LinearAlgebra.Double.DenseVector.OfArray(new double[] { 4, 5 }), true);
        AssertClose(2, x[0]); AssertClose(1.5, x[1]);
        var u = DenseMatrix.OfArray(new double[,] { { 2, 1 }, { 0, 2 } });
        var x2 = CommonLib.Numerics.SolveTriangular(u, MathNet.Numerics.LinearAlgebra.Double.DenseVector.OfArray(new double[] { 4, 5 }), false);
        AssertClose(0.75, x2[0]); AssertClose(2.5, x2[1]);
    }

    // ---- EIG generale (non simmetrica) + real Schur ----
    [Fact]
    public void EigGeneral_Matches_SciPy()
    {
        // autovalori reali: [[1,2],[2,1]] -> 3, -1
        var a = DenseMatrix.OfArray(new double[,] { { 1, 2 }, { 2, 1 } });
        var ed = CommonLib.Numerics.GeneralEigen(a);
        var re = ed.EigenvaluesReal;
        Assert.Equal(3, Math.Round(re[0]));
        Assert.Equal(-1, Math.Round(re[1]));
        Assert.Equal(0.0, ed.EigenvaluesImag[0], 5);
        // A·v = λ·v per ogni autovettore
        var vr = ed.EigenvectorsReal;
        var vi = ed.EigenvectorsImag;
        for (var j = 0; j < 2; j++)
        {
            double res = 0;
            for (var i = 0; i < 2; i++)
            {
                double p = 0;
                for (var k = 0; k < 2; k++) p += a[i, k] * vr[k, j];
                res += Math.Abs(p - ed.EigenvaluesReal[j] * vr[i, j]);
            }
            Assert.True(res < 1e-9, $"residuo autovettore {j}: {res}");
        }
    }

    [Fact]
    public void EigGeneralComplex_Matches_SciPy()
    {
        // [[0,-1],[1,0]] -> autovalori ±i
        var a = DenseMatrix.OfArray(new double[,] { { 0, -1 }, { 1, 0 } });
        var ed = CommonLib.Numerics.GeneralEigen(a);
        Assert.Equal(0.0, Math.Abs(ed.EigenvaluesReal[0]), 6);
        Assert.Equal(0.0, Math.Abs(ed.EigenvaluesReal[1]), 6);
        Assert.True(Math.Abs(Math.Abs(ed.EigenvaluesImag[0]) - 1) < 1e-9);
        Assert.True(Math.Abs(Math.Abs(ed.EigenvaluesImag[1]) - 1) < 1e-9);
        // residui complessi A·v = λ·v
        var vr = ed.EigenvectorsReal;
        var vi = ed.EigenvectorsImag;
        for (var j = 0; j < 2; j++)
        {
            double res = 0;
            for (var i = 0; i < 2; i++)
            {
                double pr = 0, pi = 0;
                for (var k = 0; k < 2; k++) { pr += a[i, k] * vr[k, j]; pi += a[i, k] * vi[k, j]; }
                res += Math.Abs(pr - (ed.EigenvaluesReal[j] * vr[i, j] - ed.EigenvaluesImag[j] * vi[i, j]));
                res += Math.Abs(pi - (ed.EigenvaluesReal[j] * vi[i, j] + ed.EigenvaluesImag[j] * vr[i, j]));
            }
            Assert.True(res < 1e-9, $"residuo autovettore {j}: {res}");
        }
    }

    [Fact]
    public void Schur_Matches_SciPy()
    {
        var m = DenseMatrix.OfArray(new double[,] { { 1, 2, 3 }, { 4, 5, 6 }, { 7, 8, 10 } });
        var sch = CommonLib.Numerics.RealSchur(m);
        var t = sch.RealSchurForm;
        var z = sch.SchurVectors;
        // quasi-triangolare: sotto la sottodiagonale tutto ~0
        for (var i = 2; i < 3; i++)
            for (var j = 0; j < i - 1; j++)
                Assert.True(Math.Abs(t[i, j]) < 1e-9, $"t[{i},{j}]={t[i, j]}");
        // ricostruzione A = Z·T·Z^T
        var rec = z * t * z.Transpose();
        for (var i = 0; i < 3; i++)
            for (var j = 0; j < 3; j++)
                Assert.True(Math.Abs(rec[i, j] - m[i, j]) < 1e-9, $"rec[{i},{j}]={rec[i, j]} atteso {m[i, j]}");
        // stessi autovalori (traccia)
        var tr = t[0, 0] + t[1, 1] + t[2, 2];
        AssertClose(16, tr, 1e-9);
    }
}
