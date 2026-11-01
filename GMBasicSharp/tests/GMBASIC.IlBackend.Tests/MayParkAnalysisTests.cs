// STEP 2a — MayParkAnalysis: test RISTI PILO dell'analisi (AST sintetico, nessuna
// pipeline). Verifica i punti di blocco espliciti, la propagazione lungo il call graph
// (fixed-point incl. ricorsione mutua) e lo split in chunk con rientro sul punto di
// blocco (GWERTY: il retry riparte dall'operazione parcheggiata, MAI dai side-effect
// precedenti — vedi GMBasic-ILBackend-GPM-Chunk-State-Machine §7 vs §16.3).

using System.Collections.Immutable;
using Xunit;
using Basic.IlBackend.Gpm;

namespace Basic.IlBackend.Tests;

public class MayParkAnalysisTests
{
    private static readonly SourcePosition P = SourcePosition.Unknown;

    private static Token Tok(string lexeme) => new(TokenKind.Identifier, lexeme, null, P);

    private static VariableExpr Var(int symbolId, string name) => new(symbolId, Tok(name + "$"), Tok(name));

    private static VariableTarget TargetV(int symbolId, string name) => new(symbolId, Tok(name + "$"), Tok(name));

    private static FunctionExpr Call(int symbolId) => new(symbolId, Tok("f"), [], null);

    private static FunctionDefinition Def(int symbolId, params Stmt[] body) =>
        new(symbolId, Tok("f" + symbolId), [], new VoidType(), [.. body]);

    /// <summary>Body piatti usati anche nel design §16: PRINT / SEND / RECEIVE.</summary>
    private static Stmt Print(int symVar) => new ExpressionStmt(Var(symVar, "x"));
    private static Stmt Send(int chSym) => new SendStmt(Var(1, "x"), Var(chSym, "ch"), Tok("SEND"));
    private static Stmt Recv(int chSym) => new ReceiveStmt(TargetV(2, "v"), Var(chSym, "ch"), Tok("RECEIVE"));

    [Fact]
    public void FlatBlockPoints_AreDetected()
    {
        // Nessun blocco: not may-park.
        var noBlock = Def(1, Print(3), Print(4));
        Assert.False(MayParkAnalysis.IsLocalMayPark(noBlock, _ => false));

        // SEND e RECEIVE espliciti.
        Assert.True(MayParkAnalysis.IsLocalMayPark(Def(2, Send(10)), _ => false));
        Assert.True(MayParkAnalysis.IsLocalMayPark(Def(3, Recv(10)), _ => false));
        Assert.True(MayParkAnalysis.IsLocalMayPark(Def(4, Print(3), Send(10), Print(4)), _ => false));

        // GO SUB/GO FUNCTION è uno SPAWN NON-bloccante (§11.4: "no blocco") e in §16.3
        // sta DENTRO il chunk 0 di Main — NON è un punto di blocco. Il suo significato GPM
        // (la creazione del task figlio) non richiede lo split del chiamante.
        Assert.False(MayParkAnalysis.IsBlockPoint(new GoProcedureStmt(Call(5), null)));
        // GO EXIT termina il task con Finished senza sospendere: non è un punto di blocco.
        Assert.False(MayParkAnalysis.IsBlockPoint(new GoExitStmt(Tok("GOEXIT"))));
        // WAIT e SLEEP parcheggiano: sì.
        Assert.True(MayParkAnalysis.IsBlockPoint(new WaitStmt(Var(6, "wg"), Tok("WAIT"))));
        Assert.True(MayParkAnalysis.IsBlockPoint(new SleepStmt(new NumberExpr(100.0, Tok("100")), Tok("SLEEP"))));
        Assert.True(MayParkAnalysis.IsBlockPoint(new SyncStmt(Tok("SYNC"))));
        Assert.True(MayParkAnalysis.IsBlockPoint(new SpriteCommandStmt(
            new Token(TokenKind.SpriteLoad, "LOAD", null, P), [], Tok("SPRITE"))));
        Assert.False(MayParkAnalysis.IsBlockPoint(new SpriteCommandStmt(
            new Token(TokenKind.SpriteSetPosition, "SETPOSITION", null, P), [], Tok("SPRITE"))));
        Assert.True(MayParkAnalysis.IsLocalMayPark(
            Def(9, new SyncStmt(Tok("SYNC"))), _ => false));
        Assert.True(MayParkAnalysis.IsBlockPoint(new ScreenStmt(
            new NumberExpr(1, Tok("1")), null, null, null, Tok("SCREEN"))));
        Assert.True(MayParkAnalysis.IsBlockPoint(new PointStmt(
            Var(1, "x"), Var(2, "y"), null, Tok("POINT"))));
        Assert.True(MayParkAnalysis.IsBlockPoint(new SpriteCommandStmt(
            new Token(TokenKind.SpriteGetPixels, "GETPIXELS", null, P), [], Tok("SPRITE"))));
        var pointCall = new BuiltinFunctionCallExpr(
            Tok("GRAPHICS"), new Token(TokenKind.Point, "POINT", null, P),
            [Var(1, "x"), Var(2, "y")], new DoubleType());
        Assert.True(MayParkAnalysis.IsLocalMayPark(
            Def(10, new LetStmt([new Assignment(TargetV(3, "c"), pointCall)], Tok("LET"))), _ => false));
        var loadInIf = new IfStmt(
            [new IfClause(Var(1, "cond"), [
                new SpriteCommandStmt(new Token(TokenKind.SpriteLoad, "LOAD", null, P), [], Tok("SPRITE"))
            ], Tok("IF"))],
            null, Tok("IF"));
        Assert.True(MayParkAnalysis.IsLocalMayPark(Def(11, loadInIf), _ => false));
        Assert.True(MayParkAnalysis.NestedParksAreGraphicsOnly(loadInIf, _ => false));
        Assert.False(MayParkAnalysis.NestedParksAreGraphicsOnly(ifStmtWait(), _ => false));

        var loadTex = new LoadTextureStmt(
            TargetV(12, "id"), new StringExpr("sprite.png", Tok("sprite.png")), null, null, Tok("LOAD"));
        Assert.True(MayParkAnalysis.HasGraphicsPark([loadTex]));
        Assert.False(MayParkAnalysis.HasGraphicsPark([Print(3)]));

        static IfStmt ifStmtWait() => new(
            [new IfClause(Var(1, "cond"), [
                new WaitStmt(Var(6, "wg"), Tok("WAIT"))
            ], Tok("IF"))],
            null, Tok("IF"));
    }

