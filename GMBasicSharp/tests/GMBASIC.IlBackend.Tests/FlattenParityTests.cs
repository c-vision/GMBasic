using Basic.IlBackend;
using Xunit;

namespace GMBASIC.IlBackend.Tests;

[Trait("Category", "Fast")]
public sealed class FlattenParityTests
{
    [Fact]
    public void NestedArrays_AreFlattenedRecursively()
    {
        object? value = new object?[]
        {
            new double[] { 1, 2 },
            new object?[] { new int[] { 3, 4 }, 5.0 },
        };
        Assert.Equal(new double[] { 1, 2, 3, 4, 5 }, RuntimeFlatten.ToDoubles(value));
    }

    [Fact]
    public void StringsAreScalars_NumericParsed_NonNumericZero()
    {
        Assert.Equal(new double[] { 3.5, 0.0 },
            RuntimeFlatten.ToDoubles(new object?[] { "3.5", "x" }));
    }

    [Fact]
    public void Dictionary_IsScalarZero_NeverThrows()
    {
        var dict = new Dictionary<string, object> { ["a"] = 1 };
        Assert.Equal(new double[] { 0.0 }, RuntimeFlatten.ToDoubles(new object?[] { dict }));
    }

    [Fact]
    public void TypedArrays_UseFastPaths()
    {
        Assert.Equal(new double[] { 1, 2 }, RuntimeFlatten.ToDoubles(new int[] { 1, 2 }));
        Assert.Equal(new double[] { 1.5, 2.5 }, RuntimeFlatten.ToDoubles(new double[] { 1.5, 2.5 }));
    }
}
