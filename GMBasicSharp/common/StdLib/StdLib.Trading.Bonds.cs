// FILE: Interpreter.StdLib.Trading.Bonds.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Trading.Bonds — namespace 'TRADING.BONDS'
 * 9 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (9):
 *   BondEquivalentYield
 *   BondPrice
 *   BondYield
 *   CurrentYield
 *   DURATION
 *   PRICESIMPLE
 *   YieldToMaturity
 *   ZeroCouponBondValue
 *   ZeroCouponBondYield
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- TRADING.BONDS NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'TRADING.BONDS'.
    /// Contiene funzioni specifiche per la valutazione di obbligazioni.
    /// </summary>
    private void CreateTradingBondsNamespace()
    {
        const string namespaceName = "TRADING.BONDS";
        const SymbolFlags publicFlag = SymbolFlags.IsPublic;
        const SymbolFlags noFlags = SymbolFlags.None;

        // --- Definizione dei parametri comuni per le funzioni obbligazionarie ---
        var settlement = new Parameter(-1, T(TokenKind.Identifier, "settlement"), new DateTimeType(), noFlags, null, false);
        var maturity = new Parameter(-1, T(TokenKind.Identifier, "maturity"), new DateTimeType(), noFlags, null, false);
        var rate = new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), noFlags, null, false);
        var yld = new Parameter(-1, T(TokenKind.Identifier, "yld"), new DoubleType(), noFlags, null, false);
        var redemption = new Parameter(-1, T(TokenKind.Identifier, "redemption"), new DoubleType(), noFlags, null, false);
        var frequency = new Parameter(-1, T(TokenKind.Identifier, "frequency"), new IntegerType(), noFlags, null, false);
        var price = new Parameter(-1, T(TokenKind.Identifier, "price"), new DoubleType(), noFlags, null, false);
        var basis = new Parameter(-1, T(TokenKind.Identifier, "basis"), new IntegerType(), noFlags, null, false);
        var calcMethod = new Parameter(-1, T(TokenKind.Identifier, "calcMethod"), new BitType(), noFlags, null, false);
        var model = new Parameter(-1, T(TokenKind.Identifier, "model"), new IntegerType(), SymbolFlags.IsOptional, null, false);

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.BondPrice, "BondPrice"),
                [settlement, maturity, rate, yld, redemption, frequency, model], new DoubleType(),
                TokenKind.BondPrice, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.BondYield, "BondYield"),
                [settlement, maturity, rate, price, redemption, frequency, model], new DoubleType(),
                TokenKind.BondYield, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Duration, "DURATION"),
                [settlement, maturity, rate, yld, frequency, basis, calcMethod], new DoubleType(),
                TokenKind.Duration),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.BondEquivalentYield, "BondEquivalentYield"), [new Parameter(-1, T(TokenKind.Identifier, "faceValue"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "bondPrice"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "daysToMaturity"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.BondEquivalentYield, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.CurrentYield, "CurrentYield"), [new Parameter(-1, T(TokenKind.Identifier, "annualCoupons"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "bondPrice"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.CurrentYield, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.YieldToMaturity, "YieldToMaturity"), [new Parameter(-1, T(TokenKind.Identifier, "couponPayment"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "faceValue"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "price"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "years"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.YieldToMaturity, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ZeroCouponBondValue, "ZeroCouponBondValue"), [new Parameter(-1, T(TokenKind.Identifier, "faceValue"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "time"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.ZeroCouponBondValue, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ZeroCouponBondYield, "ZeroCouponBondYield"), [new Parameter(-1, T(TokenKind.Identifier, "faceValue"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "price"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "time"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.ZeroCouponBondYield, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PriceSimple, "PRICESIMPLE"),
                [settlement, maturity, rate, yld, redemption, frequency], new DoubleType(),
                TokenKind.PriceSimple, ExecutionKind: NativeExecutionKind.Pure)
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}