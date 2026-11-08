// FILE: IlBackendCodegen/Partial/IlProgramCompiler.TypeSystem.RuntimeMethods.cs
// PARTIAL — dominio TYPESYSTEM/RUNTIME-METHODS: i MethodInfo/proprietà del runtime
// IlRuntime richiamati dal codice generato (DATA/RND/abs/... ). Migrato da
// IlProgramCompiler.TypeSystem.cs (2026-08-30 — split 6.0.1, file <= 1200 righe).

using System.Linq;
using System.Reflection;
using Basic.IlBackend.Gpm;
using Basic.Runtime.Gpm;
using Basic;

namespace Basic.IlBackend;

/// <summary>Partial del transpiler IL: riferimenti MethodInfo al runtime IlRuntime.</summary>
public sealed partial class IlProgramCompiler
{
    private static readonly MethodInfo IlRuntimeCurrentGetter =
        typeof(IlRuntime).GetProperty(nameof(IlRuntime.Current))!.GetGetMethod()!;

    private static readonly MethodInfo IlRuntimeBeginDataMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.BeginData))!;

    private static readonly MethodInfo IlRuntimeRestoreDataMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.RestoreData))!;

    private static readonly MethodInfo IlRuntimeReadDataDoubleMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadDataDouble))!;
    private static readonly MethodInfo IlRuntimeReadDataSingleMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadDataSingle))!;
    private static readonly MethodInfo IlRuntimeReadDataLongMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadDataLong))!;
    private static readonly MethodInfo IlRuntimeReadDataULongMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadDataULong))!;
    private static readonly MethodInfo IlRuntimeReadDataIntegerMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadDataInteger))!;
    private static readonly MethodInfo IlRuntimeReadDataUIntegerMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadDataUInteger))!;
    private static readonly MethodInfo IlRuntimeReadDataByteMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadDataByte))!;
    private static readonly MethodInfo IlRuntimeReadDataSByteMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadDataSByte))!;
    private static readonly MethodInfo IlRuntimeReadDataBitMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadDataBit))!;
    private static readonly MethodInfo IlRuntimeReadDataCurrencyMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadDataCurrency))!;
    private static readonly MethodInfo IlRuntimeReadDataDateMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadDataDate))!;
    private static readonly MethodInfo IlRuntimeReadDataStringMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadDataString))!;
    private static readonly MethodInfo IlRuntimeRandomizeMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Randomize), Type.EmptyTypes)!;
    private static readonly MethodInfo IlRuntimeSystemShellMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.SystemShell))!;
    private static readonly MethodInfo IlRuntimeRandomizeSeedMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.RandomizeSeed))!;
    private static readonly MethodInfo IlRuntimeRndMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Rnd))!;
    private static readonly MethodInfo IlRuntimeAbsMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Abs))!;
    private static readonly MethodInfo IlRuntimeAtnMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Atn))!;
    private static readonly MethodInfo IlRuntimeAtn2Method = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Atn2))!;
    private static readonly MethodInfo IlRuntimeCosMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Cos))!;
    private static readonly MethodInfo IlRuntimeExpMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Exp))!;
    private static readonly MethodInfo IlRuntimeLogMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Log))!;
    private static readonly MethodInfo IlRuntimeLog10Method = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Log10))!;
    private static readonly MethodInfo IlRuntimeSinMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Sin))!;
    private static readonly MethodInfo IlRuntimeSqrMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Sqr))!;
    private static readonly MethodInfo IlRuntimeGammaMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Gamma), new[] { typeof(double) })!;
    private static readonly MethodInfo IlRuntimeLGammaMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.LGamma))!;
    private static readonly MethodInfo IlRuntimeBetaMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Beta), new[] { typeof(double), typeof(double) })!;
    private static readonly MethodInfo IlRuntimeErfMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Erf))!;
    private static readonly MethodInfo IlRuntimeErfcMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Erfc))!;
    private static readonly MethodInfo IlRuntimeGammaIncMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.GammaInc))!;
    private static readonly MethodInfo IlRuntimeGammaInccMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.GammaIncc))!;
    private static readonly MethodInfo IlRuntimeBetaIncMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.BetaInc))!;
    private static readonly MethodInfo IlRuntimeDigammaMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Digamma))!;
    private static readonly MethodInfo IlRuntimeZetaMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Zeta))!;
    private static readonly MethodInfo IlRuntimeZetaHurwitzMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ZetaHurwitz))!;
    private static readonly MethodInfo IlRuntimePolygammaMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Polygamma))!;
    private static readonly MethodInfo IlRuntimeHypergeom2F1Method = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Hypergeom2F1))!;
    private static readonly MethodInfo IlRuntimeHypergeom1F1Method = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Hypergeom1F1))!;
    private static readonly MethodInfo IlRuntimeLambertWMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.LambertW))!;
    private static readonly MethodInfo IlRuntimeMittagLefflerMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MittagLeffler))!;
    private static readonly MethodInfo IlRuntimeStruveHMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.StruveH))!;
    private static readonly MethodInfo IlRuntimeKelvinBerMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.KelvinBer))!;
    private static readonly MethodInfo IlRuntimeKelvinBeiMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.KelvinBei))!;
    private static readonly MethodInfo IlRuntimeParabolicCylinderMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ParabolicCylinder))!;
    private static readonly MethodInfo IlRuntimeBesselJMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.BesselJ))!;
    private static readonly MethodInfo IlRuntimeBesselYMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.BesselY))!;
    private static readonly MethodInfo IlRuntimeBesselIMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.BesselI))!;
    private static readonly MethodInfo IlRuntimeBesselKMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.BesselK))!;
    private static readonly MethodInfo IlRuntimeBetaLnMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.BetaLn))!;
    private static readonly MethodInfo IlRuntimeErfInvMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ErfInv))!;
    private static readonly MethodInfo IlRuntimeErfcInvMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ErfcInv))!;
    private static readonly MethodInfo IlRuntimeGammaIncInvMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.GammaIncInv))!;
    private static readonly MethodInfo IlRuntimeGammaInccInvMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.GammaInccInv))!;
    private static readonly MethodInfo IlRuntimeBetaIncInvMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.BetaIncInv))!;
    private static readonly MethodInfo IlRuntimeConvolveMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Convolve))!;
    private static readonly MethodInfo IlRuntimeCorrelateMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Correlate))!;
    private static readonly MethodInfo IlRuntimeHannMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Hann))!;
    private static readonly MethodInfo IlRuntimeHammingMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Hamming))!;
    private static readonly MethodInfo IlRuntimeBlackmanMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Blackman))!;
    private static readonly MethodInfo IlRuntimeTanMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Tan))!;
    private static readonly MethodInfo IlRuntimeAsinMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Asin))!;
    private static readonly MethodInfo IlRuntimeAcosMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Acos))!;
    private static readonly MethodInfo IlRuntimeFloorMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Floor))!;
    private static readonly MethodInfo IlRuntimeCeilingMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Ceiling))!;
    private static readonly MethodInfo IlRuntimeSgnMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Sgn))!;
    private static readonly MethodInfo IlRuntimePiMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Pi))!;
    private static readonly MethodInfo IlRuntimeFixMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Fix))!;
    private static readonly MethodInfo IlRuntimeRoundMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Round))!;
    private static readonly MethodInfo IlRuntimeTruncMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Trunc))!;
    private static readonly MethodInfo IlRuntimeSinhMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Sinh))!;
    private static readonly MethodInfo IlRuntimeCoshMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Cosh))!;
    private static readonly MethodInfo IlRuntimeTanhMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Tanh))!;
    private static readonly MethodInfo IlRuntimeDeg2RadMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Deg2Rad))!;
    private static readonly MethodInfo IlRuntimeRad2DegMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Rad2Deg))!;
    private static readonly MethodInfo IlRuntimeHypotMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Hypot))!;
    private static readonly MethodInfo IlRuntimeMinMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Min))!;
    private static readonly MethodInfo IlRuntimeMaxMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Max))!;
    private static readonly MethodInfo IlRuntimeClampMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Clamp))!;
    private static readonly MethodInfo IlRuntimeFactMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Fact))!;
    private static readonly MethodInfo IlRuntimePermMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Perm))!;
    private static readonly MethodInfo IlRuntimeCombMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Comb))!;
    private static readonly MethodInfo IlRuntimeDist2dMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Dist2d))!;
    private static readonly MethodInfo IlRuntimeDist3dMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Dist3d))!;
    private static readonly MethodInfo IlRuntimePolarXMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.PolarX))!;
    private static readonly MethodInfo IlRuntimePolarYMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.PolarY))!;
    private static readonly MethodInfo IlRuntimeAngle2dMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Angle2d))!;
    private static readonly MethodInfo IlRuntimeMeanMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Mean))!;
    private static readonly MethodInfo IlRuntimeMedianMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Median))!;
    private static readonly MethodInfo IlRuntimeVarianceMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Variance))!;
    private static readonly MethodInfo IlRuntimeStdDevMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.StdDev))!;
    private static readonly MethodInfo IlRuntimeSkewnessMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Skewness))!;
    private static readonly MethodInfo IlRuntimeKurtosisMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Kurtosis))!;
    private static readonly MethodInfo IlRuntimeCorrelationMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Correlation))!;
    private static readonly MethodInfo IlRuntimePercentileMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Percentile))!;
    private static readonly MethodInfo IlRuntimeMadMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Mad))!;
    // Fase 9 (docs/StdLib-Runtime-Plan.md): le 22 PDF/CDF continue puntano direttamente a
    // StdLib.Runtime.MathFunctions invece che al wrapper locale IlRuntime — vedi
    // EmitDoubleArityBuiltinCallXLast per il perché dello shift di argomento.
    private static readonly MethodInfo IlRuntimeNormalPdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NormalPdf))!;
    private static readonly MethodInfo IlRuntimeNormalCdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NormalCdf))!;
    private static readonly MethodInfo IlRuntimeNormalPpfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NormalPpf))!;
    private static readonly MethodInfo IlRuntimeNormalIsfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NormalIsf))!;
    private static readonly MethodInfo IlRuntimeLognormalPpfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.LognormalPpf))!;
    private static readonly MethodInfo IlRuntimeChiSquaredPpfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ChiSquaredPpf))!;
    private static readonly MethodInfo IlRuntimeChiSquaredIsfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ChiSquaredIsf))!;
    private static readonly MethodInfo IlRuntimeChiPpfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ChiPpf))!;
    private static readonly MethodInfo IlRuntimeBetaPpfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.BetaPpf))!;
    private static readonly MethodInfo IlRuntimeFPpfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.FPpf))!;
    private static readonly MethodInfo IlRuntimeFIsfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.FIsf))!;
    private static readonly MethodInfo IlRuntimeStudentTPpfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.StudentTPpf))!;
    private static readonly MethodInfo IlRuntimeStudentTIsfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.StudentTIsf))!;
    private static readonly MethodInfo IlRuntimeExponentialPpfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ExponentialPpf))!;
    private static readonly MethodInfo IlRuntimeExponentialIsfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ExponentialIsf))!;
    private static readonly MethodInfo IlRuntimeWeibullPpfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.WeibullPpf))!;
    private static readonly MethodInfo IlRuntimeLogisticPpfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.LogisticPpf))!;
    private static readonly MethodInfo IlRuntimeUniformPpfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.UniformPpf))!;
    private static readonly MethodInfo IlRuntimeParetoPpfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ParetoPpf))!;
    private static readonly MethodInfo IlRuntimeTriangularPpfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.TriangularPpf))!;
    private static readonly MethodInfo IlRuntimeLognormalPdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.LognormalPdf))!;
    private static readonly MethodInfo IlRuntimeLognormalCdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.LognormalCdf))!;
    private static readonly MethodInfo IlRuntimeChiPdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ChiPdf))!;
    private static readonly MethodInfo IlRuntimeChiCdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ChiCdf))!;
    private static readonly MethodInfo IlRuntimeChiSquaredPdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ChiSquaredPdf))!;
    private static readonly MethodInfo IlRuntimeChiSquaredCdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ChiSquaredCdf))!;
    private static readonly MethodInfo IlRuntimeBetaPdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.BetaPdf))!;
    private static readonly MethodInfo IlRuntimeBetaCdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.BetaCdf))!;
    private static readonly MethodInfo IlRuntimeStudentTPdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.StudentTPdf))!;
    private static readonly MethodInfo IlRuntimeStudentTCdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.StudentTCdf))!;
    private static readonly MethodInfo IlRuntimeWeibullPdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.WeibullPdf))!;
    private static readonly MethodInfo IlRuntimeWeibullCdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.WeibullCdf))!;
    private static readonly MethodInfo IlRuntimeFPdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.FPdf))!;
    private static readonly MethodInfo IlRuntimeFCdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.FCdf))!;
    private static readonly MethodInfo IlRuntimeExponentialPdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ExponentialPdf))!;
    private static readonly MethodInfo IlRuntimeExponentialCdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ExponentialCdf))!;
    private static readonly MethodInfo IlRuntimeExponentialSfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ExponentialSf))!;
    private static readonly MethodInfo IlRuntimeStudentTSfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.StudentTSf))!;
    private static readonly MethodInfo IlRuntimeChiSquaredSfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ChiSquaredSf))!;
    private static readonly MethodInfo IlRuntimeWeibullSfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.WeibullSf))!;
    private static readonly MethodInfo IlRuntimeFSfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.FSf))!;
    private static readonly MethodInfo IlRuntimeNormalSfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NormalSf))!;
    private static readonly MethodInfo IlRuntimeLognormalSfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.LognormalSf))!;
    private static readonly MethodInfo IlRuntimeParetoCdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ParetoCdf))!;
    private static readonly MethodInfo IlRuntimeParetoSfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ParetoSf))!;
    private static readonly MethodInfo IlRuntimeUniformCdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.UniformCdf))!;
    private static readonly MethodInfo IlRuntimeUniformSfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.UniformSf))!;
    private static readonly MethodInfo IlRuntimeBetaSfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.BetaSf))!;
    private static readonly MethodInfo IlRuntimeChiSfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ChiSf))!;
    private static readonly MethodInfo IlRuntimeLogisticCdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.LogisticCdf))!;
    private static readonly MethodInfo IlRuntimeLogisticSfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.LogisticSf))!;
    private static readonly MethodInfo IlRuntimeTriangularCdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.TriangularCdf))!;
    private static readonly MethodInfo IlRuntimeTriangularSfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.TriangularSf))!;
    private static readonly MethodInfo IlRuntimeWeibullIsfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.WeibullIsf))!;
    private static readonly MethodInfo IlRuntimeUniformIsfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.UniformIsf))!;
    private static readonly MethodInfo IlRuntimeParetoIsfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ParetoIsf))!;
    private static readonly MethodInfo IlRuntimeBetaIsfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.BetaIsf))!;
    private static readonly MethodInfo IlRuntimeLogisticIsfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.LogisticIsf))!;
    private static readonly MethodInfo IlRuntimeLognormalIsfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.LognormalIsf))!;
    private static readonly MethodInfo IlRuntimeTriangularIsfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.TriangularIsf))!;
    private static readonly MethodInfo IlRuntimeChiIsfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ChiIsf))!;
    private static readonly MethodInfo IlRuntimeNcChi2PdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcChi2Pdf))!;
    private static readonly MethodInfo IlRuntimeNcChi2CdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcChi2Cdf))!;
    private static readonly MethodInfo IlRuntimeNcChi2SfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcChi2Sf))!;
    private static readonly MethodInfo IlRuntimeNcBetaPdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcBetaPdf))!;
    private static readonly MethodInfo IlRuntimeNcBetaCdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcBetaCdf))!;
    private static readonly MethodInfo IlRuntimeNcBetaSfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcBetaSf))!;
    private static readonly MethodInfo IlRuntimeNcGammaPdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcGammaPdf))!;
    private static readonly MethodInfo IlRuntimeNcGammaCdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcGammaCdf))!;
    private static readonly MethodInfo IlRuntimeNcGammaSfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcGammaSf))!;
    private static readonly MethodInfo IlRuntimeNcFPdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcFPdf))!;
    private static readonly MethodInfo IlRuntimeNcFCdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcFCdf))!;
    private static readonly MethodInfo IlRuntimeNcFSfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcFSf))!;
    private static readonly MethodInfo IlRuntimeNcTPdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcTPdf))!;
    private static readonly MethodInfo IlRuntimeNcTCdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcTCdf))!;
    private static readonly MethodInfo IlRuntimeNcTSfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcTSf))!;
    private static readonly MethodInfo IlRuntimeNcChi2PpfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcChi2Ppf))!;
    private static readonly MethodInfo IlRuntimeNcChi2IsfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcChi2Isf))!;
    private static readonly MethodInfo IlRuntimeNcBetaPpfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcBetaPpf))!;
    private static readonly MethodInfo IlRuntimeNcBetaIsfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcBetaIsf))!;
    private static readonly MethodInfo IlRuntimeNcGammaPpfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcGammaPpf))!;
    private static readonly MethodInfo IlRuntimeNcGammaIsfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcGammaIsf))!;
    private static readonly MethodInfo IlRuntimeNcFPpfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcFPpf))!;
    private static readonly MethodInfo IlRuntimeNcFIsfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcFIsf))!;
    private static readonly MethodInfo IlRuntimeNcTPpfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcTPpf))!;
    private static readonly MethodInfo IlRuntimeNcTIsfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NcTIsf))!;
    private static readonly MethodInfo IlRuntimeZTPoissonPmfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ZTPoissonPmf))!;
    private static readonly MethodInfo IlRuntimeZTPoissonCdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ZTPoissonCdf))!;
    private static readonly MethodInfo IlRuntimeParetoPdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.ParetoPdf))!;
    private static readonly MethodInfo IlRuntimeLogisticPdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.LogisticPdf))!;
    private static readonly MethodInfo IlRuntimeTriangularPdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.TriangularPdf))!;
    private static readonly MethodInfo IlRuntimeUniformPdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.UniformPdf))!;
    private static readonly MethodInfo IlRuntimeSkewedNormalPdfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.SkewedNormalPdf))!;
    // Le 6 PMF discrete (Bernoulli/Binomial/Poisson/Geometric/NegativeBinomial/Hypergeometric)
    // restano sul wrapper IlRuntime per ora — mescolano int/double e in un caso (BinomialPmf)
    // l'ordine non è un semplice shift, migrazione separata (Fase 9, parte B).
    // Fase 9 parte B: le 6 PMF discrete puntano ora a StdLib.Runtime.MathFunctions — vedi
    // EmitPmfBuiltinCall per gli argomenti int/double e la permutazione di BinomialPmf.
    private static readonly MethodInfo IlRuntimeBernoulliPmfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.BernoulliPmf))!;
    private static readonly MethodInfo IlRuntimeBinomialPmfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.BinomialPmf))!;
    private static readonly MethodInfo IlRuntimePoissonPmfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.PoissonPmf))!;
    private static readonly MethodInfo IlRuntimeGeometricPmfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.GeometricPmf))!;
    private static readonly MethodInfo IlRuntimeNegativeBinomialPmfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.NegativeBinomialPmf))!;
    private static readonly MethodInfo IlRuntimeHypergeometricPmfMethod = typeof(Basic.StdLibRuntime.MathFunctions).GetMethod(nameof(Basic.StdLibRuntime.MathFunctions.HypergeometricPmf))!;
    private static readonly MethodInfo StringHelpersAscMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Asc))!;
    private static readonly MethodInfo StringHelpersChrMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Chr))!;
    private static readonly MethodInfo StringHelpersStrMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Str))!;
    private static readonly MethodInfo StringHelpersValMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Val))!;
    private static readonly MethodInfo StringHelpersCStrMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.CStr))!;
    private static readonly MethodInfo StringHelpersHexMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Hex))!;
    private static readonly MethodInfo StringHelpersOctMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Oct))!;
    private static readonly MethodInfo StringHelpersBinMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Bin))!;
    private static readonly MethodInfo StringHelpersIsNumericMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.IsNumeric))!;
    private static readonly MethodInfo StringHelpersSpaceMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Space))!;
    private static readonly MethodInfo StringHelpersDupMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Dup))!;
    private static readonly MethodInfo StringHelpersReverseMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Reverse))!;
    private static readonly MethodInfo StringHelpersPascalMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Pascal))!;
    private static readonly MethodInfo StringHelpersCamelMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Camel))!;
    private static readonly MethodInfo StringHelpersTitleMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Title))!;
    private static readonly MethodInfo StringHelpersSentenceMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Sentence))!;
    private static readonly MethodInfo StringHelpersReplaceMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Replace))!;
    private static readonly MethodInfo StringHelpersRemoveMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Remove))!;
    private static readonly MethodInfo StringHelpersContainsMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Contains))!;
    private static readonly MethodInfo StringHelpersStartsWithMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.StartsWith))!;
    private static readonly MethodInfo StringHelpersEndsWithMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.EndsWith))!;
    private static readonly MethodInfo StringHelpersSnakeMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Snake))!;
    private static readonly MethodInfo StringHelpersKebabMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Kebab))!;
    private static readonly MethodInfo StringHelpersRepeatMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Repeat))!;
    private static readonly MethodInfo StringHelpersPadLMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.PadL))!;
    private static readonly MethodInfo StringHelpersPadRMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.PadR))!;
    private static readonly MethodInfo StringHelpersPadCMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.PadC))!;
    private static readonly MethodInfo StringHelpersWrapMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Wrap))!;
    private static readonly MethodInfo StringHelpersSearchMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Search))!;
    private static readonly MethodInfo StringHelpersSplitMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Split))!;
    private static readonly MethodInfo StringHelpersJoinMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Join))!;
    private static readonly MethodInfo StringHelpersFormatMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Format))!;
    private static readonly MethodInfo StringHelpersCountMethod = typeof(StringHelpers).GetMethod(nameof(StringHelpers.Count))!;
    private static readonly MethodInfo IlRuntimeNowMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Now))!;
    private static readonly MethodInfo IlRuntimeTodayMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Today))!;
    private static readonly MethodInfo IlRuntimeDateValueMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.DateValue))!;
    private static readonly MethodInfo IlRuntimeToOADateMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ToOADate))!;
    private static readonly MethodInfo IlRuntimeDateSerialMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.DateSerial))!;
    private static readonly MethodInfo IlRuntimeTimeSerialMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TimeSerial))!;
    private static readonly MethodInfo IlRuntimeDtosMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Dtos))!;
    private static readonly MethodInfo IlRuntimeStodMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Stod))!;
    private static readonly MethodInfo IlMessageCmdMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MessageCmd), new[] { typeof(string) })!;
    private static readonly MethodInfo IlRuntimeCtodMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.CtOD))!;
    private static readonly MethodInfo IlRuntimeCtODDateMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.CtODDate))!;
    private static readonly MethodInfo IlRuntimeYearMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Year))!;
    private static readonly MethodInfo IlRuntimeMonthMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Month))!;
    private static readonly MethodInfo IlRuntimeDayMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Day))!;
    private static readonly MethodInfo IlRuntimeHourMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Hour))!;
    private static readonly MethodInfo IlRuntimeMinuteMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Minute))!;
    private static readonly MethodInfo IlRuntimeSecondMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Second))!;
    private static readonly MethodInfo IlRuntimeWeekdayMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Weekday))!;
    private static readonly MethodInfo IlRuntimeIsLeapYearMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.IsLeapYear))!;
    private static readonly MethodInfo IlRuntimeDaysInMonthMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.DaysInMonth))!;
    private static readonly MethodInfo IlRuntimeFirstDayOfMonthMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FirstDayOfMonth))!;
    private static readonly MethodInfo IlRuntimeIsoWeekOfYearMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.IsoWeekOfYear))!;
    private static readonly MethodInfo IlRuntimeDateFromUnixMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.DateFromUnix))!;
    private static readonly MethodInfo IlRuntimeUnixTimestampMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.UnixTimestamp))!;
    private static readonly MethodInfo IlRuntimeTimeMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Time))!;
    private static readonly MethodInfo IlInkeyMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Inkey), Type.EmptyTypes)!;
    private static readonly MethodInfo IlInkeyTimeoutMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Inkey), new[] { typeof(int) })!;
    private static readonly MethodInfo IlWritePrinterMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WritePrinter), [typeof(string), typeof(bool)])!;
    private static readonly MethodInfo IlPrintTabZoneMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.PrintTabZone))!;
    private static readonly MethodInfo IlPrinterOnlyMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.PrinterOnly), Type.EmptyTypes)!;
    private static readonly MethodInfo IlSetClipperCmdMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.SetClipperCmd), [typeof(int), typeof(string)])!;
    private static readonly MethodInfo IlRuntimeDaySerialMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.DaySerial))!;
    private static readonly MethodInfo IlRuntimeStrDateMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.StrDate))!;
    private static readonly MethodInfo IlRuntimeStrTimeMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.StrTime))!;
    private static readonly MethodInfo IlRuntimeEomonthMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Eomonth))!;
    private static readonly MethodInfo IlRuntimeIsDateMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.IsDate))!;
    private static readonly MethodInfo IlRuntimeTiMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Ti))!;
    private static readonly MethodInfo IlRuntimeStrTiMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.StrTi))!;
    private static readonly MethodInfo IlRuntimeDateAddMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.DateAdd))!;

    private static readonly MethodInfo RuntimeHostConsoleGetter =
        typeof(Runtime.IRuntimeHost).GetProperty(nameof(Runtime.IRuntimeHost.Console))!.GetGetMethod()!;
    private static readonly MethodInfo RuntimeHostGraphicsGetter =
        typeof(Runtime.IRuntimeHost).GetProperty(nameof(Runtime.IRuntimeHost.Graphics))!.GetGetMethod()!;
    private static readonly MethodInfo RuntimeHostAudioGetter =
        typeof(Runtime.IRuntimeHost).GetProperty(nameof(Runtime.IRuntimeHost.Audio))!.GetGetMethod()!;
    private static readonly MethodInfo AudioSoundMethod =
        typeof(Runtime.IAudioService).GetMethod(nameof(Runtime.IAudioService.Sound), [typeof(double), typeof(double), typeof(int)])!;
    private static readonly MethodInfo AudioSetFilterMethod =
        typeof(Runtime.IAudioService).GetMethod(nameof(Runtime.IAudioService.SetFilter), [typeof(int), typeof(int), typeof(double), typeof(double)])!;
    private static readonly MethodInfo AudioLoadSoundMethod =
        typeof(Runtime.IAudioService).GetMethod(nameof(Runtime.IAudioService.LoadSound), [typeof(string)])!;
    private static readonly MethodInfo AudioPlaySoundMethod =
        typeof(Runtime.IAudioService).GetMethod(nameof(Runtime.IAudioService.PlaySound), [typeof(int), typeof(int), typeof(int)])!;
    private static readonly MethodInfo AudioSetVolumeMethod =
        typeof(Runtime.IAudioService).GetMethod(nameof(Runtime.IAudioService.SetVolume), [typeof(int), typeof(int?)])!;
    private static readonly MethodInfo AudioPlayMethod =
        typeof(Runtime.IAudioService).GetMethod(nameof(Runtime.IAudioService.Play), [typeof(string)])!;
    private static readonly MethodInfo AudioControlMethod =
        typeof(Runtime.IAudioService).GetMethod(nameof(Runtime.IAudioService.AudioControl), [typeof(int), typeof(object)])!;
    private static readonly MethodInfo RuntimeHostFileGetter =
        typeof(Runtime.IRuntimeHost).GetProperty(nameof(Runtime.IRuntimeHost.File))!.GetGetMethod()!;
    private static readonly MethodInfo RuntimeHostNetworkGetter =
        typeof(Runtime.IRuntimeHost).GetProperty(nameof(Runtime.IRuntimeHost.Network))!.GetGetMethod()!;
    private static readonly MethodInfo RuntimeHostDatabaseGetter =
        typeof(Runtime.IRuntimeHost).GetProperty(nameof(Runtime.IRuntimeHost.Database))!.GetGetMethod()!;
    private static readonly MethodInfo RuntimeHostHttpGetter =
        typeof(Runtime.IRuntimeHost).GetProperty(nameof(Runtime.IRuntimeHost.Http))!.GetGetMethod()!;

    private static readonly MethodInfo ConsolePrintMethod =
        typeof(Runtime.IConsoleService).GetMethod(nameof(Runtime.IConsoleService.Print), [typeof(string)])!;

    private static readonly MethodInfo ConsolePrintNewLineMethod =
        typeof(Runtime.IConsoleService).GetMethod(nameof(Runtime.IConsoleService.PrintNewLine))!;
    private static readonly MethodInfo IlRuntimePrintDispatchMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.PrintDispatch), [typeof(string)])!;
    private static readonly MethodInfo IlRuntimePrintNewLineDispatchMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.PrintNewLineDispatch))!;
    private static readonly MethodInfo IlRuntimePrintStageBeginMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.PrintStageBegin))!;
    private static readonly MethodInfo IlRuntimePrintStageEndMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.PrintStageEnd), [typeof(bool)])!;
    private static readonly MethodInfo IlRuntimeLocateDispatchMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.LocateDispatch), [typeof(int), typeof(int)])!;
    private static readonly MethodInfo IlRuntimeClsDispatchMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.ClsDispatch), [typeof(object)])!;
    private static readonly MethodInfo IlRuntimePaletteDispatchMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.PaletteDispatch), [typeof(int), typeof(int), typeof(int), typeof(int)])!;
    private static readonly MethodInfo IlRuntimeDisplayModeFieldMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.DisplayModeField), [typeof(object), typeof(string)])!;
    private static readonly MethodInfo IlRuntimeColorDispatchMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.ColorDispatch), [typeof(object), typeof(object)])!;
    private static readonly MethodInfo IlRuntimeRgbaToLineColorMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.RgbaToLineColor), [typeof(object)])!;
    private static readonly MethodInfo GraphicsDrawPixelBufferMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.DrawPixelBuffer), [typeof(int), typeof(int), typeof(object)])!;
    private static readonly MethodInfo GraphicsDrawTextMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.DrawText), [typeof(string), typeof(int), typeof(int), typeof(double)])!;
    private static readonly MethodInfo GraphicsTabMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.GraphicsTab), [typeof(int)])!;
    private static readonly MethodInfo GraphicsTabZoneMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.GraphicsTabZone))!;
    private static readonly MethodInfo GraphicsSpriteDefMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.SpriteDef), [typeof(int), typeof(int), typeof(int), typeof(int[])])!;
    private static readonly MethodInfo ConsolePrintNewLineWithTextMethod =
        typeof(Runtime.IConsoleService).GetMethod(nameof(Runtime.IConsoleService.PrintLine), [typeof(string)])!;
    private static readonly MethodInfo ConsoleClearMethod =
        typeof(Runtime.IConsoleService).GetMethod(nameof(Runtime.IConsoleService.Clear))!;
    private static readonly MethodInfo ConsoleSetForegroundColorMethod =
        typeof(Runtime.IConsoleService).GetMethod(nameof(Runtime.IConsoleService.SetForegroundColor), [typeof(int)])!;
    private static readonly MethodInfo ConsoleSetBackgroundColorMethod =
        typeof(Runtime.IConsoleService).GetMethod(nameof(Runtime.IConsoleService.SetBackgroundColor), [typeof(int)])!;
    private static readonly MethodInfo ConsoleResetColorMethod =
        typeof(Runtime.IConsoleService).GetMethod(nameof(Runtime.IConsoleService.ResetColor))!;
    private static readonly MethodInfo ConsoleSetCursorPositionMethod =
        typeof(Runtime.IConsoleService).GetMethod(nameof(Runtime.IConsoleService.SetCursorPosition), [typeof(int), typeof(int)])!;
    private static readonly MethodInfo ConsoleTitleSetterMethod =
        typeof(Runtime.IConsoleService).GetProperty(nameof(Runtime.IConsoleService.Title))!.GetSetMethod()!;
    private static readonly MethodInfo ConsoleSetPaletteColorMethod =
        typeof(Runtime.IConsoleService).GetMethod(nameof(Runtime.IConsoleService.SetPaletteColor), [typeof(int), typeof(int), typeof(int), typeof(int)])!;
    // --- IGraphicsService (CommonGraphics; in headless NullGraphicsService) ---
    private static readonly MethodInfo GraphicsScreenMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.Screen), [typeof(int), typeof(int), typeof(int), typeof(int)])!;
    /* SCREEN ... USE CELLS|PIXELS: spazio coordinate DICHIARATO (motore unico: runtime C). */
    private static readonly MethodInfo GraphicsScreenCoordsMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.ScreenCoords), [typeof(int)])!;
    private static readonly MethodInfo GraphicsTextModeMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.TextMode), [typeof(int)])!;
    private static readonly MethodInfo GraphicsPSetMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.PSet), [typeof(int), typeof(int), typeof(int)])!;
    private static readonly MethodInfo GraphicsPresetMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.Preset), [typeof(int), typeof(int)])!;
    private static readonly MethodInfo GraphicsPointMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.Point), [typeof(int), typeof(int)])!;
    private static readonly MethodInfo GraphicsLineMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.Line), [typeof(int), typeof(int), typeof(int), typeof(int), typeof(int), typeof(string)])!;
    private static readonly MethodInfo GraphicsCircleMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.Circle), [typeof(int), typeof(int), typeof(int), typeof(int)])!;
    private static readonly MethodInfo GraphicsPaintMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.Paint), [typeof(int), typeof(int), typeof(int), typeof(int)])!;
    private static readonly MethodInfo GraphicsDrawMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.Draw), [typeof(string)])!;
    private static readonly MethodInfo GraphicsPCopyMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.PCopy), [typeof(int), typeof(int)])!;
    private static readonly MethodInfo GraphicsScreenSetMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.ScreenSet), [typeof(int), typeof(int), typeof(int)])!;
    private static readonly MethodInfo GraphicsFontMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.Font), [typeof(string), typeof(int)])!;
    private static readonly MethodInfo GraphicsImageGetMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.ImageGet), [typeof(int), typeof(int), typeof(int), typeof(int)])!;
    private static readonly MethodInfo GraphicsImagePutMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.ImagePut), [typeof(int), typeof(int), typeof(int[]), typeof(int)])!;
    private static readonly MethodInfo GraphicsPaletteMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.Palette), [typeof(int), typeof(int), typeof(int), typeof(int)])!;
    private static readonly MethodInfo GraphicsDrawTexturesMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.DrawTextures), [typeof(int), typeof(int), typeof(int), typeof(int)])!;
    private static readonly MethodInfo GraphicsLoadTextureMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.LoadTexture), [typeof(string), typeof(int)])!;

    private static readonly MethodInfo GraphicsDrawTextureMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.DrawTexture), [typeof(int), typeof(int), typeof(int)])!;
    private static readonly MethodInfo GraphicsSyncMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.Sync))!;
    private static readonly MethodInfo GraphicsSpriteLoadMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.SpriteLoad), [typeof(string), typeof(int)])!;
    private static readonly MethodInfo GraphicsSpriteSetPositionMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.SpriteSetPosition), [typeof(int), typeof(float), typeof(float)])!;
    private static readonly MethodInfo GraphicsSpriteSetVisibleMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.SpriteSetVisible), [typeof(int), typeof(bool)])!;
    private static readonly MethodInfo GraphicsSpriteSetPriorityMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.SpriteSetPriority), [typeof(int), typeof(int)])!;
    private static readonly MethodInfo GraphicsSpriteSetScaleMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.SpriteSetScale), [typeof(int), typeof(float), typeof(float)])!;
    private static readonly MethodInfo GraphicsSpriteSetRotationMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.SpriteSetRotation), [typeof(int), typeof(float)])!;
    private static readonly MethodInfo GraphicsSpriteSetTransformMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.SpriteSetTransform), [typeof(int), typeof(long)])!;
    private static readonly MethodInfo GraphicsSpriteSetMatrixMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.SpriteSetMatrix), [typeof(int), typeof(long)])!;
    private static readonly MethodInfo GraphicsSpriteGetPixelsMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.SpriteGetPixels), [typeof(int)])!;
    private static readonly MethodInfo GraphicsTilemapLoadMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.TilemapLoad), [typeof(int), typeof(int), typeof(int), typeof(int), typeof(int), typeof(int), typeof(int[])])!;
    private static readonly MethodInfo GraphicsTilemapSetMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.TilemapSet), [typeof(int), typeof(int), typeof(int)])!;
    private static readonly MethodInfo GraphicsTilemapSetOffsetMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.TilemapSetOffset), [typeof(float), typeof(float)])!;
    private static readonly MethodInfo GraphicsTilemapScrollMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.TilemapScroll), [typeof(float), typeof(float)])!;
    private static readonly MethodInfo GraphicsTilemapDrawMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.TilemapDraw))!;
    private static readonly MethodInfo GraphicsChartCreateMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.ChartCreate), [typeof(string), typeof(string), typeof(string)])!;
    private static readonly MethodInfo GraphicsChartPlotMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.ChartPlot), [typeof(long), typeof(double[]), typeof(double[]), typeof(string), typeof(string)])!;
    private static readonly MethodInfo GraphicsChartFinancialMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.ChartFinancial), [typeof(long), typeof(double[]), typeof(string), typeof(string)])!;

    private static readonly MethodInfo GraphicsChartShowMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.ChartShow), [typeof(long), typeof(int), typeof(int)])!;
    private static readonly MethodInfo GraphicsChartClearMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.ChartClear), [typeof(long)])!;
    private static readonly MethodInfo GraphicsChartCloseMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.ChartClose), [typeof(long)])!;
    private static readonly MethodInfo GraphicsChartSaveMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.ChartSave), [typeof(long), typeof(string)])!;
    private static readonly MethodInfo GraphicsPolygonMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.Polygon), [typeof(float[]), typeof(int)])!;
    private static readonly MethodInfo GraphicsImagePutsMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.ImagePuts), [typeof(int[]), typeof(int[]), typeof(int)])!;
    private static readonly MethodInfo GraphicsCClsMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.CCls))!;
    private static readonly MethodInfo GraphicsCPrintMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.CPrint), [typeof(string)])!;
    private static readonly MethodInfo GraphicsCPrintNewLineMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.CPrintNewLine))!;
    private static readonly MethodInfo GraphicsCLocateMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.CLocate), [typeof(int), typeof(int)])!;
    private static readonly MethodInfo GraphicsViewPortMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.ViewPort), [typeof(int), typeof(int), typeof(int), typeof(int)])!;
    private static readonly MethodInfo GraphicsBSaveMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.BSave), [typeof(int), typeof(int), typeof(int), typeof(int)])!;
    private static readonly MethodInfo GraphicsBLoadMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.BLoad), [typeof(int), typeof(int)])!;
    private static readonly MethodInfo GraphicsSaveScreenMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.SaveScreen), [typeof(string)])!;
    private static readonly MethodInfo GraphicsRestoreScreenMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.RestoreScreen), [typeof(string)])!;
    private static readonly MethodInfo GraphicsCSaveScreenMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.CSaveScreen), [typeof(string)])!;
    private static readonly MethodInfo GraphicsCRestoreScreenMethod =
        typeof(Runtime.IGraphicsService).GetMethod(nameof(Runtime.IGraphicsService.CRestoreScreen), [typeof(string)])!;
    private static readonly MethodInfo IlRuntimeMathRoundMethod = typeof(Math).GetMethod(nameof(Math.Round), new[] { typeof(double), typeof(MidpointRounding) })!;
    private static readonly MethodInfo IlRuntimeObjToStrMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ObjToStr), [typeof(object)])!;
    private static readonly MethodInfo IlRuntimeStringConcatMethod = typeof(string).GetMethod(nameof(string.Concat), [typeof(string), typeof(string)])!;
    private static readonly MethodInfo IlRuntimeReadIntArrayMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadIntArray), [typeof(object)])!;
    private static readonly MethodInfo IlRuntimeDrawTextureMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.DrawTexture))!;
    private static readonly MethodInfo IlRuntimeDrawTexturesMethod = typeof(IlRuntime).GetMethod(
        nameof(IlRuntime.DrawTextures), [typeof(int), typeof(double[]), typeof(double[]), typeof(double[]), typeof(double[])])!;
    private static readonly MethodInfo IlRuntimeDrawTexturesObjectsMethod = typeof(IlRuntime).GetMethod(
        nameof(IlRuntime.DrawTexturesObjects), [typeof(System.Array)])!;
    private static readonly MethodInfo IlRuntimeMathTruncateMethod =
        typeof(System.Math).GetMethod(nameof(System.Math.Truncate), [typeof(double)])!;
    private static readonly MethodInfo IlRuntimeCollideMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.SpriteCollide), [typeof(int), typeof(int)])!;
    private static readonly MethodInfo IlRuntimeCollideAllMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.SpriteCollideAll), [typeof(int)])!;
    private static readonly MethodInfo IlRuntimeCollideBatchMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.SpriteCollideBatch), [typeof(object), typeof(object), typeof(object), typeof(object)])!;
    private static readonly MethodInfo IlRuntimeChartCreateMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.ChartCreate), [])!;
    private static readonly MethodInfo IlRuntimeJoyCountMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.JoyCount), [])!;
    private static readonly MethodInfo IlRuntimeJoyPresentMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.JoyPresent), [typeof(int)])!;
    private static readonly MethodInfo IlRuntimeJoyNameMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.JoyName), [typeof(int)])!;
    private static readonly MethodInfo IlRuntimeJoyMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.Joy), [typeof(int)])!;
    private static readonly MethodInfo ConsoleSetScrollRegionMethod =
        typeof(Runtime.IConsoleService).GetMethod(nameof(Runtime.IConsoleService.SetScrollRegion), [typeof(int), typeof(int)])!;
    private static readonly MethodInfo FileCloseMethod =
        typeof(Runtime.IFileService).GetMethod(nameof(Runtime.IFileService.Close), [typeof(long)])!;
    private static readonly MethodInfo FileOpenMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.FileOpenNumber), [typeof(string), typeof(string), typeof(long)])!;
    /// <summary>OPEN con content type (1 = JSON): parità PCode — abilita il PRINT #h JSON.</summary>
    private static readonly MethodInfo FileOpenJsonMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.FileOpenNumber), [typeof(string), typeof(string), typeof(long), typeof(long)])!;
    /// <summary>PRINT #h: serializza JSON se l'handle è JSON, altrimenti Convert.ToString.</summary>
    private static readonly MethodInfo FileWriteNumberOrJsonMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.FileWriteNumberOrJson), [typeof(long), typeof(object)])!;
    private static readonly MethodInfo FileWriteNumberMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.FileWriteNumber), [typeof(long), typeof(string)])!;
    private static readonly MethodInfo FileWriteFieldMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.WriteField), [typeof(object)])!;
    private static readonly MethodInfo FileReadLineNumberMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.FileReadLineNumber), [typeof(long)])!;
    private static readonly MethodInfo FileIsJsonMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.FileIsJson), [typeof(long)])!;
    private static readonly MethodInfo FileReadJsonNumberMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.FileReadJsonNumber), [typeof(long)])!;
    private static readonly MethodInfo FileReadCsvLineNumberMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.FileReadCsvLineNumber), [typeof(long)])!;
    private static readonly MethodInfo CsvFieldToStringMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.CsvFieldToString), [typeof(string[]), typeof(int)])!;
    private static readonly MethodInfo CsvFieldToDoubleMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.CsvFieldToDouble), [typeof(string[]), typeof(int)])!;
    private static readonly MethodInfo FileCloseNumberMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.FileCloseNumber), [typeof(long)])!;
    private static readonly MethodInfo FileSeekMethod =
        typeof(Runtime.IFileService).GetMethod(nameof(Runtime.IFileService.Seek), [typeof(long), typeof(long), typeof(int)])!;
    private static readonly MethodInfo FileDeleteMethod =
        typeof(Runtime.IFileService).GetMethod(nameof(Runtime.IFileService.Delete), [typeof(string)])!;
    private static readonly MethodInfo FileMoveMethod =
        typeof(Runtime.IFileService).GetMethod(nameof(Runtime.IFileService.Move), [typeof(string), typeof(string)])!;
    private static readonly MethodInfo FileCreateDirectoryMethod =
        typeof(Runtime.IFileService).GetMethod(nameof(Runtime.IFileService.CreateDirectory), [typeof(string)])!;
    private static readonly MethodInfo FileSetCurrentDirectoryMethod =
        typeof(Runtime.IFileService).GetMethod(nameof(Runtime.IFileService.SetCurrentDirectory), [typeof(string)])!;
    private static readonly MethodInfo FileCopyMethod =
        typeof(Runtime.IFileService).GetMethod(nameof(Runtime.IFileService.Copy), [typeof(string), typeof(string)])!;
    private static readonly MethodInfo ThreadSleepMethod =
        typeof(Thread).GetMethod(nameof(Thread.Sleep), [typeof(int)])!;
    /// <summary>IslandRuntime.Sleep(double ms) — park cooperativo (non Thread.Sleep OS).</summary>
    private static readonly MethodInfo IslandRuntimeSleepMethod =
        typeof(IslandRuntime).GetMethod(nameof(IslandRuntime.Sleep), [typeof(double)])!;
    private static readonly MethodInfo IslandRuntimeSendInlineMethod =
        typeof(IslandRuntime).GetMethod(nameof(IslandRuntime.SendInline), [typeof(ChannelInfo), typeof(object)])!;
    private static readonly MethodInfo IslandRuntimeReceiveInlineMethod =
        typeof(IslandRuntime).GetMethod(nameof(IslandRuntime.ReceiveInline), [typeof(ChannelInfo)])!;
    private static readonly MethodInfo IslandRuntimeSendMethod =
        typeof(IslandRuntime).GetMethod(nameof(IslandRuntime.Send), [typeof(ChannelInfo), typeof(object)])!;
    private static readonly MethodInfo IslandRuntimeReceiveMethod =
        typeof(IslandRuntime).GetMethod(nameof(IslandRuntime.Receive), [typeof(ChannelInfo)])!;
    private static readonly MethodInfo IslandRuntimeFetchMethod =
        typeof(IslandRuntime).GetMethod(nameof(IslandRuntime.Fetch), [typeof(string)])!;
    private static readonly MethodInfo IlRuntimeHttpServerStartMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.HttpServerStart), [typeof(string), typeof(string)])!;
    private static readonly MethodInfo IlRuntimeHttpServerStopMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.HttpServerStop), Type.EmptyTypes)!;
    private static readonly MethodInfo IlRuntimeHttpConfigMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.HttpConfig), [typeof(object)])!;
    private static readonly MethodInfo IlRuntimeHttpAddRouteMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.HttpAddRoute), [typeof(string), typeof(string), typeof(int)])!;
    private static readonly MethodInfo IlRuntimeHttpAddWebSocketRouteMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.HttpAddWebSocketRoute), [typeof(string), typeof(int)])!;
    private static readonly MethodInfo IlRuntimeHttpAddStaticMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.HttpAddStatic), [typeof(string), typeof(string)])!;
    private static readonly MethodInfo IlRuntimeHttpAddMiddlewareMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.HttpAddMiddleware), [typeof(string), typeof(int)])!;
    private static readonly MethodInfo IlRuntimeHttpUseNextMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.HttpUseNext), Type.EmptyTypes)!;
    private static readonly MethodInfo SslConnectMethod =
        typeof(Runtime.INetworkService).GetMethod(nameof(Runtime.INetworkService.SslConnect), [typeof(string), typeof(int), typeof(string)])!;
    private static readonly MethodInfo SslListenMethod =
        typeof(Runtime.INetworkService).GetMethod(nameof(Runtime.INetworkService.SslListen), [typeof(int), typeof(string), typeof(string)])!;
    private static readonly MethodInfo IlRuntimeEventSubscribeMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.EventSubscribe), [typeof(object), typeof(string), typeof(object)])!;
    private static readonly MethodInfo IlRuntimeEventUnsubscribeMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.EventUnsubscribe), [typeof(object), typeof(string), typeof(object)])!;
    private static readonly MethodInfo IlRuntimeRaiseEventMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.RaiseEvent), [typeof(object), typeof(string), typeof(object[])])!;
    private static readonly MethodInfo IlRuntimeGetFileMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.GetFile), [typeof(long), typeof(long)])!;
    private static readonly MethodInfo IlRuntimePutFileMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.PutFile), [typeof(long), typeof(object)])!;
    private static readonly MethodInfo IlRuntimeMemoryMapMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.MemoryMap), [typeof(string)])!;
    private static readonly MethodInfo IlRuntimeMemoryPokeMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.MemoryPoke), [typeof(long), typeof(long), typeof(double)])!;
    private static readonly MethodInfo IlRuntimeMemoryPeekMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.MemoryPeek), [typeof(long), typeof(long)])!;
    private static readonly MethodInfo IlRuntimeMemoryReadBytesMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.MemoryReadBytes), [typeof(long), typeof(long), typeof(long)])!;
    private static readonly MethodInfo IlRuntimeMemoryWriteBytesMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.MemoryWriteBytes), [typeof(long), typeof(long), typeof(byte[]), typeof(long)])!;
    private static readonly MethodInfo NetworkSetOptionMethod =
        typeof(Runtime.INetworkService).GetMethod(nameof(Runtime.INetworkService.TcpSetOption), [typeof(long), typeof(string), typeof(string)])!;
    private static readonly MethodInfo NetworkShutdownMethod =
        typeof(Runtime.INetworkService).GetMethod(nameof(Runtime.INetworkService.TcpShutdown), [typeof(long), typeof(string)])!;
    private static readonly MethodInfo IlRuntimeClipMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.Clip), [typeof(string), typeof(string)])!;
    private static readonly MethodInfo IslandRuntimeWaitGroupWaitMethod =
        typeof(IslandRuntime).GetMethod(nameof(IslandRuntime.WaitGroupWait), [typeof(PCodeWaitGroup)])!;
    private static readonly MethodInfo IslandRuntimeWaitGroupDoneMethod =
        typeof(IslandRuntime).GetMethod(nameof(IslandRuntime.WaitGroupDone), [typeof(PCodeWaitGroup)])!;
    private static readonly MethodInfo EnumerableToArrayMethod =
        typeof(Enumerable).GetMethods(BindingFlags.Public | BindingFlags.Static)
            .Single(method => method.Name == nameof(Enumerable.ToArray) && method.IsGenericMethodDefinition && method.GetParameters().Length == 1);
    private static readonly MethodInfo ToTypedSetMethod =
        typeof(IlRuntime).GetMethods(BindingFlags.Public | BindingFlags.Static)
            .Single(method => method.Name == nameof(IlRuntime.ToTypedSet) && method.IsGenericMethodDefinition && method.GetParameters().Length == 1);
    private static readonly MethodInfo BagFromItemsMethod =
        typeof(IlRuntime).GetMethods(BindingFlags.Public | BindingFlags.Static)
            .Single(method => method.Name == nameof(IlRuntime.BagFromItems) && method.IsGenericMethodDefinition && method.GetParameters().Length == 1);
    private static readonly MethodInfo EnumerableToListMethod =
        typeof(Enumerable).GetMethods(BindingFlags.Public | BindingFlags.Static)
            .Single(method => method.Name == nameof(Enumerable.ToList) && method.IsGenericMethodDefinition && method.GetParameters().Length == 1);

    // arr[a:b] su una List<T> (non double[]): slice della lista (vedi IlRuntime.ArraySliceList).
    private static readonly MethodInfo ArraySliceListMethod =
        typeof(IlRuntime).GetMethods(BindingFlags.Public | BindingFlags.Static)
            .Single(method => method.Name == nameof(IlRuntime.ArraySliceList) && method.IsGenericMethodDefinition && method.GetParameters().Length == 3);

    // CommonLib.StringUtils è un assembly di progetto canonico unico (come BRT):
    // nessun problema di identità corelib, typeof() diretto va bene in ogni "mondo" di tipi.
    private static readonly MethodInfo StringUtilsLeftMethod =
        typeof(CommonLib.StringUtils).GetMethod(nameof(CommonLib.StringUtils.Left))!;

    private static readonly MethodInfo StringUtilsRightMethod =
        typeof(CommonLib.StringUtils).GetMethod(nameof(CommonLib.StringUtils.Right))!;

    private static readonly MethodInfo StringUtilsInstrMethod =
        typeof(CommonLib.StringUtils).GetMethod(nameof(CommonLib.StringUtils.Instr))!;

    // StringHelpers è un tipo interno a IlBackend (vedi StringHelpers.cs): stesso
    // ragionamento di identità di CommonLib/BRT, nessun problema di "mondo" di tipi.
    private static readonly MethodInfo StringHelpersMidToEndMethod =
        typeof(StringHelpers).GetMethod(nameof(StringHelpers.MidToEnd))!;

    private static readonly MethodInfo StringHelpersMidWithLengthMethod =
        typeof(StringHelpers).GetMethod(nameof(StringHelpers.MidWithLength))!;

    // Basic.IlBackend.ArrayHelpers (ArrayHelpers.cs) e CommonLib.ArrayUtils: stesso
    // ragionamento di identità di StringUtils/StringHelpers sopra — assembly di progetto
    // canonici, nessun problema di "due mondi" di tipi. I metodi generici restano OPEN
    // (MakeGenericMethod applicato al call-site, dove è noto il tipo elemento concreto).
    private static readonly MethodInfo ArrayHelpersACloneMethod =
        typeof(ArrayHelpers).GetMethod(nameof(ArrayHelpers.AClone))!;

    private static readonly MethodInfo ArrayHelpersAUniqueMethod =
        typeof(ArrayHelpers).GetMethod(nameof(ArrayHelpers.AUnique))!;

    private static readonly MethodInfo ArrayHelpersATokensMethod =
        typeof(ArrayHelpers).GetMethod(nameof(ArrayHelpers.ATokens))!;

    private static readonly MethodInfo ArrayHelpersAMapSeqMethod =
        typeof(ArrayHelpers).GetMethod(nameof(ArrayHelpers.AMapSeq))!;
    private static readonly MethodInfo ArrayHelpersAFilterSeqMethod =
        typeof(ArrayHelpers).GetMethod(nameof(ArrayHelpers.AFilterSeq))!;
    private static readonly MethodInfo ArrayHelpersAMapMethod =
        typeof(ArrayHelpers).GetMethod(nameof(ArrayHelpers.AMap))!;

    private static readonly MethodInfo ArrayHelpersAFilterNumericMethod =
        typeof(ArrayHelpers).GetMethod(nameof(ArrayHelpers.AFilterNumeric))!;
    private static readonly MethodInfo ArrayHelpersAFilterMethod =
        typeof(ArrayHelpers).GetMethod(nameof(ArrayHelpers.AFilter))!;

    private static readonly MethodInfo ArrayHelpersAReduceMethod =
        typeof(ArrayHelpers).GetMethod(nameof(ArrayHelpers.AReduce))!;

    private static readonly MethodInfo ArrayHelpersAReduceNoInitialMethod =
        typeof(ArrayHelpers).GetMethod(nameof(ArrayHelpers.AReduceNoInitial))!;

    private static readonly MethodInfo ArrayHelpersACountMethod =
        typeof(ArrayHelpers).GetMethod(nameof(ArrayHelpers.ACount))!;

    private static readonly MethodInfo ArrayHelpersAScanValueMethod =
        typeof(ArrayHelpers).GetMethod(nameof(ArrayHelpers.AScanValue))!;

    private static readonly MethodInfo ArrayHelpersAScanPredicateMethod =
        typeof(ArrayHelpers).GetMethod(nameof(ArrayHelpers.AScanPredicate))!;

    private static readonly MethodInfo ArrayUtilsReverseMethod =
        typeof(CommonLib.ArrayUtils).GetMethod(nameof(CommonLib.ArrayUtils.Reverse))!;

    private static readonly MethodInfo ArrayUtilsShuffleMethod =
        typeof(CommonLib.ArrayUtils).GetMethod(nameof(CommonLib.ArrayUtils.Shuffle))!;

    private static readonly MethodInfo ArrayUtilsBinarySearchMethod =
        typeof(CommonLib.ArrayUtils).GetMethod(nameof(CommonLib.ArrayUtils.BinarySearch))!;

    private static readonly MethodInfo ArrayUtilsJoinMethod =
        typeof(CommonLib.ArrayUtils).GetMethod(nameof(CommonLib.ArrayUtils.Join))!;

    private static readonly MethodInfo ArrayUtilsFlattenMethod =
        typeof(CommonLib.ArrayUtils).GetMethod(nameof(CommonLib.ArrayUtils.Flatten))!;

    private static readonly MethodInfo ArrayUtilsPadMethod =
        typeof(CommonLib.ArrayUtils).GetMethod(nameof(CommonLib.ArrayUtils.Pad))!;

    private static readonly MethodInfo ArrayHelpersSortArrayMethod =
        typeof(Array).GetMethod(nameof(Array.Sort), [typeof(Array)])!;
    private static readonly MethodInfo ArrayHelpersReverseArrayMethod =
        typeof(Array).GetMethod(nameof(Array.Reverse), [typeof(Array)])!;
    private static readonly MethodInfo IlRuntimeFetchResultSaveToFileMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FetchResultSaveToFile))!;
    private static readonly MethodInfo IlRuntimeFetchFunctionMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FetchFunction))!;
    private static readonly MethodInfo IlRuntimeWsConnectMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WsConnect))!;
    private static readonly MethodInfo IlRuntimeWsSetHandlerMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WsSetHandler))!;
    private static readonly MethodInfo IlRuntimeLenMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Len))!;
    private static readonly MethodInfo IlRuntimeLenBytesMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.LenBytes))!;
    private static readonly MethodInfo IlRuntimeChunkMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Chunk))!;
    private static readonly MethodInfo IlRuntimeZipArraysMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ZipArrays))!;
    private static readonly MethodInfo ArrayHelpersConvertArrayMethod =
        typeof(ArrayHelpers).GetMethod(nameof(ArrayHelpers.ConvertArray))!;
    private static readonly MethodInfo IlRuntimeRectToJaggedMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.RectToJagged))!;
    private static readonly MethodInfo ArrayHelpersNewInstanceArrayMethod =
        typeof(ArrayHelpers).GetMethod(nameof(ArrayHelpers.NewInstanceArray))!;
    private static readonly MethodInfo ArrayHelpersNewStringArrayMethod =
        typeof(ArrayHelpers).GetMethod(nameof(ArrayHelpers.NewStringArray))!;
    private static readonly MethodInfo ArrayHelpersASortMethod =
        typeof(ArrayHelpers).GetMethod(nameof(ArrayHelpers.ASort))!;

    private static readonly MethodInfo ArrayHelpersASortWithComparerMethod =
        typeof(ArrayHelpers).GetMethod(nameof(ArrayHelpers.ASortWithComparer))!;

    private static readonly MethodInfo ArrayHelpersAEvalMethod =
        typeof(ArrayHelpers).GetMethod(nameof(ArrayHelpers.AEval))!;

    private static readonly MethodInfo ArrayHelpersAEvalIndexedMethod =
        typeof(ArrayHelpers).GetMethod(nameof(ArrayHelpers.AEvalIndexed))!;

    private static readonly MethodInfo FileLockMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.FileLock), [typeof(long), typeof(long), typeof(long)])!;
    private static readonly MethodInfo FileUnlockMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.FileUnlock), [typeof(long), typeof(long), typeof(long)])!;

    // Network
    private static readonly MethodInfo TcpConnectMethod = typeof(Runtime.INetworkService).GetMethod(nameof(Runtime.INetworkService.TcpConnect))!;
    private static readonly MethodInfo TcpCloseMethod = typeof(Runtime.INetworkService).GetMethod(nameof(Runtime.INetworkService.TcpClose))!;
    private static readonly MethodInfo TcpSendMethod = typeof(Runtime.INetworkService).GetMethod(nameof(Runtime.INetworkService.TcpSend))!;
    private static readonly MethodInfo TcpReceiveMethod = typeof(Runtime.INetworkService).GetMethod(nameof(Runtime.INetworkService.TcpReceive))!;
    private static readonly MethodInfo UdpSendMethod = typeof(Runtime.INetworkService).GetMethod(nameof(Runtime.INetworkService.UdpSend))!;
    private static readonly MethodInfo UdpCloseMethod = typeof(Runtime.INetworkService).GetMethod(nameof(Runtime.INetworkService.UdpClose))!;
    private static readonly MethodInfo UdpOpenMethod = typeof(Runtime.INetworkService).GetMethod(nameof(Runtime.INetworkService.UdpOpen))!;
    private static readonly MethodInfo UdpReceiveMethod = typeof(Runtime.INetworkService).GetMethod(nameof(Runtime.INetworkService.UdpReceive))!;
    private static readonly MethodInfo IlRuntimeUdpReceiveUnpackMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.UdpReceiveUnpack))!;
    // Convertitori CONDIVISI col park v2 (EmitTcpRecvPark): byte[]? → stringa / lunghezza double.
    private static readonly MethodInfo IlRuntimeNetworkDataToStringMethod =
        typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.NetworkDataToString))!;
    private static readonly MethodInfo IlRuntimeBytesLengthMethod =
        typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.BytesLength))!;
    private static readonly MethodInfo IlRuntimeLocalPortMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.LocalPort))!;
    private static readonly MethodInfo IlRuntimeMutexTryLockMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.MutexTryLock))!;
    // MESSAGE.DELIVERY(bus) — contatori di consegna (handle numerico assegnato da MESSAGE CONNECT).
    private static readonly MethodInfo IlRuntimeMessageDeliveryMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.MessageDelivery))!;
    private static readonly MethodInfo IlRuntimeContextBackgroundMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.ContextBackground))!;
    private static readonly MethodInfo IlRuntimeContextWithCancelMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.ContextWithCancel))!;
    private static readonly MethodInfo IlRuntimeContextCancelMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.ContextCancel))!;
    private static readonly MethodInfo IlRuntimeContextDoneMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.ContextDone))!;
    private static readonly MethodInfo IlRuntimeContextCancelledMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.ContextCancelled))!;
    private static readonly MethodInfo IlRuntimePortMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.Port))!;
    private static readonly MethodInfo IlRuntimeRemoteMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.Remote))!;
    private static readonly MethodInfo TcpOpenMethod = typeof(Runtime.INetworkService).GetMethod(nameof(Runtime.INetworkService.TcpListen))!;
    private static readonly MethodInfo TcpAcceptMethod = typeof(Runtime.INetworkService).GetMethod(nameof(Runtime.INetworkService.TcpAccept))!;
    private static readonly MethodInfo SslCloseMethod = typeof(Runtime.INetworkService).GetMethod(nameof(Runtime.INetworkService.SslClose))!;
    private static readonly MethodInfo ValueToBytesMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.ValueToBytes))!;
    private static readonly MethodInfo ConsoleMenuMethod = typeof(Runtime.IConsoleService).GetMethod(nameof(Runtime.IConsoleService.Menu))!;
    private static readonly MethodInfo ConsoleFileDialogMethod = typeof(Runtime.IConsoleService).GetMethod(nameof(Runtime.IConsoleService.FileDialog))!;
    private static readonly MethodInfo ConsoleColorPickerMethod = typeof(Runtime.IConsoleService).GetMethod(nameof(Runtime.IConsoleService.ColorPicker))!;
    private static readonly MethodInfo ConsoleDatePickerMethod = typeof(Runtime.IConsoleService).GetMethod(nameof(Runtime.IConsoleService.DatePicker))!;
    private static readonly MethodInfo ConsoleSpinnerMethod = typeof(Runtime.IConsoleService).GetMethod(nameof(Runtime.IConsoleService.Spinner))!;
    private static readonly MethodInfo ConsoleToggleMethod = typeof(Runtime.IConsoleService).GetMethod(nameof(Runtime.IConsoleService.Toggle))!;
    private static readonly MethodInfo ConsoleSliderMethod = typeof(Runtime.IConsoleService).GetMethod(nameof(Runtime.IConsoleService.Slider))!;
    private static readonly MethodInfo IlRuntimeStringifyArrayMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.StringifyArray))!;
    private static readonly MethodInfo IlRuntimeCharToStringMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.CharToString))!;
    private static readonly MethodInfo IlRuntimeReadKeyMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadKey), Type.EmptyTypes)!;
    private static readonly MethodInfo ConsoleReadKeyMethod = typeof(System.Console).GetMethod(nameof(System.Console.ReadKey), Type.EmptyTypes)!;
    private static readonly MethodInfo ConsoleKeyInfoKeyCharGetter = typeof(ConsoleKeyInfo).GetProperty(nameof(ConsoleKeyInfo.KeyChar))!.GetGetMethod()!;
    private static readonly MethodInfo IlRuntimeJsonSaveMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.JsonSave), [typeof(object), typeof(string)])!;
    private static readonly MethodInfo IlRuntimeJsonWriteValueMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.JsonWriteValue), [typeof(string), typeof(object), typeof(string)])!;
    private static readonly MethodInfo IlRuntimeJsonWriteSectionMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.JsonWriteSection), [typeof(object), typeof(string)])!;
    private static readonly MethodInfo IlRuntimeIoWatchMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.IoWatch), [typeof(string), typeof(int), typeof(string)])!;
    private static readonly MethodInfo IlRuntimeIoUnwatchMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.IoUnwatch), [typeof(string)])!;
    private static readonly MethodInfo IlRuntimeSetDateFormatMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.SetDateFormat), [typeof(string)])!;
    private static readonly MethodInfo IlRuntimeSetThemeMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.SetTheme), [typeof(string)])!;
    private static readonly MethodInfo IlRuntimeDrawBoxMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.DrawBox))!;
    private static readonly MethodInfo IlRuntimeReadPolygonPointsMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadPolygonPoints), [typeof(object)])!;
    private static readonly MethodInfo IlRuntimeTuiCheckBoxMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiCheckBox))!;
    private static readonly MethodInfo IlRuntimeTuiListBoxMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiListBox))!;
    private static readonly MethodInfo IlRuntimeTuiComboBoxMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiComboBox))!;
    private static readonly MethodInfo IlRuntimeTuiToggleMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiToggle))!;
    private static readonly MethodInfo IlRuntimeTuiRadioGroupMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiRadioGroup))!;
    private static readonly MethodInfo IlRuntimeTuiSliderMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiSlider))!;
    private static readonly MethodInfo IlRuntimeTuiNumericUpDownMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiNumericUpDown))!;
    private static readonly MethodInfo IlRuntimeTuiStarRatingMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiStarRating))!;
    private static readonly MethodInfo IlRuntimeTuiScrollBarMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiScrollBar))!;
    private static readonly MethodInfo IlRuntimeTuiScrollViewMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiScrollView))!;
    private static readonly MethodInfo IlRuntimeTuiLabelMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiLabel))!;
    private static readonly MethodInfo IlRuntimeTuiDataListMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiDataList))!;
    private static readonly MethodInfo IlRuntimeTuiPropertyGridMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiPropertyGrid))!;
    private static readonly MethodInfo IlRuntimeTuiLogViewMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiLogView))!;
    private static readonly MethodInfo IlRuntimeTuiHexViewMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiHexView))!;
    private static readonly MethodInfo IlRuntimeTuiBigTextMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiBigText))!;
    private static readonly MethodInfo IlRuntimeTuiCodeAreaMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiCodeArea))!;
    private static readonly MethodInfo IlRuntimeTuiReorderListBoxMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiReorderListBox))!;
    private static readonly MethodInfo IlRuntimeTuiAlertBoxMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiAlertBox))!;
    private static readonly MethodInfo IlRuntimeTuiConfirmationBoxMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiConfirmationBox))!;
    private static readonly MethodInfo IlRuntimeTuiFileExplorerMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiFileExplorer))!;
    private static readonly MethodInfo IlRuntimeTuiDashboardMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiDashboard))!;
    private static readonly MethodInfo IlRuntimeTuiSplitterMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiSplitter))!;
    private static readonly MethodInfo IlRuntimeTuiContextMenuMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiContextMenu))!;
    private static readonly MethodInfo IlRuntimeTuiSpinnerMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiSpinner))!;
    private static readonly MethodInfo IlRuntimeTuiTabMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiTab))!;
    private static readonly MethodInfo IlRuntimeTuiGridMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiGrid))!;
    private static readonly MethodInfo IlRuntimeTuiDatePickerMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiDatePicker))!;
    private static readonly MethodInfo IlRuntimeTuiColorPickerMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiColorPicker))!;
    private static readonly MethodInfo IlRuntimeTuiTreeViewMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiTreeView))!;
    private static readonly MethodInfo IlRuntimeTuiMenuMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiMenu))!;
    private static readonly MethodInfo IlRuntimeTuiFileDialogMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiFileDialog))!;
    private static readonly MethodInfo IlRuntimeTuiPrinterDialogMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiPrinterDialog))!;
    private static readonly MethodInfo IlRuntimeTuiTBrowseMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TuiTBrowse))!;
    private static readonly MethodInfo IlRuntimeArrayToDoubleArrayMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ArrayToDoubleArray), [typeof(object)])!;
    private static readonly MethodInfo IlRuntimeArrayToIntArrayMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ArrayToIntArray), [typeof(object)])!;
    private static readonly MethodInfo IlGpmRuntimeThrowIfWorkerMethod =
        typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.ThrowIfWorker), [typeof(string)])!;
    private static readonly MethodInfo IlRuntimeEmptyDoubleArrayMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.EmptyDoubleArray))!;
    private static readonly MethodInfo IlRuntimeCopyIntArrayToMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.CopyIntArrayTo), [typeof(int[]), typeof(Array)])!;
    private static readonly MethodInfo IlRuntimeTickBeginMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TickBegin), [typeof(int)])!;
    private static readonly MethodInfo IlRuntimeTickEndMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TickEnd), [typeof(int)])!;
    private static readonly MethodInfo IlRuntimeAtGetMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.AtGet))!;
    private static readonly MethodInfo IlRuntimeStatusBarMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.StatusBar))!;
    private static readonly MethodInfo IlRuntimeScrollRegionMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ScrollRegion))!;
    private static readonly MethodInfo IlRuntimeSetScrollRegionMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.SetScrollRegion))!;
    private static readonly MethodInfo IlRuntimeClearRectMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ClearRect))!;
    private static readonly MethodInfo IlRuntimeCursorVisibleMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.CursorVisible))!;
    private static readonly MethodInfo IlRuntimeSetShellEnabledMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.SetShellEnabled))!;
    private static readonly MethodInfo IlRuntimeCursorShapeMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.CursorShape))!;
    private static readonly MethodInfo IlRuntimeConsoleSendKeysMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ConsoleSendKeys))!;
    private static readonly MethodInfo IlRuntimeSetScreenModeMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.SetScreenMode))!;
    private static readonly MethodInfo IlRuntimeSaveScreenMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.SaveScreenBuffer))!;
    private static readonly MethodInfo IlRuntimeRestoreScreenMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.RestoreScreenBuffer))!;
    private static readonly MethodInfo IlRuntimeSetWindowRegionMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.SetWindowRegion))!;
    private static readonly MethodInfo IlRuntimeSetAttributeMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.SetConsoleAttribute))!;
    private static readonly MethodInfo IlRuntimeDefineWindowMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.DefineWindow), [typeof(string), typeof(int), typeof(int), typeof(int), typeof(int), typeof(string), typeof(bool), typeof(bool), typeof(bool), typeof(int), typeof(int)])!;
    private static readonly MethodInfo IlRuntimeWindowActionMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WindowAction))!;
    private static readonly MethodInfo IlRuntimeWindowSetVisibleMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WindowSetVisible))!;
    private static readonly MethodInfo IlRuntimeWindowSetTitleMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WindowSetTitle))!;
    private static readonly MethodInfo IlRuntimeWindowAppendMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WindowAppendLine))!;
    private static readonly MethodInfo IlRuntimeWindowSetContentMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WindowSetContent))!;
    private static readonly MethodInfo IlRuntimeWindowClearMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WindowClear))!;
    private static readonly MethodInfo IlRuntimeWindowScrollMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WindowScroll))!;
    private static readonly MethodInfo IlRuntimeWindowEditMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WindowEdit))!;
    private static readonly MethodInfo IlRuntimeWindowSelectMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WindowSelect))!;
    private static readonly MethodInfo IlRuntimeWindowThemeMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WindowTheme))!;
    private static readonly MethodInfo IlRuntimeWindowDesktopMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WindowDesktop))!;
    private static readonly MethodInfo IlRuntimeWindowMoveDesktopMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WindowMoveDesktop))!;
    private static readonly MethodInfo IlRuntimeWindowFrameMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WindowFrame))!;
    private static readonly MethodInfo IlRuntimeWindowShadowMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WindowShadow))!;
    private static readonly MethodInfo IlRuntimeAtSayDispatchMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.AtSayDispatch))!;
    private static readonly MethodInfo IlRuntimeReadAddFieldMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadAddField))!;
    private static readonly MethodInfo IlRuntimeReadGetValueMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadGetValue))!;
    private static readonly MethodInfo IlRuntimeReadClearMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadClear))!;
    private static readonly MethodInfo IlRuntimeToDoubleMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ToDouble), new[] { typeof(object) })!;
    private static readonly MethodInfo IlRuntimeReadLineMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadLine))!;
    private static readonly MethodInfo IlRuntimeReadRenderFieldMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadRenderField))!;
    private static readonly MethodInfo IlRuntimeReadNativeAddMethod = typeof(Basic.IlBackend.IlGpmRuntime).GetMethod(nameof(Basic.IlBackend.IlGpmRuntime.ReadNativeAdd))!;
    private static readonly MethodInfo IlRuntimeReadSetMessageMethod = typeof(Basic.IlBackend.IlGpmRuntime).GetMethod(nameof(Basic.IlBackend.IlGpmRuntime.ReadNativeSetMessage))!;
    private static readonly MethodInfo IlRuntimeReadSetErrorMethod = typeof(Basic.IlBackend.IlGpmRuntime).GetMethod(nameof(Basic.IlBackend.IlGpmRuntime.ReadNativeSetError))!;
    private static readonly MethodInfo IlRuntimeReadSequentialFieldMethod = typeof(Basic.IlBackend.IlGpmRuntime).GetMethod(nameof(Basic.IlBackend.IlGpmRuntime.ReadSequentialField))!;
    private static readonly MethodInfo IlRuntimeReadNativeResetMethod = typeof(Basic.IlBackend.IlGpmRuntime).GetMethod(nameof(Basic.IlBackend.IlGpmRuntime.ReadNativeReset))!;
    private static readonly MethodInfo IlRuntimeReadNativeRunMethod = typeof(Basic.IlBackend.IlGpmRuntime).GetMethod(nameof(Basic.IlBackend.IlGpmRuntime.ReadNativeRun))!;
    private static readonly MethodInfo IlRuntimeReadNativeValueMethod = typeof(Basic.IlBackend.IlGpmRuntime).GetMethod(nameof(Basic.IlBackend.IlGpmRuntime.ReadNativeValue))!;
    private static readonly MethodInfo IlRuntimeMouseEnableMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MouseEnable))!;
    private static readonly MethodInfo IlRuntimeMouseSetPosMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MouseSetPos))!;
    private static readonly MethodInfo IlRuntimeMouseShowMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MouseShow))!;
    private static readonly MethodInfo IlRuntimeMouseCursorMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MouseCursor))!;
    private static readonly MethodInfo IlRuntimeMouseLimitMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MouseLimit))!;
    private static readonly MethodInfo IlRuntimeMouseWaitMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MouseWait))!;
    private static readonly MethodInfo IlRuntimePrinterDialogMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.PrinterDialog))!;
    private static readonly MethodInfo ConsoleSetPaletteMethod = typeof(Runtime.IConsoleService).GetMethod(nameof(Runtime.IConsoleService.SetPaletteColor))!;
    private static readonly MethodInfo IlRuntimeAgentCreateMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.AgentCreate))!;
    private static readonly MethodInfo IlRuntimeAgentDeleteMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.AgentDelete))!;
    private static readonly MethodInfo IlRuntimeAgentSaveMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.AgentSave))!;
    private static readonly MethodInfo IlRuntimeAgentLoadMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.AgentLoad))!;
    private static readonly MethodInfo IlRuntimeTeamCreateMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamCreate))!;
    private static readonly MethodInfo IlRuntimeTeamAddAgentMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamAddAgent))!;
    private static readonly MethodInfo IlRuntimeTeamRemoveAgentMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamRemoveAgent))!;
    private static readonly MethodInfo IlRuntimeTeamMessageMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamMessage))!;
    private static readonly MethodInfo IlRuntimeMessageConnectMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MessageConnect))!;
    private static readonly MethodInfo IlRuntimeMessageSubscribeMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MessageSubscribe))!;
    private static readonly MethodInfo IlRuntimeMessageUnsubscribeMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MessageUnsubscribe))!;
    private static readonly MethodInfo IlRuntimeMessagePublishMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MessagePublish))!;
    private static readonly MethodInfo IlRuntimeMessagePingMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MessagePing))!;
    private static readonly MethodInfo IlRuntimeMessageCloseMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MessageClose))!;
    private static readonly MethodInfo IlRuntimeMessageHandlerBySymbolMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.RegisterMessageHandlerBySymbol))!;
    private static readonly MethodInfo IlRuntimeSignalSubscribeMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.SignalSubscribe))!;
    private static readonly MethodInfo IlRuntimeSignalEmitMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.SignalEmit))!;
    private static readonly MethodInfo IlRuntimeSignalUnsubscribeMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.SignalUnsubscribe))!;
    private static readonly MethodInfo IlRuntimeSignalPingMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.SignalPing))!;
    private static readonly MethodInfo IlRuntimeSignalHandlerBySymbolMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.RegisterSignalHandlerBySymbol))!;
    private static readonly MethodInfo IlRuntimeSignalUnregisterHandlerMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.UnregisterSignalHandler))!;
    private static readonly MethodInfo IlRuntimeTeamSetRoleMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamSetRole), [typeof(string), typeof(string), typeof(string)])!;
    private static readonly MethodInfo IlRuntimeTeamSetStrategyMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamSetStrategy), [typeof(string), typeof(string)])!;
    private static readonly MethodInfo IlRuntimeTeamSetCoordinatorMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamSetCoordinator), [typeof(string), typeof(string)])!;
    private static readonly MethodInfo IlRuntimeTeamSaveMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamSave), [typeof(string)])!;
    private static readonly MethodInfo IlRuntimeTeamLoadMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamLoad), [typeof(string)])!;
    private static readonly MethodInfo IlRuntimeTeamRunMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamRun), [typeof(string), typeof(string)])!;
    private static readonly MethodInfo IlRuntimeTeamSetRecursionMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamSetRecursion))!;
    private static readonly MethodInfo IlRuntimeTeamSetLatentMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamSetLatent))!;
    private static readonly MethodInfo IlRuntimeTeamSetLinkMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamSetLink))!;
    private static readonly MethodInfo IlRuntimeTeamSetThoughtsMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamSetThoughts))!;
    private static readonly MethodInfo IlRuntimeTeamTrainInnerMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamTrainInner))!;
    private static readonly MethodInfo IlRuntimeTeamTrainOuterMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamTrainOuter))!;
    private static readonly MethodInfo IlRuntimeTeamSetDagMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamSetDag))!;
    private static readonly MethodInfo IlRuntimeTeamSetComplexityMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamSetComplexity))!;
    private static readonly MethodInfo IlRuntimeTeamSetReflectionMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamSetReflection))!;
    private static readonly MethodInfo IlRuntimeTeamSetCriticMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamSetCritic))!;
    private static readonly MethodInfo IlRuntimeTeamSetPruneMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamSetPrune))!;
    private static readonly MethodInfo IlRuntimeTeamTrainThinkingMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamTrainThinking))!;
    private static readonly MethodInfo IlRuntimeTeamTrainActorCriticMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamTrainActorCritic))!;
    private static readonly MethodInfo IlRuntimeTeamDistillThinkingMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamDistillThinking))!;
    private static readonly MethodInfo IlRuntimeTeamBroadcastMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamBroadcast))!;
    private static readonly MethodInfo IlRuntimeTeamRedefineMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TeamRedefine))!;
    private static readonly MethodInfo IlRuntimeWorkflowDefineMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WorkflowDefine))!;
    private static readonly MethodInfo IlRuntimeWorkflowStepMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WorkflowStep))!;
    private static readonly MethodInfo IlRuntimeWorkflowEndMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WorkflowEnd))!;
    private static readonly MethodInfo IlRuntimeWorkflowExecuteMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.WorkflowExecute))!;
    private static readonly MethodInfo IlRuntimeMacroEvalMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MacroEval))!;
    private static readonly MethodInfo IlRuntimeMacroEvalExprMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MacroEvalExpr))!;
    private static readonly MethodInfo IlRuntimeMacroEvalStmtWithLocalsMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MacroEvalWithLocals))!;
    private static readonly MethodInfo IlRuntimeMacroEvalExprWithLocalsMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MacroEvalExprWithLocals))!;
    private static readonly MethodInfo IlRuntimeCoerceEvalLocalMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.CoerceEvalLocal))!;
    private static readonly MethodInfo GetTypeFromHandleMethod = typeof(Type).GetMethod(nameof(Type.GetTypeFromHandle))!;
    private static readonly MethodInfo IlRuntimeRunCommandMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.RunCommand))!;
    private static readonly MethodInfo SqlExecuteMethod = typeof(Runtime.IDatabaseService).GetMethod(nameof(Runtime.IDatabaseService.SqlExecute))!;
    private static readonly MethodInfo SqlCloseMethod = typeof(Runtime.IDatabaseService).GetMethod(nameof(Runtime.IDatabaseService.SqlClose))!;
    private static readonly MethodInfo SqlQueryMethod = typeof(Runtime.IDatabaseService).GetMethod(nameof(Runtime.IDatabaseService.SqlQuery))!;
    private static readonly MethodInfo SqlOpenMethod = typeof(Runtime.IDatabaseService).GetMethod(nameof(Runtime.IDatabaseService.SqlOpen))!;
    private static readonly MethodInfo SqlLastInsertRowIdMethod = typeof(Runtime.IDatabaseService).GetMethod(nameof(Runtime.IDatabaseService.SqlLastInsertRowId))!;

    // ============================================================================
    // CLOSURES / LAMBDA — FN(...) => espressione, FN(...) AS T: ... END FN
    // ============================================================================
    //
    // MVP scope: una lambda cattura per VALORE (snapshot al momento della creazione,
    // mai un riferimento condiviso/mutabile all'outer): se la stessa variabile è
    // catturata da più lambda, ognuna riceve una COPIA indipendente. Questo evita di
    // dover "promuovere" (boxare) la variabile nello scope ESTERNO — che resta un
    // normale locale CLR, invariato — a costo di non supportare la mutazione
    // condivisa tra chiusure (semantica "capture by value", non "capture by
    // reference" — sufficiente per l'uso più comune, funzioni di ordine superiore
    // come AMAP/AFILTER). Una lambda ANNIDATA dentro un'altra lambda è supportata: le
    // sue catture si propagano transitivamente verso l'esterno (vedi
    // CollectLambdaCaptures, caso LambdaExpr in CollectReferencedSymbolsInExpr) — ciò
    // che la nidificata prende dal proprio scope enclosing, se non soddisfatto da un
    // parametro/locale proprio, diventa a sua volta una cattura della lambda che la
    // contiene, ricorsivamente su qualunque profondità di annidamento.
    //
    // Rappresentazione: nessuna cattura → metodo STATIC sul tipo Program, il valore
    // lambda è un delegate legato a null (ldnull+ldftn+newobj). Con catture → una
    // CLASSE sintetica (reference type, un campo per cattura) con un metodo
    // d'istanza "Invoke"; il valore lambda è quell'istanza (popolata con gli
    // snapshot correnti al momento della NEW implicita) legata come target del
    // delegate (ldarg/dup+ldftn+newobj).
}
