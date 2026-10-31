// FILE: ByRefBoxGenericTests.cs
// Unit test Fase 1 del piano Closure-ByRef-Box-Linker-Definitivo: verifica il nuovo tipo
// ByRefBox<T> (common/BRT/Gpm/ByRefBoxGeneric.cs) in isolamento, prima di qualunque uso da
// parte di linker/compiler (Fasi 2+). Zero dipendenze da IL/PCode/native — solo il tipo.
using Xunit;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend.Tests;

public class ByRefBoxGenericTests
{
    [Fact]
    public void DefaultValue_IsNull_ForReferenceType()
    {
        var box = new ByRefBox<string>();
        Assert.Null(box.Value);
    }

    [Fact]
    public void DefaultValue_IsZero_ForValueType()
    {
        var box = new ByRefBox<long>();
        Assert.Equal(0L, box.Value);
    }

    [Fact]
    public void ConstructorWithValue_SetsValue()
    {
        var box = new ByRefBox<long>(42L);
        Assert.Equal(42L, box.Value);
    }

    [Fact]
    public void WriteThenRead_RoundTrips()
    {
        var box = new ByRefBox<long>(100L);
        box.Value = box.Value + 1;
        Assert.Equal(101L, box.Value);
        box.Value = box.Value + 1;
        Assert.Equal(102L, box.Value);
    }

    [Fact]
    public void SharedReference_WriteThroughVisibleToOtherHolder()
    {
        // Simula il caso reale: due "holder" (genitore + closure) che condividono LO STESSO
        // box — una scrittura dall'uno deve essere visibile dall'altro (write-through), a
        // differenza di una copia per valore.
        var box = new ByRefBox<long>(100L);
        var aliasHeldByClosure = box;
        aliasHeldByClosure.Value = 101L;
        Assert.Equal(101L, box.Value);
    }

    [Fact]
    public void SetToNull_ThenBack_Works()
    {
        var box = new ByRefBox<string>("hello");
        box.Value = null;
        Assert.Null(box.Value);
        box.Value = "world";
        Assert.Equal("world", box.Value);
    }
}
