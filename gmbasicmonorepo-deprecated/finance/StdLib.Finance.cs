// FILE: Interpreter.StdLib.Finance.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Finance — namespace 'FINANCE'
 * 23 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (23):
 *   CUMIPMT
 *   CUMPRINC
 *   DAYS360
 *   DB
 *   DDB
 *   FV
 *   FVEXT
 *   IRR
 *   MIRR
 *   MNPV
 *   NPER
 *   NPEREXT
 *   NPV
 *   PMT
 *   PMTEXT
 *   PV
 *   PVEXT
 *   RATE
 *   SLN
 *   SYD
 *   XIRR
 *   XNPV
 *   YEARFRAC
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- FINANCE NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'FINANCE'.
    /// Include le funzioni finanziarie principali implementate nel core.
    /// </summary>
    private void CreateFinanceNamespace()
    {
        const string namespaceName = "FINANCE";
        const SymbolFlags publicFlag = SymbolFlags.IsPublic;
        const SymbolFlags noFlags = SymbolFlags.None;
        const SymbolFlags optionalFlag = SymbolFlags.IsOptional;
        const SymbolFlags arrayFlag = SymbolFlags.IsArray;
        const SymbolFlags paramArrayFlag = SymbolFlags.IsParamArray;

        var doubleArrayType = new ArrayType(new DoubleType());
        var dateArrayType = new ArrayType(new DateTimeType());

        var procedures = new ProcedureDefinition[]
        {
            // Funzioni di base per prestiti e investimenti
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Pmt, "PMT"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "nper"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "pv"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "fv"), new DoubleType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "type"), new IntegerType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.Pmt),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Fv, "FV"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "nper"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "pmt"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "pv"), new DoubleType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "type"), new IntegerType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.Fv),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Pv, "PV"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "nper"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "pmt"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "fv"), new DoubleType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "type"), new IntegerType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.Pv),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Npv, "NPV"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), noFlags, null, false),
                // CORREZIONE: Il tipo deve essere ArrayType, non DoubleType, perché è un paramArray.
                new Parameter(-1, T(TokenKind.Identifier, "values"), doubleArrayType,
                    arrayFlag | paramArrayFlag, null, false)
            ], new DoubleType(), TokenKind.Npv),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Irr, "IRR"),
            [
                // CORREZIONE: Il tipo deve essere ArrayType.
                new Parameter(-1, T(TokenKind.Identifier, "values"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "guess"), new DoubleType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.Irr),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Rate, "RATE"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "nper"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "pmt"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "pv"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "fv"), new DoubleType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "type"), new IntegerType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "guess"), new DoubleType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.Rate),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Nper, "NPER"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "pmt"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "pv"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "fv"), new DoubleType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "type"), new IntegerType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.Nper),

            // Funzioni cumulative
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Cumipmt, "CUMIPMT"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "nper"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "pv"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "startPeriod"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "endPeriod"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "type"), new IntegerType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Cumipmt, ExecutionKind: NativeExecutionKind.Pure),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Cumprinc, "CUMPRINC"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "nper"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "pv"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "startPeriod"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "endPeriod"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "type"), new IntegerType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Cumprinc, ExecutionKind: NativeExecutionKind.Pure),

            // Funzioni di ammortamento
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Sln, "SLN"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "cost"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "salvage"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "life"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Sln),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Syd, "SYD"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "cost"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "salvage"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "life"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "per"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Syd),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Db, "DB"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "cost"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "salvage"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "life"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "period"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "month"), new IntegerType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Db, ExecutionKind: NativeExecutionKind.Pure),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Ddb, "DDB"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "cost"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "salvage"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "life"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "period"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "factor"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Ddb),

            // Funzioni per obbligazioni

            // Funzioni di data
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.YearFrac, "YEARFRAC"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "start"), new DateTimeType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "end"), new DateTimeType(), noFlags, null, false),
                // Il runtime (IlRuntime.YearFrac / Numerics.YearFrac) prende DOUBLE: dichiarare
                // IntegerType faceva iniettare al Linker un CINT(0) avvolto, e la sequenza
                // EmitDoubleArgument(CINT) → ToInt16+Conv_R_Un → mismatch di stack nel metodo
                // che segue un'altra chiamata data (YearFrac+Days360) → InvalidProgram.
                new Parameter(-1, T(TokenKind.Identifier, "basis"), new DoubleType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.YearFrac),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Days360, "DAYS360"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "start"), new DateTimeType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "end"), new DateTimeType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "european"), new BitType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.Days360),

            // Funzioni avanzate
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Mirr, "MIRR"),
            [
                // CORREZIONE: Il tipo deve essere ArrayType.
                new Parameter(-1, T(TokenKind.Identifier, "values"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "financeRate"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "reinvestRate"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Mirr),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Xnpv, "XNPV"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), noFlags, null, false),
                // CORREZIONE: Il tipo deve essere ArrayType.
                new Parameter(-1, T(TokenKind.Identifier, "values"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "dates"), dateArrayType, arrayFlag, null, false)
            ], new DoubleType(), TokenKind.Xnpv),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Xirr, "XIRR"),
            [
                // CORREZIONE: Il tipo deve essere ArrayType.
                new Parameter(-1, T(TokenKind.Identifier, "values"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "dates"), dateArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "guess"), new DoubleType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.Xirr, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Mnpv, "MNPV"),
            [new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), SymbolFlags.None, null, false),
             new Parameter(-1, T(TokenKind.Identifier, "cashFlows"), new ArrayType(new DoubleType()), SymbolFlags.None, null, false)
            ], new DoubleType(), TokenKind.Mnpv),
            // FINANCE varianti (2026-08-06): PMTEXT/FVEXT/PVEXT/NPEREXT/YIELD/PRICESIMPLE
            // (implementazioni in CommonLib.Numerics — non erano esposte nel BASIC).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PmtExt, "PMTEXT"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "nper"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "pv"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "fv"), new DoubleType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "type"), new IntegerType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.PmtExt),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.FvExt, "FVEXT"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "nper"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "pmt"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "pv"), new DoubleType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "type"), new IntegerType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.FvExt),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PvExt, "PVEXT"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "nper"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "pmt"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "fv"), new DoubleType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "type"), new IntegerType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.PvExt),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.NperExt, "NPEREXT"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "pmt"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "pv"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "fv"), new DoubleType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "type"), new IntegerType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.NperExt),

        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}