// FILE: Interpreter.StdLib.Stats.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Stats — namespace 'STATS'
 * 83 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (83):
 *   Anova
 *   BernoulliPmf
 *   BetaCdf
 *   BetaPdf
 *   BetaPpf
 *   BinomialPmf
 *   Chi2
 *   Chi2Con
 *   ChiCdf
 *   ChiPdf
 *   ChiPpf
 *   ChiSquaredCdf
 *   ChiSquaredIsf
 *   ChiSquaredPdf
 *   ChiSquaredPpf
 *   Correlation
 *   Describe
 *   ExponentialCdf
 *   ExponentialIsf
 *   ExponentialPdf
 *   ExponentialPpf
 *   FCdf
 *   FIsf
 *   FPdf
 *   FPpf
 *   Fisher
 *   GammaCdf
 *   GammaIsf
 *   GammaPdf
 *   GammaPpf
 *   GeometricPmf
 *   HypergeometricPmf
 *   Iqr
 *   JarqueBera
 *   Ks
 *   Kurtosis
 *   KurtosisTest
 *   LinRegress
 *   LogisticPdf
 *   LogisticPpf
 *   LognormalCdf
 *   LognormalPdf
 *   LognormalPpf
 *   Mad
 *   MannWhitney
 *   Mean
 *   Median
 *   Mode
 *   Moment
 *   NegativeBinomialPmf
 *   NormalCdf
 *   NormalIsf
 *   NormalPdf
 *   NormalPpf
 *   NormalTest
 *   ParetoPdf
 *   ParetoPpf
 *   PearsonR
 *   Percentile
 *   PoissonPmf
 *   Shapiro
 *   SkewTest
 *   SkewedNormalPdf
 *   Skewness
 *   SpearmanR
 *   StdDev
 *   StudentTCdf
 *   StudentTIsf
 *   StudentTPdf
 *   StudentTPpf
 *   TTest
 *   TTestOneSample
 *   TTestPaired
 *   TriangularPdf
 *   TriangularPpf
 *   UniformPdf
 *   UniformPpf
 *   Variance
 *   WeibullCdf
 *   WeibullPdf
 *   WeibullPpf
 *   Wilcoxon
 *   ZTest
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- STATS NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'STATS' (Statistiche Descrittive e Distribuzioni).
    /// </summary>
    private void CreateStatsNamespace()
    {
        const string namespaceName = "STATS";
        var publicFlag = SymbolFlags.IsPublic;
        var noFlags = SymbolFlags.None;
        var arrayFlag = SymbolFlags.IsArray;

        var dataArrayParam = new Parameter(-1, T(TokenKind.Identifier, "data"), new ArrayType(new DoubleType()), arrayFlag, null, false);
        var dataArray2Param = new Parameter(-1, T(TokenKind.Identifier, "data2"), new ArrayType(new DoubleType()), arrayFlag, null, false);
        var xParam = new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false);
        var pParam = new Parameter(-1, T(TokenKind.Identifier, "p"), new DoubleType(), noFlags, null, false);

        var procedures = new ProcedureDefinition[]
        {
            // Descrittive

            // Distribuzioni Continue
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.NormalPdf, "NormalPdf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "mu"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "sigma"), new DoubleType(), SymbolFlags.IsOptional, null, false)
            ], new DoubleType(), TokenKind.NormalPdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.NormalCdf, "NormalCdf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "mu"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "sigma"), new DoubleType(), SymbolFlags.IsOptional, null, false)
            ], new DoubleType(), TokenKind.NormalCdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsNormalSf, "NormalSf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "mu"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "sigma"), new DoubleType(), SymbolFlags.IsOptional, null, false)
            ], new DoubleType(), TokenKind.StatsNormalSf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.LognormalPdf, "LognormalPdf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "mu"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "sigma"), new DoubleType(), SymbolFlags.IsOptional, null, false)
            ], new DoubleType(), TokenKind.LognormalPdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.LognormalCdf, "LognormalCdf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "mu"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "sigma"), new DoubleType(), SymbolFlags.IsOptional, null, false)
            ], new DoubleType(), TokenKind.LognormalCdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsLognormalSf, "LognormalSf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "mu"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "sigma"), new DoubleType(), SymbolFlags.IsOptional, null, false)
            ], new DoubleType(), TokenKind.StatsLognormalSf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsNcBetaSf, "NcBetaSf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "nc"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsNcBetaSf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsNcGammaSf, "NcGammaSf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "shape"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "nc"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsNcGammaSf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsNcFPdf, "NcFPdf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "d1"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "d2"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "nc"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsNcFPdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsNcFCdf, "NcFCdf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "d1"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "d2"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "nc"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsNcFCdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsNcFSf, "NcFSf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "d1"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "d2"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "nc"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsNcFSf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsNcTPdf, "NcTPdf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "df"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "nc"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsNcTPdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsNcTCdf, "NcTCdf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "df"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "nc"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsNcTCdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsNcTSf, "NcTSf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "df"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "nc"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsNcTSf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsZTPoissonPmf, "ZTPoissonPmf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "lambda"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsZTPoissonPmf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsZTPoissonCdf, "ZTPoissonCdf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "lambda"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsZTPoissonCdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsNcFPpf, "NcFPpf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "d1"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "d2"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "nc"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsNcFPpf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsNcFIsf, "NcFIsf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "d1"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "d2"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "nc"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsNcFIsf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsNcTPpf, "NcTPpf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "df"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "nc"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsNcTPpf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsNcTIsf, "NcTIsf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "df"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "nc"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsNcTIsf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsNcChi2Sf, "NcChi2Sf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "df"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "nc"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsNcChi2Sf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsLogisticCdf, "LogisticCdf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "location"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), SymbolFlags.IsOptional, null, false)
            ], new DoubleType(), TokenKind.StatsLogisticCdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsLogisticSf, "LogisticSf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "location"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), SymbolFlags.IsOptional, null, false)
            ], new DoubleType(), TokenKind.StatsLogisticSf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TriangularPdf, "TriangularPdf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "c"), new DoubleType(), SymbolFlags.IsOptional, null, false)
            ], new DoubleType(), TokenKind.TriangularPdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsTriangularCdf, "TriangularCdf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "min"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "mode"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "max"), new DoubleType(), SymbolFlags.IsOptional, null, false)
            ], new DoubleType(), TokenKind.StatsTriangularCdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsTriangularSf, "TriangularSf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "min"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "mode"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "max"), new DoubleType(), SymbolFlags.IsOptional, null, false)
            ], new DoubleType(), TokenKind.StatsTriangularSf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.UniformPdf, "UniformPdf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), SymbolFlags.IsOptional, null, false)
            ], new DoubleType(), TokenKind.UniformPdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SkewedNormalPdf, "SkewedNormalPdf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "location"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "alpha"), new DoubleType(), SymbolFlags.IsOptional, null, false)
            ], new DoubleType(), TokenKind.SkewedNormalPdf, ExecutionKind: NativeExecutionKind.Pure),

            // Inverse CDF (ppf) e ISF — quantili e valori critici (scipy.stats)
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.NormalPpf, "NormalPpf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "mu"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "sigma"), new DoubleType(), SymbolFlags.IsOptional, null, false)],
                new DoubleType(), TokenKind.NormalPpf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.NormalIsf, "NormalIsf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "mu"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "sigma"), new DoubleType(), SymbolFlags.IsOptional, null, false)],
                new DoubleType(), TokenKind.NormalIsf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.LognormalPpf, "LognormalPpf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "mu"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "sigma"), new DoubleType(), SymbolFlags.IsOptional, null, false)],
                new DoubleType(), TokenKind.LognormalPpf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsChiIsf, "ChiIsf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "k"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsChiIsf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsBetaIsf, "BetaIsf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsBetaIsf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsLogisticIsf, "LogisticIsf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "location"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsLogisticIsf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsLognormalIsf, "LognormalIsf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "mu"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "sigma"), new DoubleType(), SymbolFlags.IsOptional, null, false)],
                new DoubleType(), TokenKind.StatsLognormalIsf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsTriangularIsf, "TriangularIsf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "min"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "mode"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "max"), new DoubleType(), SymbolFlags.IsOptional, null, false)],
                new DoubleType(), TokenKind.StatsTriangularIsf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsWeibullIsf, "WeibullIsf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "shape"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsWeibullIsf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.UniformPpf, "UniformPpf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), SymbolFlags.IsOptional, null, false)],
                new DoubleType(), TokenKind.UniformPpf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsUniformIsf, "UniformIsf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), SymbolFlags.IsOptional, null, false)],
                new DoubleType(), TokenKind.StatsUniformIsf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsParetoIsf, "ParetoIsf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "shape"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsParetoIsf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TriangularPpf, "TriangularPpf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "c"), new DoubleType(), SymbolFlags.IsOptional, null, false)],
                new DoubleType(), TokenKind.TriangularPpf, ExecutionKind: NativeExecutionKind.Pure),

            // Distribuzioni Discrete
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.BernoulliPmf, "BernoulliPmf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "p"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.BernoulliPmf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.BinomialPmf, "BinomialPmf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "n"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "p"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.BinomialPmf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PoissonPmf, "PoissonPmf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "lambda"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.PoissonPmf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.GeometricPmf, "GeometricPmf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "p"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.GeometricPmf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.NegativeBinomialPmf, "NegativeBinomialPmf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "r"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "p"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.NegativeBinomialPmf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.HypergeometricPmf, "HypergeometricPmf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "N"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "K"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "n"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.HypergeometricPmf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TTest, "TTest"),
                [dataArrayParam, dataArray2Param], new AnyType(), TokenKind.TTest, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TTestPaired, "TTestPaired"),
                [dataArrayParam, dataArray2Param], new AnyType(), TokenKind.TTestPaired, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TTestOneSample, "TTestOneSample"),
                [dataArrayParam, new Parameter(-1, T(TokenKind.Identifier, "mu"), new DoubleType(), noFlags, null, false)],
                new AnyType(), TokenKind.TTestOneSample, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsAnova, "Anova"),
                [new Parameter(-1, T(TokenKind.Identifier, "groups"), new ArrayType(new DoubleType()), arrayFlag, null, false)],
                new AnyType(), TokenKind.StatsAnova, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Chi2Test, "Chi2"),
                [dataArrayParam, dataArray2Param], new AnyType(), TokenKind.Chi2Test, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.FisherExact, "Fisher"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "a"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "c"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "d"), new IntegerType(), noFlags, null, false)
            ], new AnyType(), TokenKind.FisherExact, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ShapiroWilk, "Shapiro"),
                [dataArrayParam], new AnyType(), TokenKind.ShapiroWilk, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.NormalTest, "NormalTest"),
                [dataArrayParam], new AnyType(), TokenKind.NormalTest, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ZTest, "ZTest"),
            [
                dataArrayParam,
                new Parameter(-1, T(TokenKind.Identifier, "mu"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "sigma"), new DoubleType(), noFlags, null, false)
            ], new AnyType(), TokenKind.ZTest, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.KolmogorovSmirnov, "Ks"),
                [dataArrayParam, dataArray2Param], new AnyType(), TokenKind.KolmogorovSmirnov,
                ExecutionKind: NativeExecutionKind.Pure),

            // --- scipy.stats P0: correlazione, test, Gamma, descrittive ---
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsKendallTau, "KendallTau"),
                [dataArrayParam, dataArray2Param], new AnyType(), TokenKind.StatsKendallTau),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsFTest, "FTest"),
                [dataArrayParam, dataArray2Param], new AnyType(), TokenKind.StatsFTest),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsKruskal, "Kruskal"),
                [new Parameter(-1, T(TokenKind.Identifier, "groups"), new ArrayType(new ArrayType(new DoubleType())), SymbolFlags.IsArray, null, false)],
                new AnyType(), TokenKind.StatsKruskal),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsPearsonR, "PearsonR"),
                [dataArrayParam, dataArray2Param], new AnyType(), TokenKind.StatsPearsonR,
                ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsSpearmanR, "SpearmanR"),
                [dataArrayParam, dataArray2Param], new AnyType(), TokenKind.StatsSpearmanR,
                ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsLinRegress, "LinRegress"),
                [dataArrayParam, dataArray2Param], new AnyType(), TokenKind.StatsLinRegress,
                ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsChi2Contingency, "Chi2Con"),
                [new Parameter(-1, T(TokenKind.Identifier, "table"), new ArrayType(dataArrayParam.DataType), arrayFlag, null, false)],
                new AnyType(), TokenKind.StatsChi2Contingency, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsMannWhitney, "MannWhitney"),
                [dataArrayParam, dataArray2Param], new AnyType(), TokenKind.StatsMannWhitney,
                ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsWilcoxon, "Wilcoxon"),
                [dataArrayParam, dataArray2Param], new AnyType(), TokenKind.StatsWilcoxon,
                ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsKurtosisTest, "KurtosisTest"),
                [dataArrayParam], new AnyType(), TokenKind.StatsKurtosisTest, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsJarqueBera, "JarqueBera"),
                [dataArrayParam], new AnyType(), TokenKind.StatsJarqueBera, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsMode, "Mode"),
                [dataArrayParam], new DoubleType(), TokenKind.StatsMode),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsMoment, "Moment"),
                [dataArrayParam, new Parameter(-1, T(TokenKind.Identifier, "order"), new IntegerType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsMoment),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsIqr, "Iqr"),
                [dataArrayParam], new DoubleType(), TokenKind.StatsIqr),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsDescribe, "Describe"),
                [dataArrayParam], new AnyType(), TokenKind.StatsDescribe, ExecutionKind: NativeExecutionKind.Pure),
            // REGRESS: ex REGRESSION.Regress (namespace eliminato 2026-11-08). Regressione
            // lineare ONE-PASS (CommonLib.Numerics.LinearRegression) → {Slope, Intercept, R2}.
            // NON è un alias di LinRegress (scipy, due passate: R/PValue/Stderr; ~1 ULP di
            // differenza sull'intercetta).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Regress, "Regress"),
                [dataArrayParam, dataArray2Param], new RegressionResultType(), TokenKind.Regress,
                ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsSkewTest, "SkewTest"),
                [dataArrayParam], new AnyType(), TokenKind.StatsSkewTest, ExecutionKind: NativeExecutionKind.Pure)
        };
        
        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "AnovaResult", new AnovaResultType() },
            { "RegressionResult", new RegressionResultType() }
        };
        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
