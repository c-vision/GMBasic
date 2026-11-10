// Test del descrittore gestito MatrixValue (mirror di gmb_matrix_t).
// Verifica view O(1), strides, contiguità e clone indipendente.

using CommonLib;
using Xunit;

namespace Basic.IlBackend.Tests;

public class MatrixValueDescriptorTests
{
    private static MatrixValue Make3x3()
    {
        var m = MatrixValue.Create(3, 3);
        var v = 1d;
        for (var r = 0; r < 3; r++)
            for (var c = 0; c < 3; c++)
                m.Set(r, c, v++);
        return m;
    }

    [Fact]
    public void SliceView_IsO1_AndSharesStorage()
    {
        var m = Make3x3();
        var v = m.View(1, 1, 2, 2);

        Assert.Equal(2, v.Rows);
        Assert.Equal(2, v.Cols);
        Assert.Same(m.Storage, v.Storage);
        Assert.Equal(5d, v.Get(0, 0)); // (1,1) della 3x3
        Assert.Equal(9d, v.Get(1, 1)); // (2,2)

        v.Set(0, 0, 42d);
        Assert.Equal(42d, m.Get(1, 1)); // la view condivide lo storage
    }

    [Fact]
    public void TransposeView_SwapsDimensionsAndStrides()
    {
        var m = Make3x3();
        var t = m.TransposeView();

        Assert.Equal(3, t.Rows);
        Assert.Equal(3, t.Cols);
        Assert.Equal(m.ColStride, t.RowStride);
        Assert.Equal(m.RowStride, t.ColStride);
        Assert.Equal(m.Get(0, 2), t.Get(2, 0));
        Assert.Same(m.Storage, t.Storage);
    }

    [Fact]
    public void RowAndColViews_AreOneDimensionalMatrices()
    {
        var m = Make3x3();
        var row = m.RowView(1);
        var col = m.ColView(2);

        Assert.Equal(1, row.Rows);
        Assert.Equal(3, row.Cols);
        Assert.Equal(3, col.Rows);
        Assert.Equal(1, col.Cols);
        Assert.Equal(m.Get(1, 0), row.Get(0, 0));
        Assert.Equal(m.Get(0, 2), col.Get(0, 0));
    }

    [Fact]
    public void Clone_BreaksAliasing()
    {
        var m = Make3x3();
        var v = m.View(0, 0, 2, 2);
        var clone = v.Clone();

        Assert.NotSame(m.Storage, clone.Storage);
        clone.Set(0, 0, -1d);
        Assert.NotEqual(-1d, m.Get(0, 0));
        Assert.Equal(1d, m.Get(0, 0));
    }

    [Fact]
    public void IsContiguous_DetectsStridedViews()
    {
        var m = Make3x3();
        Assert.True(m.IsContiguous);
        Assert.True(m.RowView(0).IsContiguous);
        Assert.True(m.ColView(0).IsContiguous);
        Assert.False(m.TransposeView().IsContiguous); // colonna-major: non contigua row-major (canone C)
        Assert.True(m.View(0, 0, 2, 3).IsContiguous);  // prime due righe intere: contigue
        Assert.False(m.View(0, 0, 2, 2).IsContiguous);
        Assert.False(m.View(0, 0, 3, 2).IsContiguous); // tutte le righe, due colonne: stride
    }

    [Fact]
    public void FromRect_ToRect_RoundTrips()
    {
        var source = new double[,] { { 1, 2 }, { 3, 4 } };
        var m = MatrixValue.FromRect(source);
        var back = m.ToRect();
        Assert.Equal(source, back);
    }

    [Fact]
    public void Zeros_And_FromFlat()
    {
        var z = MatrixValue.Zeros(2, 3);
        Assert.Equal(2, z.Rows);
        Assert.Equal(3, z.Cols);
        Assert.True(z.IsContiguous);
        Assert.Equal(0d, z.Get(0, 0));
        Assert.Equal(0d, z.Get(1, 2));

        var f = MatrixValue.FromFlat([1, 2, 3, 4, 5, 6], 2, 3);
        Assert.Equal(6d, f.Get(1, 2));
        Assert.True(f.IsContiguous);
    }
}
