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
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Mean, "Mean"), [dataArrayParam], new DoubleType(), TokenKind.Mean),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Median, "Median"), [dataArrayParam], new DoubleType(), TokenKind.Median),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Variance, "Variance"), [dataArrayParam], new DoubleType(), TokenKind.Variance),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StdDev, "StdDev"), [dataArrayParam], new DoubleType(), TokenKind.StdDev),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Skewness, "Skewness"), [dataArrayParam], new DoubleType(), TokenKind.Skewness),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Kurtosis, "Kurtosis"), [dataArrayParam], new DoubleType(), TokenKind.Kurtosis),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Correlation, "Correlation"), [dataArrayParam, dataArray2Param], new DoubleType(), TokenKind.Correlation),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsCovariance, "Covariance"),
                [dataArrayParam, dataArray2Param], new DoubleType(), TokenKind.StatsCovariance),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsRange, "RANGE"),
                [dataArrayParam], new DoubleType(), TokenKind.StatsRange),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsMax, "MAX"),
                [dataArrayParam], new DoubleType(), TokenKind.StatsMax),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsMin, "MIN"),
                [dataArrayParam], new DoubleType(), TokenKind.StatsMin),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Percentile, "Percentile"),
            [
                dataArrayParam, new Parameter(-1, T(TokenKind.Identifier, "p"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Percentile),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Mad, "Mad"), [dataArrayParam], new DoubleType(), TokenKind.Mad),

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
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ChiPdf, "ChiPdf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "k"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.ChiPdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ChiCdf, "ChiCdf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "k"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.ChiCdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ChiSquaredPdf, "ChiSquaredPdf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "k"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.ChiSquaredPdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ChiSquaredCdf, "ChiSquaredCdf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "k"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.ChiSquaredCdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsChiSquaredSf, "ChiSquaredSf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "k"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsChiSquaredSf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.BetaPdf, "BetaPdf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "alpha"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "beta"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.BetaPdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.BetaCdf, "BetaCdf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "alpha"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "beta"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.BetaCdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StudentTPdf, "StudentTPdf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "nu"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StudentTPdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StudentTCdf, "StudentTCdf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "nu"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StudentTCdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsStudentTSf, "StudentTSf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "nu"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsStudentTSf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.WeibullPdf, "WeibullPdf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "shape"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.WeibullPdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.WeibullCdf, "WeibullCdf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "shape"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.WeibullCdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsWeibullSf, "WeibullSf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "shape"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.StatsWeibullSf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.FPdf, "FPdf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "d1"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "d2"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.FPdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.FCdf, "FCdf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "d1"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "d2"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.FCdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsFSf, "FSf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "d1"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "d2"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.StatsFSf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ExponentialPdf, "ExponentialPdf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.ExponentialPdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ExponentialCdf, "ExponentialCdf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.ExponentialCdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsExpSf, "ExponentialSf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsExpSf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ParetoPdf, "ParetoPdf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "shape"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.ParetoPdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsParetoCdf, "ParetoCdf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "shape"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.StatsParetoCdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsParetoSf, "ParetoSf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "shape"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.StatsParetoSf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.LogisticPdf, "LogisticPdf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "location"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.LogisticPdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TriangularPdf, "TriangularPdf"),
            [
                xParam, new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "c"), new DoubleType(), SymbolFlags.IsOptional, null, false)
            ], new DoubleType(), TokenKind.TriangularPdf, ExecutionKind: NativeExecutionKind.Pure),
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
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ChiSquaredPpf, "ChiSquaredPpf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "k"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.ChiSquaredPpf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ChiSquaredIsf, "ChiSquaredIsf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "k"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.ChiSquaredIsf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ChiPpf, "ChiPpf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "k"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.ChiPpf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.BetaPpf, "BetaPpf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "alpha"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "beta"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.BetaPpf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.FPpf, "FPpf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "d1"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "d2"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.FPpf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.FIsf, "FIsf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "d1"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "d2"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.FIsf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StudentTPpf, "StudentTPpf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "nu"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StudentTPpf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StudentTIsf, "StudentTIsf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "nu"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StudentTIsf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ExponentialPpf, "ExponentialPpf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.ExponentialPpf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ExponentialIsf, "ExponentialIsf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.ExponentialIsf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.WeibullPpf, "WeibullPpf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "shape"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.WeibullPpf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.LogisticPpf, "LogisticPpf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "location"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.LogisticPpf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.UniformPpf, "UniformPpf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), SymbolFlags.IsOptional, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), SymbolFlags.IsOptional, null, false)],
                new DoubleType(), TokenKind.UniformPpf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsUniformCdf, "UniformCdf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsUniformCdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsUniformSf, "UniformSf"),
                [xParam, new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsUniformSf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ParetoPpf, "ParetoPpf"),
                [pParam, new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "shape"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.ParetoPpf, ExecutionKind: NativeExecutionKind.Pure),
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
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsGammaPdf, "GammaPdf"),
                [new Parameter(-1, T(TokenKind.Identifier, "shape"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsGammaPdf),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsGammaCdf, "GammaCdf"),
                [new Parameter(-1, T(TokenKind.Identifier, "shape"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsGammaCdf),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsGammaSf, "GammaSf"),
                [new Parameter(-1, T(TokenKind.Identifier, "shape"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsGammaSf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsGammaPpf, "GammaPpf"),
                [new Parameter(-1, T(TokenKind.Identifier, "shape"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "p"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsGammaPpf),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsGammaIsf, "GammaIsf"),
                [new Parameter(-1, T(TokenKind.Identifier, "shape"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "scale"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "p"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.StatsGammaIsf),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StatsSkewTest, "SkewTest"),
                [dataArrayParam], new AnyType(), TokenKind.StatsSkewTest, ExecutionKind: NativeExecutionKind.Pure),
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
                [dataArrayParam], new AnyType(), TokenKind.StatsDescribe, ExecutionKind: NativeExecutionKind.Pure)
        };
        
        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "AnovaResult", new AnovaResultType() }
        };
        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
