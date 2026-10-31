// scipy.spatial.KDTree — query k-NN e query_ball_point. Oracoli: scipy 1.18.
using System;
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class SciPyKdTreeTests
{
    private static double[,] Pts()
    {
        return new double[,] {
            { 2, 3 }, { 5, 4 }, { 9, 6 }, { 4, 7 }, { 8, 1 },
            { 7, 2 }, { 1, 8 }, { 3, 3 }, { 6, 5 }, { 0, 6 }
        };
    }

    [Fact]
    public void Query_KNn_Matches_SciPy()
    {
        var tree = CommonLib.Numerics.KdTreeBuild(Pts());
        var (d, i) = CommonLib.Numerics.KdTreeQuery(tree, [4, 4], 3);
        Assert.Equal(3, i.Length);
        Assert.True(Math.Abs(d[0] - 1.0) < 1e-9 && Math.Abs(d[1] - Math.Sqrt(2)) < 1e-9 && Math.Abs(d[2] - Math.Sqrt(5)) < 1e-9,
            $"d={string.Join(",", d)}");
        Assert.Equal(1, i[0]);
        Assert.Equal(7, i[1]);
        Assert.Equal(0, i[2]);

        var (d5, i5) = CommonLib.Numerics.KdTreeQuery(tree, [4, 4], 5);
        Assert.Equal(5, i5.Length);
        Assert.True(Math.Abs(d5[4] - 3.0) < 1e-9, $"d5[4]={d5[4]}");
        // i5 = [1,7,0,8,3] (3 e 8 hanno la stessa distanza √5 → ordine non garantito tra i due)
        Assert.Contains(1, i5);
        Assert.Contains(7, i5);
        Assert.Contains(0, i5);
        Assert.Contains(8, i5);
        Assert.Contains(3, i5);
    }

    [Fact]
    public void QueryBall_Matches_SciPy()
    {
        var tree = CommonLib.Numerics.KdTreeBuild(Pts());
        var b = CommonLib.Numerics.KdTreeQueryBall(tree, [4, 4], 2.0);
        Array.Sort(b);
        Assert.Equal(new[] { 1, 7 }, b);
    }

    [Fact]
    public void Query_3D_Matches_SciPy()
    {
        var pts3 = new double[,] { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 1, 1, 1 }, { 2, 2, 2 }, { 3, 0, 1 } };
        var tree = CommonLib.Numerics.KdTreeBuild(pts3);
        var (d, i) = CommonLib.Numerics.KdTreeQuery(tree, [0.5, 0.5, 0.5], 2);
        Assert.Equal(2, i.Length);
        // i punti [0,0,0],[1,0,0],[0,1,0],[1,1,1] sono TUTTI a distanza √0.75 → con tie
        // l'ordine non è deterministico tra scipy e la nostra; verifichiamo le distanze e il set.
        Assert.True(Math.Abs(d[0] - 0.8660254037844386) < 1e-9 && Math.Abs(d[1] - 0.8660254037844386) < 1e-9,
            $"d={string.Join(",", d)}");
        foreach (var idx in i) Assert.Contains(idx, new[] { 0, 1, 2, 3 });
    }
}
