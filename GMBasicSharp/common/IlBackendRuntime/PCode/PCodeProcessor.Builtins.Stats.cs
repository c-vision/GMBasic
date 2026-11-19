// FILE: Basic.IlBackend.PCode.PCodeProcessor.Builtins.cs
// P-CODE VM: ExecuteBuiltIn — refactored from PCodeProcessor.cs (2026-08-03)

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;
using Microsoft.Data.Sqlite;
using Basic.AI.Rag;
using Basic.Databases.Rdd;
using Basic.IlBackend.PCode.Http;
using CommonLib.Http;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>STATS: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> STATSBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.STATS_PEARSONR,
        BuiltInFunction.STATS_SPEARMANR,
        BuiltInFunction.STATS_LINREGRESS,
        BuiltInFunction.STATS_CHI2CON,
        BuiltInFunction.STATS_MANNWHITNEY,
        BuiltInFunction.STATS_WILCOXON,
        BuiltInFunction.STATS_SKEWTEST,
        BuiltInFunction.STATS_KURTOSISTEST,
        BuiltInFunction.STATS_JARQUEBERA,
        BuiltInFunction.STATS_DESCRIBE,
        BuiltInFunction.STATS_GAMMAPDF,
        BuiltInFunction.STATS_GAMMACDF,
        BuiltInFunction.STATS_GAMMASF,
        BuiltInFunction.STATS_EXPSF,
        BuiltInFunction.STATS_STUDENTSF,
        BuiltInFunction.STATS_CHISQUAREDSF,
        BuiltInFunction.STATS_WEIBULLSF,
        BuiltInFunction.STATS_FSF,
        BuiltInFunction.STATS_NORMALSF,
        BuiltInFunction.STATS_LOGNORMALSF,
        BuiltInFunction.STATS_PARETOCDF,
        BuiltInFunction.STATS_PARETOSF,
        BuiltInFunction.STATS_UNIFORMCDF,
        BuiltInFunction.STATS_UNIFORMSF,
        BuiltInFunction.STATS_KENDALLTAU,
        BuiltInFunction.STATS_FTEST,
        BuiltInFunction.STATS_BETASF,
        BuiltInFunction.STATS_CHISF,
        BuiltInFunction.STATS_LOGISTICCDF,
        BuiltInFunction.STATS_LOGISTICSF,
        BuiltInFunction.STATS_TRIANGULARCDF,
        BuiltInFunction.STATS_TRIANGULARSF,
        BuiltInFunction.STATS_WEIBULLISF,
        BuiltInFunction.STATS_UNIFORMISF,
        BuiltInFunction.STATS_PARETOISF,
        BuiltInFunction.STATS_BETAISF,
        BuiltInFunction.STATS_LOGISTICISF,
        BuiltInFunction.STATS_LOGNORMALISF,
        BuiltInFunction.STATS_TRIANGULARISF,
        BuiltInFunction.STATS_GAMMAPPF,
        BuiltInFunction.STATS_GAMMAISF,
        BuiltInFunction.STATS_MODE,
        BuiltInFunction.STATS_MOMENT,
        BuiltInFunction.STATS_IQR,
        BuiltInFunction.STATS_COVARIANCE,
        BuiltInFunction.STATS_RANGE,
        BuiltInFunction.STATS_MAX,
        BuiltInFunction.STATS_MIN,
        BuiltInFunction.TTEST,
        BuiltInFunction.TTESTPAIRED,
        BuiltInFunction.TTESTONESAMPLE,
        BuiltInFunction.STATSANOVA,
        BuiltInFunction.STATS_KRUSKAL,
        BuiltInFunction.STATS_CHIISF,
        BuiltInFunction.STATS_NCCHI2PDF,
        BuiltInFunction.STATS_NCCHI2CDF,
        BuiltInFunction.STATS_NCCHI2SF,
        BuiltInFunction.STATS_NCBETAPDF,
        BuiltInFunction.STATS_NCBETACDF,
        BuiltInFunction.STATS_NCBETASF,
        BuiltInFunction.STATS_NCGAMMAPDF,
        BuiltInFunction.STATS_NCGAMMACDF,
        BuiltInFunction.STATS_NCGAMMASF,
        BuiltInFunction.STATS_NCFPDF,
        BuiltInFunction.STATS_NCFCDF,
        BuiltInFunction.STATS_NCFSF,
        BuiltInFunction.STATS_NCTPDF,
        BuiltInFunction.STATS_NCTCDF,
        BuiltInFunction.STATS_NCTSF,
        BuiltInFunction.STATS_NCCHI2PPF,
        BuiltInFunction.STATS_NCCHI2ISF,
        BuiltInFunction.STATS_NCBETAPPF,
        BuiltInFunction.STATS_NCBETAISF,
        BuiltInFunction.STATS_NCGAMMAPPF,
        BuiltInFunction.STATS_NCGAMMAISF,
        BuiltInFunction.STATS_NCFPPF,
        BuiltInFunction.STATS_NCFISF,
        BuiltInFunction.STATS_NCTPPF,
        BuiltInFunction.STATS_NCTISF,
        BuiltInFunction.STATS_ZTPOISSONPMF,
        BuiltInFunction.STATS_ZTPOISSONCDF,
        BuiltInFunction.CHI2TEST,
        BuiltInFunction.FISHEREXACT,
        BuiltInFunction.SHAPIROWILK,
        BuiltInFunction.NORMALTEST,
        BuiltInFunction.ZTEST,
        BuiltInFunction.KOLMOGOROVSMIRNOV,

    };

    /// <summary>Stats: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _statsDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> STATSDispatch
        => _statsDispatch ??= BuildStatsDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildStatsDispatch() => new()
    {
        [BuiltInFunction.STATS_PEARSONR] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.PearsonR(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1)));
        },
        [BuiltInFunction.STATS_SPEARMANR] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.SpearmanR(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1)));
        },
        [BuiltInFunction.STATS_LINREGRESS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.LinRegress(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1)));
        },
        [BuiltInFunction.STATS_CHI2CON] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Chi2Con(ToJaggedArr(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_MANNWHITNEY] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MannWhitney(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1)));
        },
        [BuiltInFunction.STATS_WILCOXON] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Wilcoxon(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1)));
        },
        [BuiltInFunction.STATS_SKEWTEST] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.SkewTest(ToDoubleArr(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_KURTOSISTEST] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.KurtosisTest(ToDoubleArr(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_JARQUEBERA] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.JarqueBera(ToDoubleArr(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_DESCRIBE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Describe(ToDoubleArr(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_KENDALLTAU] = (task, args) =>
        {
                return HypothesisResult(CommonLib.Numerics.KendallTau(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1))));
        },
        [BuiltInFunction.STATS_WEIBULLISF] = (task, args) =>
        {
                return CommonLib.Numerics.WeibullIsf(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.STATS_BETAISF] = (task, args) =>
        {
                return CommonLib.Numerics.BetaIsf(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.STATS_LOGISTICISF] = (task, args) =>
        {
                return CommonLib.Numerics.LogisticIsf(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1, 0.0)), SafeDouble(ArgOr(args, 2, 1.0)));
        },
        [BuiltInFunction.STATS_PARETOISF] = (task, args) =>
        {
                return CommonLib.Numerics.ParetoIsf(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.STATS_FTEST] = (task, args) =>
        {
                return HypothesisResult(CommonLib.Numerics.FTest(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1))));
        },
        [BuiltInFunction.STATS_MODE] = (task, args) =>
        {
                return CommonLib.Numerics.Mode(ToDoubleArr(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_MOMENT] = (task, args) =>
        {
                return CommonLib.Numerics.Moment(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.STATS_IQR] = (task, args) =>
        {
                return CommonLib.Numerics.Iqr(ToDoubleArr(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_MAX] = (task, args) =>
        {
                var a = ToDoubleArr(ArgOr(args, 0));
                if (a.Length == 0) return 0.0;
                double m = a[0];
                foreach (var v in a) if (v > m) m = v;
                return m;
        },
        [BuiltInFunction.STATS_MIN] = (task, args) =>
        {
                var a = ToDoubleArr(ArgOr(args, 0));
                if (a.Length == 0) return 0.0;
                double m = a[0];
                foreach (var v in a) if (v < m) m = v;
                return m;
        },
        [BuiltInFunction.STATS_RANGE] = (task, args) =>
        {
                var a = ToDoubleArr(ArgOr(args, 0));
                if (a.Length == 0) return 0.0;
                double lo = a[0], hi = a[0];
                foreach (var v in a) { if (v < lo) lo = v; if (v > hi) hi = v; }
                return hi - lo;
        },
        [BuiltInFunction.STATS_COVARIANCE] = (task, args) =>
        {
                var x = ToDoubleArr(ArgOr(args, 0));
                var y = ToDoubleArr(ArgOr(args, 1));
                var n = Math.Min(x.Length, y.Length);
                if (n < 2) return 0.0;
                double mx = 0, my = 0;
                for (var i = 0; i < n; i++) { mx += x[i]; my += y[i]; }
                mx /= n; my /= n;
                double sxy = 0;
                for (var i = 0; i < n; i++) sxy += (x[i] - mx) * (y[i] - my);
                return sxy / (n - 1);
        },
        [BuiltInFunction.TTEST] = (task, args) =>
        {
                return HypothesisResult(CommonLib.Numerics.TTestIndependent(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1))));
        },
        [BuiltInFunction.TTESTPAIRED] = (task, args) =>
        {
                return HypothesisResult(CommonLib.Numerics.TTestPaired(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1))));
        },
        [BuiltInFunction.TTESTONESAMPLE] = (task, args) =>
        {
                return HypothesisResult(CommonLib.Numerics.TTestOneSample(ToDoubleArr(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1))));
        },
        [BuiltInFunction.STATS_NCFPDF] = (task, args) =>
        {
                return CommonLib.Numerics.NcFPdf(SafeDouble(ArgOr(args, 1, 1.0)), SafeDouble(ArgOr(args, 2, 1.0)), SafeDouble(ArgOr(args, 3, 0.0)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_NCFCDF] = (task, args) =>
        {
                return CommonLib.Numerics.NcFCdf(SafeDouble(ArgOr(args, 1, 1.0)), SafeDouble(ArgOr(args, 2, 1.0)), SafeDouble(ArgOr(args, 3, 0.0)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_NCFSF] = (task, args) =>
        {
                return CommonLib.Numerics.NcFSf(SafeDouble(ArgOr(args, 1, 1.0)), SafeDouble(ArgOr(args, 2, 1.0)), SafeDouble(ArgOr(args, 3, 0.0)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_NCTPDF] = (task, args) =>
        {
                return CommonLib.Numerics.NcTPdf(SafeDouble(ArgOr(args, 1, 1.0)), SafeDouble(ArgOr(args, 2, 0.0)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_NCTCDF] = (task, args) =>
        {
                return CommonLib.Numerics.NcTCdf(SafeDouble(ArgOr(args, 1, 1.0)), SafeDouble(ArgOr(args, 2, 0.0)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_NCTSF] = (task, args) =>
        {
                return CommonLib.Numerics.NcTSf(SafeDouble(ArgOr(args, 1, 1.0)), SafeDouble(ArgOr(args, 2, 0.0)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_ZTPOISSONPMF] = (task, args) =>
        {
                return CommonLib.Numerics.ZTPoissonPmf(SafeDouble(ArgOr(args, 1, 1.0)), (int)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_ZTPOISSONCDF] = (task, args) =>
        {
                return CommonLib.Numerics.ZTPoissonCdf(SafeDouble(ArgOr(args, 1, 1.0)), (int)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_NCFPPF] = (task, args) =>
        {
                return CommonLib.Numerics.NcFPpf(SafeDouble(ArgOr(args, 1, 1.0)), SafeDouble(ArgOr(args, 2, 1.0)), SafeDouble(ArgOr(args, 3, 0.0)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_NCFISF] = (task, args) =>
        {
                return CommonLib.Numerics.NcFIsf(SafeDouble(ArgOr(args, 1, 1.0)), SafeDouble(ArgOr(args, 2, 1.0)), SafeDouble(ArgOr(args, 3, 0.0)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_NCTPPF] = (task, args) =>
        {
                return CommonLib.Numerics.NcTPpf(SafeDouble(ArgOr(args, 1, 1.0)), SafeDouble(ArgOr(args, 2, 0.0)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_NCTISF] = (task, args) =>
        {
                return CommonLib.Numerics.NcTIsf(SafeDouble(ArgOr(args, 1, 1.0)), SafeDouble(ArgOr(args, 2, 0.0)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_NCBETASF] = (task, args) =>
        {
                return CommonLib.Numerics.NcBetaSf(SafeDouble(ArgOr(args, 1, 1.0)), SafeDouble(ArgOr(args, 2, 1.0)), SafeDouble(ArgOr(args, 3, 0.0)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_NCGAMMASF] = (task, args) =>
        {
                return CommonLib.Numerics.NcGammaSf(SafeDouble(ArgOr(args, 1, 1.0)), SafeDouble(ArgOr(args, 2, 0.0)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_NCCHI2SF] = (task, args) =>
        {
                return CommonLib.Numerics.NcChi2Sf(SafeDouble(ArgOr(args, 1, 1.0)), SafeDouble(ArgOr(args, 2, 0.0)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_CHIISF] = (task, args) =>
        {
                return CommonLib.Numerics.ChiIsf(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.STATS_KRUSKAL] = (task, args) =>
        {
                return HypothesisResult(CommonLib.Numerics.Kruskal(ToJaggedArr(ArgOr(args, 0))));
        },
        [BuiltInFunction.STATSANOVA] = (task, args) =>
        {
                // STATS.Anova e STATS.ANOVA sono lo STESSO identificatore (BASIC è
                // case-insensitive): il contratto è il dict SUPERSET di IlRuntime.StatsAnova
                // (Statistic/PValue/DegreesOfFreedom + FValue/DF*/SS*/MS*). Prima qui si
                // costruiva il dict RIDOTTO HypothesisResult → su PCode i membri
                // FValue/DFBetween/... restavano vuoti (analysis_all rosso).
                var groups = ToDoubleMatrix(ArgOr(args, 0));
                return Basic.IlBackend.IlRuntime.StatsAnova(groups);
        },
        [BuiltInFunction.CHI2TEST] = (task, args) =>
        {
                return HypothesisResult(CommonLib.Numerics.ChiSquareGoodnessOfFit(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1))));
        },
        [BuiltInFunction.FISHEREXACT] = (task, args) =>
        {
                return HypothesisResult(CommonLib.Numerics.FisherExact((int)SafeDouble(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)), (int)SafeDouble(ArgOr(args, 2)), (int)SafeDouble(ArgOr(args, 3))));
        },
        [BuiltInFunction.SHAPIROWILK] = (task, args) =>
        {
                return HypothesisResult(CommonLib.Numerics.ShapiroWilk(ToDoubleArr(ArgOr(args, 0))));
        },
        [BuiltInFunction.NORMALTEST] = (task, args) =>
        {
                return HypothesisResult(CommonLib.Numerics.NormalityTest(ToDoubleArr(ArgOr(args, 0))));
        },
        [BuiltInFunction.ZTEST] = (task, args) =>
        {
                return HypothesisResult(CommonLib.Numerics.ZTestOneSample(ToDoubleArr(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2))));
        },
        [BuiltInFunction.KOLMOGOROVSMIRNOV] = (task, args) =>
        {
                return HypothesisResult(CommonLib.Numerics.KolmogorovSmirnovTwoSample(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1))));
        },
    };

    private object? ExecuteBuiltInStats(PCodeTask task, BuiltInFunction fn, object?[] args)
        => STATSDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInStats (fn={fn}).");

}
