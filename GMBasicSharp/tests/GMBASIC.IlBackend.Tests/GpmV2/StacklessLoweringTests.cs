// G2 — StacklessLowering + verifier IR (piano §8, §12 G2).
// Compila GoRoutinesWorkers.bas (IF+RECEIVE+SLEEP+SEND in Lavoratore; FOR+RECEIVE in
// Manager), calcola il may-park, lowera ogni procedura may-suspend e verifica:
//   - IR valido (IlCoroutineVerifier);
//   - struttura FOR (init→check, check Switch(corpo, uscita), next→check);
//   - park (Await + Park) per SLEEP/SEND/RECEIVE;
//   - IF con terminator Switch.

using System.Collections.Immutable;
using Xunit;
using Basic.IlBackend.Gpm;
using Basic.IlBackend.Gpm.Ir;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class StacklessLoweringTests
{
    private static readonly string Root = FindRepoRoot();

    private static string FindRepoRoot()
    {
        var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (dir != null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "GMBasicSharp", "GMBasicSharp.slnx"))) return dir.FullName;
            dir = dir.Parent;
        }
        return Directory.GetCurrentDirectory();
    }

    private static (ImmutableArray<FunctionDefinition> procedures, ImmutableHashSet<int> mayPark, Func<int, FunctionDefinition?> byId) Compile(string rel)
    {
        var sample = Path.Combine(Root, rel);
        C99Emission.Require(sample);
        Assert.True(File.Exists(sample), $"Sample mancante: {sample}");
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var procedures = program.LoadedModules.Values
            .SelectMany(m => m.Declarations)
            .OfType<FunctionDefinition>()
            .ToImmutableArray();
        var byId = (int id) => procedures.FirstOrDefault(p => p.SymbolId == id);
        var mayPark = MayParkAnalysis.ComputeMayPark(procedures, byId);
        return (procedures, mayPark, byId);
    }

    [Fact]
    public void AllMayParkProcedures_LowerTo_ValidIr()
    {
        var (procedures, mayPark, byId) = Compile(Path.Combine("shared", "samples", "Concurrency", "GoRoutinesWorkers.bas"));
        Assert.True(mayPark.Count >= 3, $"Attese ≥3 procedure may-park, trovate {mayPark.Count}");

        foreach (var proc in procedures.Where(p => mayPark.Contains(p.SymbolId)))
        {
            var ir = StacklessLowering.Lower(proc, mayPark, byId);
            IlCoroutineVerifier.AssertValid(ir, proc.OriginalName.Lexeme);
            Assert.Contains(StacklessLowering.EntryPc, ir.ByPc.Keys);
        }
    }

    [Fact]
    public void Manager_ForLoop_IsCabledCheckNextBackEdge()
    {
        var (procedures, mayPark, byId) = Compile(Path.Combine("shared", "samples", "Concurrency", "GoRoutinesWorkers.bas"));
        var manager = procedures.First(p => p.OriginalName.Lexeme.Equals("Manager", StringComparison.OrdinalIgnoreCase));
        var ir = StacklessLowering.Lower(manager, mayPark, byId);

        var init = ir.Blocks.First(b => b.AstNode is ForStmt);
        Assert.Equal(IlIrTermKind.Branch, init.Terminator.Kind);

        var check = ir.Blocks.First(b => b.AstNode is ForCheckAndJumpStmt);
        Assert.Equal(IlIrTermKind.Switch, check.Terminator.Kind);
        Assert.Equal(2, check.Terminator.Targets.Length);
        // corpo e uscita devono essere target validi
        Assert.True(ir.ByPc.ContainsKey(check.Terminator.Targets[0]));
        Assert.True(ir.ByPc.ContainsKey(check.Terminator.Targets[1]) || check.Terminator.Targets[1] == StacklessLowering.EndPc);

        var next = ir.Blocks.First(b => b.AstNode is NextStmt);
        Assert.Equal(IlIrTermKind.Branch, next.Terminator.Kind);
        Assert.Equal(check.Pc, next.Terminator.TargetPc); // loop-back al check

        // RECEIVE nel corpo del FOR è un park con Await+Park.
        var receivePark = ir.Blocks.First(b => b.AstNode is ReceiveStmt);
        Assert.Equal(IlIrTermKind.Park, receivePark.Terminator.Kind);
        Assert.Contains(receivePark.Instructions, i => i.Kind == IlIrInstKind.Await);
    }

    [Fact]
    public void Lavoratore_IfAndParks_AreLowered()
    {
        var (procedures, mayPark, byId) = Compile(Path.Combine("shared", "samples", "Concurrency", "GoRoutinesWorkers.bas"));
        var lavoratore = procedures.First(p => p.OriginalName.Lexeme.Equals("Lavoratore", StringComparison.OrdinalIgnoreCase));
        var ir = StacklessLowering.Lower(lavoratore, mayPark, byId);

        // IF → blocco condizione con Switch.
        var ifBlock = ir.Blocks.First(b => b.AstNode is IfStmt);
        Assert.Equal(IlIrTermKind.Switch, ifBlock.Terminator.Kind);
        Assert.Equal(2, ifBlock.Terminator.Targets.Length);

        // SLEEP / SEND / RECEIVE → park con Await + Park.
        Assert.Contains(ir.Blocks, b => b.AstNode is SleepStmt && b.Terminator.Kind == IlIrTermKind.Park);
        Assert.Contains(ir.Blocks, b => b.AstNode is SendStmt && b.Terminator.Kind == IlIrTermKind.Park);
        Assert.Contains(ir.Blocks, b => b.AstNode is ReceiveStmt && b.Terminator.Kind == IlIrTermKind.Park);
    }

    [Fact]
    public void Verifier_Rejects_UnbalancedStack()
    {
        var ir = new IlCoroutineIr { ProcedureId = 0, LocalCount = 4, SpillCount = 0 };
        var b = ir.CreateBlock(0, IlIrRegionPath.Empty);
        b.Instructions.Add(new IlIrInst(IlIrInstKind.StoreLocal, Slot: 0)); // pop a vuoto
        b.Terminator = new IlIrTerminator(IlIrTermKind.CompleteTask);

        var errors = IlCoroutineVerifier.Validate(ir);
        Assert.Contains(errors, e => e.Contains("pop a vuoto", StringComparison.OrdinalIgnoreCase));

        // Park senza Await → rifiutato.
        var ir2 = new IlCoroutineIr { ProcedureId = 1, LocalCount = 4, SpillCount = 0 };
        var b2 = ir2.CreateBlock(0, IlIrRegionPath.Empty);
        b2.Terminator = new IlIrTerminator(IlIrTermKind.Park, ResumePc: StacklessLowering.EndPc);
        var errors2 = IlCoroutineVerifier.Validate(ir2);
        Assert.Contains(errors2, e => e.Contains("senza Await", StringComparison.OrdinalIgnoreCase));
    }
}
