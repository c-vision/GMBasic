// Test del registro di implementazione condiviso (task 6.0.2).
// Verifica che: il registry sia queryabile per backend, l'audit NotImplemented
// funzioni, e lo stato di default sia NotImplemented (mai stub silenziosi).

using Basic.BackendSupport;
using Xunit;

namespace Basic.BackendSupport.Tests;

public class BackendImplementationRegistryTests
{
    [Fact]
    public void NonDichiarato_Default_NotImplemented()
    {
        var r = new BackendImplementationRegistry();
        // nessuna registrazione → non implementato (mai stub silenziosi)
        Assert.Equal(ImplementationState.NotImplemented, r.StateOf(Backend.VMC, "DBF.LASTREC"));
        Assert.False(r.IsImplemented(Backend.VMC, "DBF.LASTREC"));
    }

    [Fact]
    public void Registrato_IsImplemented()
    {
        var r = new BackendImplementationRegistry();
        r.Register(Backend.PCodeVM, "DBF.LASTREC");
        Assert.True(r.IsImplemented(Backend.PCodeVM, "DBF.LASTREC"));
        Assert.Equal(ImplementationState.Implemented, r.StateOf(Backend.PCodeVM, "DBF.LASTREC"));
        // non deve propagare ad altri backend
        Assert.False(r.IsImplemented(Backend.IL, "DBF.LASTREC"));
    }

    [Fact]
    public void NotImplementedAuditIsQueryable()
    {
        var r = new BackendImplementationRegistry();
        r.Register(Backend.IL, "DBF.LASTREC");
        r.Register(Backend.IL, "DBF.USE");
        // non dichiarato → non implementato (mai stub silenziosi)
        r.Register(Backend.IL, "HTTP.SERVER", ImplementationState.NotImplemented);
        var missing = r.NotImplemented(Backend.IL).ToList();
        Assert.DoesNotContain("DBF.LASTREC", missing);
        Assert.Contains("HTTP.SERVER", missing);
    }

    [Fact]
    public void Divergence_ContaComeImplementato()
    {
        var r = new BackendImplementationRegistry();
        r.Register(Backend.C99Native, "VECTOR.ADD WRAP", ImplementationState.ImplementedWithDivergence);
        Assert.True(r.IsImplemented(Backend.C99Native, "VECTOR.ADD WRAP"));
        Assert.Equal(ImplementationState.ImplementedWithDivergence,
            r.StateOf(Backend.C99Native, "VECTOR.ADD WRAP"));
    }

    [Fact]
    public void Backend_Enum_HaQuattroValori()
    {
        Assert.Equal(4, System.Enum.GetValues<Backend>().Length);
    }

    [Fact]
    public void PCodeVM_Registry_HaTutteLeBuiltinImplementate()
    {
        // Registry generato da gmb_audit --registry-pcodevm (superficie canonica StdLib):
        // namespace.nome delle funzioni canoniche che il PCodeVM dispatcha.
        var r = BackendImplementationRegistry.CreatePCodeVM();
        Assert.True(r.IsImplemented(Backend.PCodeVM, "MATH.Abs"));
        Assert.True(r.IsImplemented(Backend.PCodeVM, "DBF.LASTREC"));
        // non presenti → NotImplemented (mai stub silenziosi)
        Assert.Equal(ImplementationState.NotImplemented, r.StateOf(Backend.PCodeVM, "NON_ESISTE"));
    }

    [Fact]
    public void IL_Registry_HaTokenKindDispatchati()
    {
        // Registry generato da gmb_audit --registry-il (superficie canonica StdLib).
        var r = BackendImplementationRegistry.CreateIL();
        Assert.True(r.IsImplemented(Backend.IL, "MATH.Sin"));
        Assert.True(r.IsImplemented(Backend.IL, "DBF.LASTREC"));
        Assert.Equal(ImplementationState.NotImplemented, r.StateOf(Backend.IL, "NON_ESISTE"));
    }

    [Fact]
    public void VMC_Registry_HaCaseNumerici()
    {
        // Registry generato da gmb_audit --registry-vmc (superficie canonica StdLib).
        var r = BackendImplementationRegistry.CreateVMC();
        Assert.True(r.IsImplemented(Backend.VMC, "MATH.Abs"));
        Assert.True(r.IsImplemented(Backend.VMC, "MATH.Sin"));
        Assert.Equal(ImplementationState.NotImplemented, r.StateOf(Backend.VMC, "NON_ESISTE"));
    }

    [Fact]
    public void C99Native_Registry_HaFunzioniCanoniche()
    {
        // Registry generato da gmb_audit --registry-c99native (superficie canonica StdLib,
        // funzioni emesse dal compilatore nativo — non più statement GMB_STMT_).
        var r = BackendImplementationRegistry.CreateC99Native();
        Assert.True(r.IsImplemented(Backend.C99Native, "MATH.Abs"));
        Assert.True(r.IsImplemented(Backend.C99Native, "SQL.COL"));
        Assert.Equal(ImplementationState.NotImplemented, r.StateOf(Backend.C99Native, "NON_ESISTE"));
    }
}
