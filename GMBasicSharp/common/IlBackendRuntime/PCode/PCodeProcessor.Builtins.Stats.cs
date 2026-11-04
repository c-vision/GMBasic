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
using CommonLib.Trading;

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
        [BuiltInFunction.STATS_GAMMAPDF] = (task, args) =>
        {
                return CommonLib.Numerics.GammaPdf(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.STATS_EXPSF] = (task, args) =>
        {
                return 1.0 - CommonLib.Numerics.ExponentialCdf(SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_STUDENTSF] = (task, args) =>
        {
                return 1.0 - CommonLib.Numerics.StudentTCdf(SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_WEIBULLSF] = (task, args) =>
        {
                return 1.0 - CommonLib.Numerics.WeibullCdf(SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_NORMALSF] = (task, args) =>
        {
                var x = SafeDouble(ArgOr(args, 0));
                var mu = SafeDouble(ArgOr(args, 1, 0.0));
                var sd = SafeDouble(ArgOr(args, 2, 1.0));
                return 1.0 - CommonLib.Numerics.NormalCdf(mu, sd, x);
        },
        [BuiltInFunction.STATS_PARETOCDF] = (task, args) =>
        {
                return CommonLib.Numerics.ParetoCdf(SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_UNIFORMCDF] = (task, args) =>
        {
                return CommonLib.Numerics.UniformCdf(SafeDouble(ArgOr(args, 1, 0.0)), SafeDouble(ArgOr(args, 2, 1.0)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_UNIFORMSF] = (task, args) =>
        {
                return CommonLib.Numerics.UniformSf(SafeDouble(ArgOr(args, 1, 0.0)), SafeDouble(ArgOr(args, 2, 1.0)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_PARETOSF] = (task, args) =>
        {
                return 1.0 - CommonLib.Numerics.ParetoCdf(SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_LOGNORMALSF] = (task, args) =>
        {
                var x = SafeDouble(ArgOr(args, 0));
                var mu = SafeDouble(ArgOr(args, 1, 0.0));
                var sd = SafeDouble(ArgOr(args, 2, 1.0));
                return 1.0 - CommonLib.Numerics.LognormalCdf(mu, sd, x);
        },
        [BuiltInFunction.STATS_FSF] = (task, args) =>
        {
                return 1.0 - CommonLib.Numerics.FCdf(SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_CHISQUAREDSF] = (task, args) =>
        {
                return 1.0 - CommonLib.Numerics.ChiSquaredCdf(SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.STATS_GAMMASF] = (task, args) =>
        {
                return 1.0 - CommonLib.Numerics.GammaCdf(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.STATS_GAMMACDF] = (task, args) =>
        {
                return CommonLib.Numerics.GammaCdf(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.STATS_GAMMAPPF] = (task, args) =>
        {
                return CommonLib.Numerics.GammaPpf(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.STATS_GAMMAISF] = (task, args) =>
        {
                return CommonLib.Numerics.GammaIsf(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
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
        [BuiltInFunction.STATSANOVA] = (task, args) =>
        {
                {
                var groups = ToDoubleMatrix(ArgOr(args, 0));
                var (statistic, pValue, _, _) = CommonLib.Numerics.Anova(groups);
                return HypothesisResult(new CommonLib.StatisticalTestResult(statistic, pValue, groups.Sum(static group => group.Length) - groups.Length));
                }
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
