// Esegue i sample BASIC SciPy (samples/Math/*SciPyParity.bas) sul P-Code
// managed e verifica che l'output coincida con gli oracoli SciPy 1.18 (scipy.special,
// scipy.spatial.distance) entro 1e-9. Non interattivo (solo filtro mirato).
using System.IO;
using System.Text.RegularExpressions;
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class SciPySamplesParityTests
{
    // I sample SciPy vivono in `shared/samples/Math` (dal 2026-10 la cartella `samples/` è
    // stata spostata sotto `shared/`). Il percorso vecchio (`samples/Community/Math`) faceva
    // fallire TUTTA la classe con "sample mancante" prima di misurare qualunque numero.
    private static readonly string SamplesDir =
        Path.Combine(FindRepoRoot(), "shared", "samples", "Math");

    /// <summary>Radice del monorepo: la directory che contiene GMBasicSharp/ e shared/.
    /// Non si usa un file marcatore (`GMBASIC.sln`/`AGENTS.md`): il primo non esiste più, il
    /// secondo non è versionato. Le due directory sono stabili nel checkout.</summary>
    private static string FindRepoRoot()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir != null)
        {
            if (Directory.Exists(Path.Combine(dir.FullName, "GMBasicSharp"))
                && Directory.Exists(Path.Combine(dir.FullName, "shared")))
                return dir.FullName;
            dir = dir.Parent;
        }
        return ".";
    }

    private static string RunSample(string name)
    {
        var file = Path.Combine(SamplesDir, name);
        C99Emission.Require(file);
        Assert.True(File.Exists(file), $"sample mancante: {file}");
        var program = Basic.Orchestrator.Compile(file, null, false, "il");
        var chunk = new global::Compiler.Compiler().Compile(program);
        var originalOut = Console.Out;
        var sw = new StringWriter();
        try
        {
            Console.SetOut(sw);
            Basic.IlBackend.IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
            var r = global::Basic.IlBackend.PCode.PCodeVmRunner.Run(chunk);
            Assert.True(r.Success, $"{name} fallito:\n{r.Output}");
            return r.Output;
        }
        finally { Console.SetOut(originalOut); }
    }

    // Percorso IL: compila a IL assembly, invoca GeneratedProgram.Main, cattura l'output.
    private static string RunSampleIl(string name)
    {
        var file = Path.Combine(SamplesDir, name);
        C99Emission.Require(file);
        Assert.True(File.Exists(file), $"sample mancante: {file}");
        var program = Basic.Orchestrator.Compile(file, null, false, "il");
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "IlPar_" + name.Replace('.', '_'), out _);
        var dll = Path.Combine(Path.GetTempPath(), "IlPar_" + name.Replace('.', '_') + ".dll");
        using (var fs = File.Create(dll)) asmBuilder.Save(fs);
        var assembly = System.Reflection.Assembly.LoadFile(dll);
        var main = assembly.GetType("GeneratedProgram")!.GetMethod("Main", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static)!;
        var originalOut = Console.Out;
        var sw = new StringWriter();
        try
        {
            Console.SetOut(sw);
            Basic.IlBackend.IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
            var task = Task.Run(() => main.Invoke(null, null));
            Assert.True(task.Wait(TimeSpan.FromSeconds(20)), $"IL TIMEOUT {name}\n{sw}");
            task.GetAwaiter().GetResult();
            return sw.ToString();
        }
        finally
        {
            Console.SetOut(originalOut);
            try { File.Delete(dll); } catch { }
        }
    }

    private static void AssertValue(string output, string label, double expected)
        => AssertValue(output, label, expected, 1e-9);

    private static void AssertValue(string output, string label, double expected, double tol)
    {
        var m = Regex.Match(output, Regex.Escape(label) + @"\s*=\s*(-?[0-9.]+(?:[eE][+-]?\d+)?)");
        Assert.True(m.Success, $"etichetta '{label}' non trovata in:\n{output}");
        var got = double.Parse(m.Groups[1].Value, System.Globalization.CultureInfo.InvariantCulture);
        Assert.True(Math.Abs(got - expected) <= tol,
            $"{label}: atteso {expected}, ottenuto {got}");
    }

    [Fact]
    public void SpecialSample_Matches_SciPy()
    {
        var out_ = RunSample("SpecialSciPyParity.bas");
        AssertValue(out_, "gamma(5)", 24.0);
        AssertValue(out_, "gamma(0.5)", 1.7724538509055159);
        AssertValue(out_, "lgamma(5)", 3.1780538303479458);
        AssertValue(out_, "beta(2,3)", 0.08333333333333333);
        AssertValue(out_, "erf(0.5)", 0.5204998778130465);
        AssertValue(out_, "erfc(0.5)", 0.4795001221869535);
        AssertValue(out_, "gammainc", 0.8008517265285442);
        AssertValue(out_, "gammaincc", 0.1991482734714558);
        AssertValue(out_, "betainc", 0.6875);
        AssertValue(out_, "digamma(3)", 0.9227843350984671);
        AssertValue(out_, "jv(1,2)", 0.5767248077568736);
        AssertValue(out_, "yv(1,2)", -0.1070324315409375);
        AssertValue(out_, "iv(1,2)", 1.590636854637329);
        AssertValue(out_, "kv(1,2)", 0.13986588181652246);
        AssertValue(out_, "betaln", -2.4849066497880004);
        AssertValue(out_, "erfinv(.5)", 0.4769362762044699);
        AssertValue(out_, "erfcinv(.5)", 0.4769362762044699);
        AssertValue(out_, "gammaincinv", 2.994308347002123);
        AssertValue(out_, "gammainccinv", 2.9943083470021223);
        AssertValue(out_, "betaincinv", 0.3857275681323895);
    }

    [Fact]
    public void VectorSample_Matches_SciPy()
    {
        var out_ = RunSample("VectorDistanceSciPyParity.bas");
        AssertValue(out_, "manhattan", 12.0);
        AssertValue(out_, "chebyshev", 5.0);
        AssertValue(out_, "cosine", 0.007416666029069763);
        AssertValue(out_, "minkowski", 6.0);
    }

    private static void AssertArray(string output, string label, double[] expected, double tol = 1e-9)
    {
        var m = Regex.Match(output, Regex.Escape(label) + @"\s*=\s*(.*?)$", RegexOptions.Multiline);
        Assert.True(m.Success, $"etichetta '{label}' non trovata in:\n{output}");
        var parts = Regex.Matches(m.Groups[1].Value.Trim(), @"-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?")
            .Select(x => double.Parse(x.Value, System.Globalization.CultureInfo.InvariantCulture)).ToArray();
        Assert.Equal(expected.Length, parts.Length);
        for (var i = 0; i < expected.Length; i++)
            Assert.True(Math.Abs(expected[i] - parts[i]) <= tol,
                $"{label}[{i}]: atteso {expected[i]}, ottenuto {parts[i]}");
    }

    [Fact]
    public void SignalSample_Matches_SciPy()
    {
        var out_ = RunSample("SignalSciPyParity.bas");
        AssertArray(out_, "convolve", [0, 1, 2.5, 4, 1.5]);
        AssertArray(out_, "correlate", [0.5, 2, 3.5, 3, 0]);
        AssertArray(out_, "hann", [0, 0.75, 0.75, 0]);
        AssertArray(out_, "hamming", [0.08, 0.77, 0.77, 0.08]);
        AssertArray(out_, "blackman", [0, 0.63, 0.63, 0], 1e-14);
    }

    // Il percorso IL deve produrre gli stessi valori SciPy (SPECIAL/VECTOR/SIGNAL/MATRIX).
    [Fact]
    public void Samples_On_IlBackend_Match_SciPy()
    {
        var sp = RunSampleIl("SpecialSciPyParity.bas");
        AssertValue(sp, "gamma(5)", 24.0);
        AssertValue(sp, "erf(0.5)", 0.5204998778130465);
        AssertValue(sp, "betainc", 0.6875);
        AssertValue(sp, "gammaincinv", 2.994308347002123);
        AssertValue(sp, "betaincinv", 0.3857275681323895);
        AssertValue(sp, "kv(1,2)", 0.13986588181652246);

        var vd = RunSampleIl("VectorDistanceSciPyParity.bas");
        AssertValue(vd, "manhattan", 12.0);
        AssertValue(vd, "cosine", 0.007416666029069763);
        AssertValue(vd, "minkowski", 6.0);

        var sg = RunSampleIl("SignalSciPyParity.bas");
        AssertArray(sg, "convolve", [0, 1, 2.5, 4, 1.5]);
        AssertArray(sg, "correlate", [0.5, 2, 3.5, 3, 0]);

        var mf = RunSampleIl("MatrixFuncSciPyParity.bas");
        AssertValue(mf, "det(expm(I))", 7.38905609893065);
        AssertValue(mf, "det(kron(I,I))", 1.0);
    }

    // Nuclei scipy.integrate/interpolate/fft: sample su P-Code managed e IL, oracoli scipy 1.18.
    [Fact]
    public void IntegrateInterpFftSample_Matches_SciPy_On_PCode_And_Il()
    {
        var pc = RunSample("IntegrateInterpFftSciPyParity.bas");
        AssertValue(pc, "integrate", 1.0 / 3.0);
        AssertValue(pc, "simpson", 1.0 / 3.0);
        AssertValue(pc, "trapezoid", 9.5);
        AssertArray(pc, "cumtrap", [0.5, 3.0, 9.5]);
        AssertValue(pc, "ode y(1)", 0.367879774412498, 1e-6);
        AssertValue(pc, "cubicspline", 1.3125);
        AssertArray(pc, "fftfreq6", [0, 1.0 / 6, 1.0 / 3, -0.5, -1.0 / 3, -1.0 / 6]);
        AssertArray(pc, "fftshift", [3, 4, 5, 0, 1, 2]);
        AssertArray(pc, "ifftshift", [3, 4, 5, 0, 1, 2]);

        var il = RunSampleIl("IntegrateInterpFftSciPyParity.bas");
        AssertValue(il, "integrate", 1.0 / 3.0);
        AssertValue(il, "simpson", 1.0 / 3.0);
        AssertValue(il, "trapezoid", 9.5);
        AssertArray(il, "cumtrap", [0.5, 3.0, 9.5]);
        AssertValue(il, "ode y(1)", 0.367879774412498, 1e-6);
        AssertValue(il, "cubicspline", 1.3125);
        AssertArray(il, "fftfreq6", [0, 1.0 / 6, 1.0 / 3, -0.5, -1.0 / 3, -1.0 / 6]);
        AssertArray(il, "fftshift", [3, 4, 5, 0, 1, 2]);
        AssertArray(il, "ifftshift", [3, 4, 5, 0, 1, 2]);
    }

    // Filtri IIR scipy.signal: sample su P-Code e IL, oracoli scipy 1.18.
    [Fact]
    public void SignalFiltersSample_Matches_SciPy_On_PCode_And_Il()
    {
        var pc = RunSample("SignalFiltersSciPyParity.bas");
        AssertArray(pc, "lfilter", [0.131106439917, 0.360252856803, 0.364810496267, 0.174735132172, 0.031358174304, -0.024116209763, -0.026570004153, -0.013303969028, -0.002715812036, 0.001590684252], 1e-9);
        AssertArray(pc, "filtfilt", [0.99999793, 0.43878922, 0.07309755, -0.06477432, -0.06831519], 1e-7);
        AssertArray(pc, "freqz", [1.0, 0.988585217538, 0.834275081701, 0.502685360346, 0.251285858007, 0.115138087405, 0.044498972049, 0.010271465769], 1e-9);
        AssertArray(pc, "ellip lfilt", [0.132071151725, 0.254563216417, 0.271683803814, 0.270922419569], 1e-9);

        var il = RunSampleIl("SignalFiltersSciPyParity.bas");
        AssertArray(il, "lfilter", [0.131106439917, 0.360252856803, 0.364810496267, 0.174735132172, 0.031358174304, -0.024116209763, -0.026570004153, -0.013303969028, -0.002715812036, 0.001590684252], 1e-9);
        AssertArray(il, "filtfilt", [0.99999793, 0.43878922, 0.07309755, -0.06477432, -0.06831519], 1e-7);
        AssertArray(il, "freqz", [1.0, 0.988585217538, 0.834275081701, 0.502685360346, 0.251285858007, 0.115138087405, 0.044498972049, 0.010271465769], 1e-9);
        AssertArray(il, "ellip lfilt", [0.132071151725, 0.254563216417, 0.271683803814, 0.270922419569], 1e-9);
    }

    // Wrapper TIMESERIES sui filtri SIGNAL: TSLOWPASS/TSHIGHPASS/TSBANDPASS.
    [Fact]
    public void TimeSeriesFiltersSample_Matches_SciPy()
    {
        var pc = RunSample("TimeSeriesFiltersSciPyParity.bas");
        AssertArray(pc, "lowpass", [0.131106439917, 0.360252856803, 0.364810496267, 0.174735132172, 0.031358174304, -0.024116209763], 1e-9);
        AssertArray(pc, "highpass", [0.50500103, -0.63236775, -0.10534556, 0.09336368, 0.09849298, 0.048237], 1e-7);
        AssertArray(pc, "bandpass", [0.13110644, 0.183558, -0.17201468, -0.38803466, -0.10816017, 0.17900696], 1e-7);

        var il = RunSampleIl("TimeSeriesFiltersSciPyParity.bas");
        AssertArray(il, "lowpass", [0.131106439917, 0.360252856803, 0.364810496267, 0.174735132172, 0.031358174304, -0.024116209763], 1e-9);
        AssertArray(il, "highpass", [0.50500103, -0.63236775, -0.10534556, 0.09336368, 0.09849298, 0.048237], 1e-7);
        AssertArray(il, "bandpass", [0.13110644, 0.183558, -0.17201468, -0.38803466, -0.10816017, 0.17900696], 1e-7);
    }

    // Inverse CDF (ppf) e ISF delle distribuzioni — oracoli scipy.stats 1.18.
    [Fact]
    public void StatsInverseSample_Matches_SciPy()
    {
        var pc = RunSample("StatsInverseSciPyParity.bas");
        AssertValue(pc, "normppf", 4.919927969080108);
        AssertValue(pc, "tppf", 2.228138851986275);
        AssertValue(pc, "tisf", 2.228138851986275);
        AssertValue(pc, "chi2ppf", 11.070497693516351);
        AssertValue(pc, "chippf", 3.466559767807347);
        AssertValue(pc, "fppf", 4.0661805513511595);
        AssertValue(pc, "betappf", 0.3857275681323895);
        AssertValue(pc, "expppf", 4.605170185988092);
        AssertValue(pc, "weibppf", 2.746710033739222);
        AssertValue(pc, "logppf", 5.159170355622591);

        var il = RunSampleIl("StatsInverseSciPyParity.bas");
        AssertValue(il, "normppf", 4.919927969080108);
        AssertValue(il, "tppf", 2.228138851986275);
        AssertValue(il, "chi2ppf", 11.070497693516351);
        AssertValue(il, "fppf", 4.0661805513511595);
        AssertValue(il, "betappf", 0.3857275681323895);
        AssertValue(il, "weibppf", 2.746710033739222);
    }

    // CDF/PDF delle distribuzioni (C99: verifica ordine argomenti corretto).
    [Fact]
    public void StatsCdfPdfSample_Matches_SciPy()
    {
        var pc = RunSample("StatsCdfPdfSciPyParity.bas");
        AssertValue(pc, "ncdf", 0.9331927987311419);
        AssertValue(pc, "npdf", 0.3989422804014327);
        AssertValue(pc, "tcdf", 0.9750000000005817);
        AssertValue(pc, "bcdf", 0.6875);
        AssertValue(pc, "c2cdf", 0.95);
        AssertValue(pc, "fcdf", 0.9500000000013653);
        AssertValue(pc, "ecdf", 0.950212931632136);
        AssertValue(pc, "wcdf", 0.7527962752955224);
        AssertValue(pc, "lncdf", 0.5781741009350194);
    }

    // scipy.spatial.KDTree: query k-NN (indici/distanze) e query_ball_point.
    [Fact]
    public void KdTreeSample_Matches_SciPy()
    {
        var pc = RunSample("KdTreeSciPyParity.bas");
        AssertArray(pc, "query", [1, 7, 0]);
        AssertArray(pc, "qdist", [1.0, 1.4142135623730951, 2.23606797749979], 1e-9);
        AssertArray(pc, "ball", [1, 7]);
        var il = RunSampleIl("KdTreeSciPyParity.bas");
        AssertArray(il, "query", [1, 7, 0]);
        AssertArray(il, "qdist", [1.0, 1.4142135623730951, 2.23606797749979], 1e-9);
        AssertArray(il, "ball", [1, 7]);
    }




    // scipy P1: welch, savgol, resample, decimate, splrep/splev, pdist/cdist, rfft/irfft, hull.
    [Fact]
    public void SciPyP1Sample_Matches_SciPy()
    {
        var pc = RunSample("SciPyP1Parity.bas");
        AssertValue(pc, "welchrows", 33.0, 1e-9);
        AssertValue(pc, "savgol10", 100.0, 1e-6);
        AssertValue(pc, "savgol0", 0.0, 1e-6);
        AssertValue(pc, "resample64", -0.9510175931303089, 1e-6);
        AssertValue(pc, "splev", 12.25, 1e-6);
        AssertValue(pc, "pdist0", 5.0, 1e-9);
        AssertValue(pc, "cdist10", 5.0, 1e-9);
        AssertValue(pc, "distmat10", 5.0, 1e-9);
        AssertValue(pc, "rfft10", 4.0, 1e-9);
        AssertValue(pc, "irfft0", 1.0, 1e-6);
        AssertValue(pc, "hull", 0.0, 1e-9);

        var il = RunSampleIl("SciPyP1Parity.bas");
        AssertValue(il, "savgol10", 100.0, 1e-6);
        AssertValue(il, "splev", 12.25, 1e-6);
        AssertValue(il, "pdist0", 5.0, 1e-9);
        AssertValue(il, "irfft0", 1.0, 1e-6);
        AssertValue(il, "hull", 0.0, 1e-9);
    }

    // scipy.signal P0 (firwin/find_peaks) + scipy.integrate P0 (solve_ivp).
    [Fact]
    public void SignalIntegrateP0Sample_Matches_SciPy()
    {
        var pc = RunSample("SignalIntegrateP0Parity.bas");
        AssertValue(pc, "firwin5", 0.4, 1e-9);
        AssertValue(pc, "firwin4", 0.273822482781629, 1e-9);
        AssertValue(pc, "peaks", 1.0, 1e-9);
        AssertValue(pc, "ivprows", 5.0, 1e-9);
        AssertValue(pc, "ivpyend", 2.718281828459045, 1e-5);

        var il = RunSampleIl("SignalIntegrateP0Parity.bas");
        AssertValue(il, "firwin5", 0.4, 1e-9);
        AssertValue(il, "peaks", 1.0, 1e-9);
        AssertValue(il, "ivpyend", 2.718281828459045, 1e-5);
    }

    // scipy.stats P0: correlazione, linregress, test, Gamma, descrittive.
    [Fact]
    public void StatsP0Sample_Matches_SciPy()
    {
        // NOTA: il sample è stato riscritto (commit 809ad9213, altra sessione) per
        // consolidare i 20 test d'ipotesi STATS su x/y={2,4,5,7,9}/{1,3,6,8,10} (NON
        // più {1,2,3,4,5}/{2,4,6,8,10}, quindi pearsonr/linreg NON sono più 1.0/pendenza
        // esatta — x,y sono correlati ma non perfettamente linearmente) con
        // MATH.ROUND(...,6) per la stabilità cross-backend — da cui la tolleranza 1e-6
        // invece di 1e-9. Le vecchie etichette gammapdf/gammacdf/gammappf/mode/moment2/
        // iqr/descmean/pearsonn non esistono più nel sample (sostituite da ttest/
        // ttpaired/ttone/anova/chi2/fisher/ztest/ks/shapiro/normtest/kurttest/describe).
        // Valori attesi verificati indipendentemente a mano: pearsonr (dx·dy/√(Σdx²Σdy²)),
        // ttest (t di Student pooled-variance) — entrambi confermano l'implementazione
        // STATS corretta; normtest/skewtest/kurttest verificati contro una chiamata reale
        // scipy.stats.normaltest/skewtest/kurtosistest(np.arange(1,21)) (bug precedente:
        // skewtest usava un'approssimazione grezza non-scipy, normtest/kurttest
        // ereditavano lo scarto perché skewZ==0 invece di ≈1.0403 per un campione
        // simmetrico — vedi HypothesisTests.SkewTestZ); gli altri sono cross-verificati
        // per parità IL/PCode (identici).
        var pc = RunSample("StatsSciPyParityP0.bas");
        AssertValue(pc, "ttest", -0.098533, 1e-6);
        AssertValue(pc, "ttpaired", 1.5, 1e-6);
        AssertValue(pc, "ttone", 0.331042, 1e-6);
        AssertValue(pc, "anova", 4.8, 1e-6);
        AssertValue(pc, "chi2con", 0.793651, 1e-6);
        AssertValue(pc, "fisher", 20.0, 1e-6);
        AssertValue(pc, "ztest", 0.447214, 1e-6);
        AssertValue(pc, "ks", 0.2, 1e-6);
        AssertValue(pc, "shapiro", 0.986339, 1e-6);
        AssertValue(pc, "normtest", 3.992116, 1e-6);
        AssertValue(pc, "pearsonr", 0.984429, 1e-6);
        AssertValue(pc, "spearman", 1.0, 1e-6);
        AssertValue(pc, "linreg", 1.328767, 1e-6);   // slope
        AssertValue(pc, "mannwhit", 12.0, 1e-6);     // U
        AssertValue(pc, "wilcoxon", 6.0, 1e-6);      // W
        AssertValue(pc, "skewtest", 1.040349, 1e-6);
        AssertValue(pc, "kurttest", -1.70581, 1e-6);
        AssertValue(pc, "jarque", 4.848241, 1e-6);
        AssertValue(pc, "describe", 5.0, 1e-6);      // Nobs

        var il = RunSampleIl("StatsSciPyParityP0.bas");
        AssertValue(il, "pearsonr", 0.984429, 1e-6);
        AssertValue(il, "linreg", 1.328767, 1e-6);
        AssertValue(il, "chi2con", 0.793651, 1e-6);
        AssertValue(il, "ttest", -0.098533, 1e-6);
        AssertValue(il, "ks", 0.2, 1e-6);
    }

    // scipy.linalg completato: sqrtm/logm/hessenberg/block_diag.
    [Fact]
    public void MatrixFunctionsSample_Matches_SciPy()
    {
        var pc = RunSample("MatrixFunctionsSciPyParity.bas");
        AssertValue(pc, "sqrtm00", 1.36602540378444, 1e-9);
        AssertValue(pc, "sqrtm01", 0.366025403784439, 1e-9);
        AssertValue(pc, "logm00", 0.549306144334055, 1e-9);
        AssertValue(pc, "logm11", 0.549306144334055, 1e-9);
        AssertValue(pc, "hess20", 0.0, 1e-6);
        AssertValue(pc, "bd00", 2.0, 1e-9);
        AssertValue(pc, "bd22", 2.0, 1e-9);
        var il = RunSampleIl("MatrixFunctionsSciPyParity.bas");
        AssertValue(il, "sqrtm00", 1.36602540378444, 1e-9);
        AssertValue(il, "sqrtm01", 0.366025403784439, 1e-9);
        AssertValue(il, "logm00", 0.549306144334055, 1e-9);
        AssertValue(il, "hess20", 0.0, 1e-6);
        AssertValue(il, "bd00", 2.0, 1e-9);
    }

    // scipy.linalg completato (2° lotto): cosm/sinm/tanm/signm/fractional_power,
    // orth/subspace_angles/polar/cond/nuclear, toeplitz/hankel/circulant,
    // solve_triangular, eig generale e schur.
    [Fact]
    public void MatrixFunctions2Sample_Matches_SciPy()
    {
        var pc = RunSample("MatrixFunctionsSciPyParity2.bas");
        AssertValue(pc, "cosm00", -0.2248450953661529, 1e-9);
        AssertValue(pc, "sinm01", -0.35017548837401463, 1e-9);
        AssertValue(pc, "tanm00", 0.707430590790312, 1e-6);
        AssertValue(pc, "signm01", 0.0, 1e-9);
        AssertValue(pc, "fp00", 1.36602540378444, 1e-9);
        // ORTH: il segno delle colonne è canonico POSITIVO (primo elemento di modulo massimo),
        // convenzione condivisa col runtime C (`gmb_matrix_orth`/`CanonicalizeColumnSigns`).
        // SciPy lascia il segno all'implementazione SVD: qui il valore atteso è il modulo.
        AssertValue(pc, "orth00", 0.5257311121191336, 1e-9);
        AssertValue(pc, "anglen", 2.0, 1e-9);
        AssertValue(pc, "angle0", 0.0, 1e-9);
        AssertValue(pc, "polar00", -0.4472135954999579, 1e-9);
        AssertValue(pc, "polarP00", 1.3416407864998738, 1e-9);
        AssertValue(pc, "cond", 17.94427190999916, 1e-9);
        AssertValue(pc, "nuc", 4.47213595499958, 1e-9);
        AssertValue(pc, "toep01", 4.0, 1e-9);
        AssertValue(pc, "toep10", 2.0, 1e-9);
        AssertValue(pc, "hankel21", 4.0, 1e-9);
        AssertValue(pc, "circ01", 3.0, 1e-9);
        AssertValue(pc, "st00", 2.0, 1e-9);
        AssertValue(pc, "eigreal0", 3.0, 1e-9);
        AssertValue(pc, "eigreal1", 1.0, 1e-9);
        AssertValue(pc, "eigvr00", 0.7071067811865476, 1e-9);
        AssertValue(pc, "schur00", 3.0, 1e-9);

        var il = RunSampleIl("MatrixFunctionsSciPyParity2.bas");
        AssertValue(il, "cosm00", -0.2248450953661529, 1e-9);
        AssertValue(il, "sinm01", -0.35017548837401463, 1e-9);
        AssertValue(il, "tanm00", 0.707430590790312, 1e-6);
        AssertValue(il, "fp00", 1.36602540378444, 1e-9);
        // Segno canonico positivo condiviso (vedi nota sul leg PCode).
        AssertValue(il, "orth00", 0.5257311121191336, 1e-9);
        AssertValue(il, "polar00", -0.4472135954999579, 1e-9);
        AssertValue(il, "cond", 17.94427190999916, 1e-9);
        AssertValue(il, "toep01", 4.0, 1e-9);
        AssertValue(il, "hankel21", 4.0, 1e-9);
        AssertValue(il, "circ01", 3.0, 1e-9);
        AssertValue(il, "st00", 2.0, 1e-9);
        AssertValue(il, "eigreal0", 3.0, 1e-9);
        AssertValue(il, "eigvr00", 0.7071067811865476, 1e-9);
        AssertValue(il, "schur00", 3.0, 1e-9);
    }
}