    [Fact]
    public void NestedContainers_AreWalked()
    {
        // Blocco dentro un IF: may-park.
        var ifStmt = new IfStmt(
            [new IfClause(Var(1, "cond"), [Recv(10)], Tok("IF"))],
            null, Tok("IF"));
        Assert.True(MayParkAnalysis.IsLocalMayPark(Def(1, ifStmt), _ => false));

        // Blocco nel CATCH di un TRY/CATCH.
        var tryCatch = new TryCatchStmt(
            [Print(3)],
            [new CatchClause(null, null, [Send(10)], Tok("CATCH"))],
            null, Tok("TRY"));
        Assert.True(MayParkAnalysis.IsLocalMayPark(Def(2, tryCatch), _ => false));

        // FOR EACH annidato dentro WITH.
        var forEach = new ForEachStmt([Tok("item")], null, Var(5, "coll"), [Send(10)], Tok("FOR"));
        var withStmt = new WithStmt(Var(6, "obj"), [forEach], Tok("WITH"));
        Assert.True(MayParkAnalysis.IsLocalMayPark(Def(3, withStmt), _ => false));
    }

    [Fact]
    public void CallGraphPropagation_FixedPoint_Works()
    {
        // P1 bloccante (RECEIVE esplicito); P2 chiama P1; P3 chiama P2 → P2 e P3 sono
        // may-park transitivamente (propagazione lungo il call graph), P3 il più lontano.
        var p1 = Def(1, Recv(10));
        var p2 = Def(2, new ExpressionStmt(Call(1)));
        var p3 = Def(3, new ExpressionStmt(Call(2)));
        var procs = ImmutableArray.Create(p1, p2, p3);
        var byId = (int id) => procs.FirstOrDefault(p => p.SymbolId == id);

        var result = MayParkAnalysis.ComputeMayPark(procs, byId);

        Assert.Contains(1, result);
        Assert.Contains(2, result);
        Assert.Contains(3, result);
    }

    [Fact]
    public void MutualRecursion_DoesNotEscape_FixedPoint()
    {
        // P1 e P2 si chiamano a vicenda, nessuno ha blocchi diretti → nessun may-park.
        var p1 = Def(1, new ExpressionStmt(Call(2)));
        var p2 = Def(2, new ExpressionStmt(Call(1)));
        var procs = ImmutableArray.Create(p1, p2);
        var byId = (int id) => procs.FirstOrDefault(p => p.SymbolId == id);
        Assert.Empty(MayParkAnalysis.ComputeMayPark(procs, byId));
    }

    [Fact]
    public void MutualRecursion_WithBlock_Propagates()
    {
        // P1 chiama P2 (che ha un RECEIVE) → P1 e P2 sono may-park.
        var p1 = Def(1, new ExpressionStmt(Call(2)), Print(3));
        var p2 = Def(2, Recv(10), new ExpressionStmt(Call(1)));
        var procs = ImmutableArray.Create(p1, p2);
        var byId = (int id) => procs.FirstOrDefault(p => p.SymbolId == id);
        var result = MayParkAnalysis.ComputeMayPark(procs, byId);
        Assert.Contains(1, result);
        Assert.Contains(2, result);
    }

}
