// FILE: Interpreter.StdLib.Rdd.cs

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Rdd — namespace 'RDD'
 * 22 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (22):
 *   BOF
 *   BUFFER
 *   CTOD
 *   CURVAL
 *   DELETE
 *   DTOC
 *   EOF
 *   FLOAT
 *   FLOCK
 *   FOUND
 *   GETFIELD
 *   GETFLDSTATE
 *   OLDVAL
 *   RECALL
 *   RECNO
 *   REPLACE
 *   RLOCK
 *   SETDELETED
 *   TABLEREVERT
 *   TABLEUPDATE
 *   TODAY
 *   VALTYPE
    ==== GMB-STDLIB-AUDIT-END ==== */

using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateRddNamespace()
    {
        const string ns = "RDD";
        var f = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        var Tk = (TokenKind k, string n) => new Token(k, n, null, SourcePosition.Unknown);
        var P = (string n, DataType t) => new Parameter(-1, Tk(TokenKind.Identifier, n), t, SymbolFlags.None, null, false);

        // Comandi RDD (sintassi spazio: RDD SET DEFAULT "ISAM", RDD SET FIELD 1,2,3, ...).
        // Le NativeProcedure VOID per questi comandi sono state RIMOSSE: un comando non è
        // una funzione, quindi non deve avere né ritorno né forma con parentesi.
        // Il dispatch legacy passa dal switch in ParseStatement (case TokenKind.Rdd),
        // come ISAM — NON dal registry (RegisterCommand intercetterebbe anche RDD.xxx).

        var procs = new ProcedureDefinition[] {
            new NativeProcedureDefinition(-1, f, Tk(TokenKind.RddGetField, "GETFIELD"), [P("area",new LongType()), P("field",new LongType())], new AnyType(), TokenKind.RddGetField),
            // Cursore (driver-agnostico: dispatcha all'area corrente come DBF).
            new NativeProcedureDefinition(-1, f, Tk(TokenKind.EofFunc, "EOF"), [], new BitType(), TokenKind.EofFunc),
            new NativeProcedureDefinition(-1, f, Tk(TokenKind.Bof, "BOF"), [], new BitType(), TokenKind.Bof),
            new NativeProcedureDefinition(-1, f, Tk(TokenKind.Found, "FOUND"), [], new BitType(), TokenKind.Found),
            new NativeProcedureDefinition(-1, f, Tk(TokenKind.Recno, "RECNO"), [], new LongType(), TokenKind.Recno),
            // SETDELETED: nome non keyword (lexa Identifier) — il dispatch passa da
            // TryEmitDatabaseSyscall("RDD","SETDELETED") che ha GIÀ il case; il BuiltinKind
            // Identifier non raggiunge mai la BuiltInFunctionMap (prima c'è il syscall).
            new NativeProcedureDefinition(-1, f, Tk(TokenKind.RddSetDeleted, "SETDELETED"), [P("area",new LongType()), P("flag",new LongType())], new BitType(), TokenKind.RddSetDeleted),

            new NativeProcedureDefinition(-1, f, Tk(TokenKind.RddFLock, "FLOCK"), [P("area",new LongType())], new BitType(), TokenKind.RddFLock),
            new NativeProcedureDefinition(-1, f, Tk(TokenKind.RddRLock, "RLOCK"), [P("area",new LongType())], new BitType(), TokenKind.RddRLock),







            new NativeProcedureDefinition(-1, f, Tk(TokenKind.RddToday, "TODAY"), [], new DateType(), TokenKind.RddToday),
            new NativeProcedureDefinition(-1, f, Tk(TokenKind.RddCurval, "CURVAL"), [P("value",new DoubleType())], new DoubleType(), TokenKind.RddCurval),
            new NativeProcedureDefinition(-1, f, Tk(TokenKind.RddCtod, "CTOD"), [P("date",new StringType())], new DateType(), TokenKind.RddCtod),
            new NativeProcedureDefinition(-1, f, Tk(TokenKind.RddDtoc, "DTOC"), [P("date",new DateType())], new AnyType(), TokenKind.RddDtoc),
            new NativeProcedureDefinition(-1, f, Tk(TokenKind.RddBuffer, "BUFFER"), [P("area",new LongType()), P("mode",new LongType())], new BitType(), TokenKind.RddBuffer),
            new NativeProcedureDefinition(-1, f, Tk(TokenKind.RddTableRevert, "TABLEREVERT"), [P("area",new LongType())], new BitType(), TokenKind.RddTableRevert),
            new NativeProcedureDefinition(-1, f, Tk(TokenKind.RddTableUpdate, "TABLEUPDATE"), [P("area",new LongType())], new BitType(), TokenKind.RddTableUpdate),
            new NativeProcedureDefinition(-1, f, Tk(TokenKind.RddOldVal, "OLDVAL"), [P("area",new LongType()), P("field",new LongType())], new AnyType(), TokenKind.RddOldVal),
            new NativeProcedureDefinition(-1, f, Tk(TokenKind.RddGetFieldState, "GETFLDSTATE"), [P("area",new LongType()), P("field",new LongType())], new LongType(), TokenKind.RddGetFieldState),
            new NativeProcedureDefinition(-1, f, Tk(TokenKind.RddValType, "VALTYPE"), [P("area",new LongType())], new StringType(), TokenKind.RddValType),






            new NativeProcedureDefinition(-1, f, Tk(TokenKind.RddFloat, "FLOAT"), [P("expr",new AnyType())], new DoubleType(), TokenKind.RddFloat),
            new NativeProcedureDefinition(-1, f, Tk(TokenKind.RddDeleteScoped, "DELETE"), [P("scope",new StringType()), P("expr",new AnyType())], new LongType(), TokenKind.RddDeleteScoped),
            new NativeProcedureDefinition(-1, f, Tk(TokenKind.RddRecallScoped, "RECALL"), [P("scope",new StringType()), P("expr",new AnyType())], new LongType(), TokenKind.RddRecallScoped),
            new NativeProcedureDefinition(-1, f, Tk(TokenKind.RddReplaceScoped, "REPLACE"), [P("scope",new StringType()), P("field",new StringType()), P("value",new AnyType()), P("expr",new AnyType())], new LongType(), TokenKind.RddReplaceScoped),
        };
        RegisterBuiltinNamespace(ns, procs, ImmutableDictionary<string, DataType>.Empty);
    }
}
