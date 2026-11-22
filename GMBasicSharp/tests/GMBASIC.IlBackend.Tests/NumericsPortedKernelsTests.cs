// FILE: tests/GMBASIC.IlBackend.Tests/NumericsPortedKernelsTests.cs
// Copertura delle parti portate al runtime C che BASIC non espone direttamente:
//   - Outliers metodo IQR (TSOUTLIERS espone solo Z);
//   - RollingDelta con order > 1 (TSROLLDELTA espone solo l'ordine 1);
//   - valori di Decompose e RollingSortino (i MiniTest ne controllano solo forma/rounding).
// Tutti i valori attesi sono calcolati a mano sulle formule documentate.

using System;
using CommonLib;
using Xunit;

namespace GMBASIC.IlBackend.Tests;

public class NumericsPortedKernelsTests
{
    private static void AssertClose(double expected, double actual, double tol = 1e-12)
        => Assert.True(Math.Abs(expected - actual) <= tol, $"atteso {expected}, ottenuto {actual}");

    [Fact]
    public void Outliers_Iqr_Finds_Only_The_Far_Value()
    {
        // n=5: Q1=sorted[1]=2, Q3=sorted[3]=4, IQR=2, th=1.5 → [-1, 7] → solo 100 è fuori.
        var idx = Numerics.Outliers(new[] { 1.0, 2.0, 3.0, 4.0, 100.0 }, "IQR");
        Assert.Equal(new[] { 4 }, idx);
    }

    [Fact]
    public void Outliers_Iqr_Threshold_Changes_The_Bounds()
    {
        // th=0.1 → low = 2 - 0.1*2 = 1.8, high = 4 + 0.2 = 4.2 → fuori 1 (idx 0) e 100 (idx 4).
        var idx = Numerics.Outliers(new[] { 1.0, 2.0, 3.0, 4.0, 100.0 }, "IQR", 0.1);
        Assert.Equal(new[] { 0, 4 }, idx);
    }

    [Fact]
    public void Decompose_Uses_Centered_Moving_Average_And_Phase_Means()
    {
        var d = Numerics.Decompose(new double[] { 1, 2, 3, 4, 5, 6, 7, 8 }, 2);
        Assert.Equal(3, d.GetLength(0));
        Assert.Equal(8, d.GetLength(1));
        AssertClose(1.5, d[0, 0]);   // trend ai bordi: media clampata di {1,2}
        AssertClose(2.0, d[0, 1]);
        AssertClose(7.5, d[0, 7]);   // bordo destro: media di {7,8}
        AssertClose(-0.125, d[1, 0]); // stagionalità fase 0
        AssertClose(0.125, d[1, 1]);  // stagionalità fase 1
        AssertClose(-0.375, d[2, 0]); // residuo = 1 - 1.5 - (-0.125)
        AssertClose(0.375, d[2, 7]);  // residuo = 8 - 7.5 - 0.125
    }

    [Fact]
    public void RollingSortino_Divides_By_Negative_Count()
    {
        var r = Numerics.RollingSortino(new[] { -0.1, 0.2, -0.2, 0.3 }, 2);
        AssertClose(0.5, r[0]);   // mean 0.05 / 0.1
        AssertClose(0.0, r[1]);   // mean 0 / 0.2
        AssertClose(0.25, r[2]);  // mean 0.05 / 0.2
    }

    [Fact]
    public void RollingSortino_No_Downside_Is_Positive_Infinity()
    {
        var r = Numerics.RollingSortino(new[] { 0.1, 0.2, 0.3 }, 2);
        Assert.True(double.IsPositiveInfinity(r[0]));
        Assert.True(double.IsPositiveInfinity(r[1]));
    }

    [Fact]
    public void RollingDelta_Supports_Order_Greater_Than_One()
    {
        var d1 = Numerics.RollingDelta(new double[] { 1, 2, 4, 7, 11 }, 3);
        AssertClose(3, d1[0]);
        AssertClose(5, d1[1]);
        AssertClose(7, d1[2]);

        var d2 = Numerics.RollingDelta(new double[] { 1, 2, 4, 7, 11 }, 3, order: 2);
        AssertClose(2, d2[0]);
        AssertClose(3, d2[1]);
        AssertClose(4, d2[2]);
    }

    [Fact]
    public void GpRegress_And_GmmEm1D_Use_The_Runtime()
    {
        // GP su dati lineari y=2x con rumore 0: la previsione ai nodi deve tornare y.
        var gp = Numerics.GpRegress(
            new double[] { 0, 1, 2, 3 }, new double[] { 0, 2, 4, 6 }, new double[] { 1, 2 }, 1.0, 1e-6);
        AssertClose(2.0, gp.Mean[0], 1e-4);
        AssertClose(4.0, gp.Mean[1], 1e-4);

        // GMM su due cluster ben separati: le medie devono cadere sui due gruppi.
        var gmm = Numerics.GmmEm1D(new double[] { 0, 0.1, 0.2, 10, 10.1, 10.2 }, 2, 100);
        var m0 = Math.Min(gmm.Mu[0], gmm.Mu[1]);
        var m1 = Math.Max(gmm.Mu[0], gmm.Mu[1]);
        AssertClose(0.1, m0, 1e-3);
        AssertClose(10.1, m1, 1e-3);
    }
}
