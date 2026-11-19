// FILE: Basic.IlBackend.Tests.SciPyAnovaParityTests.cs

using System.Globalization;
using System.Reflection;
using Basic.IlBackend;
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public sealed class SciPyAnovaParityTests
{
    private const double SciPyFStatistic = 4.8d;
    private const double SciPyPValue = 0.07098765432098765d;
    private static readonly string Root = FindRepoRoot();

    [Fact]
    public void CommonLibAnovaMatchesSciPyFOneWay()
    {
        double[][] groups = [[2d, 3d, 4d, 5d], [4d, 5d, 6d, 7d]];

        var (fStatistic, pValue, ssBetween, ssWithin) = CommonLib.Numerics.Anova(groups);

        Assert.Equal(SciPyFStatistic, fStatistic, 13);
        Assert.Equal(SciPyPValue, pValue, 13);
        Assert.Equal(8d, ssBetween, 13);
        Assert.Equal(10d, ssWithin, 13);
    }

    [Fact]
    public void StatsHypothesisTestsBasicProgramMatchesSciPy()
    {
        var sample = Path.Combine(
            Root,
            "shared",
            "samples",
            "Analysis",
            "StatsSciPyParity.bas");

        var output = RunSample(sample);

        AssertPair(output, "TTEST=", -0.098533d, 0.923933d);
        AssertPair(output, "PAIRED=", 1.5d, 0.208d);
        AssertPair(output, "ONESAMPLE=", 0.331042d, 0.757228d);
        AssertPair(output, "ANOVA=", 4.8d, 0.070988d);
        AssertPair(output, "CHI2=", 0.5d, 0.992123d);
        AssertPair(output, "FISHER=", 20d, 0.034965d);
        AssertPair(output, "SHAPIRO=", 0.985318d, 0.942475d);
        AssertPair(output, "NORMALTEST=", 0.786766d, 0.67477d);
        AssertPair(output, "ZTEST=", 0.447214d, 0.654721d);
        AssertPair(output, "KS=", 0.2d, 1d);
    }

    private static string FindRepoRoot()
    {
        var directory = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (directory is not null)
        {
            if (File.Exists(Path.Combine(directory.FullName, "GMBasicSharp", "GMBasicSharp.slnx")))
                return directory.FullName;
            directory = directory.Parent;
        }

        throw new DirectoryNotFoundException("Root GMBasic non trovata.");
    }

    private static double ReadValue(string output, string marker)
    {
        var markerIndex = output.IndexOf(marker, StringComparison.Ordinal);
        Assert.True(markerIndex >= 0, $"Marker '{marker}' non trovato nell'output: {output}");
        var valueStart = markerIndex + marker.Length;
        var valueEnd = output.IndexOfAny(['\r', '\n'], valueStart);
        var value = valueEnd < 0 ? output[valueStart..] : output[valueStart..valueEnd];
        return double.Parse(value.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture);
    }

    private static void AssertPair(string output, string marker, double statistic, double pValue)
    {
        var markerIndex = output.IndexOf(marker, StringComparison.Ordinal);
        Assert.True(markerIndex >= 0, $"Marker '{marker}' non trovato nell'output: {output}");
        var valueStart = markerIndex + marker.Length;
        var valueEnd = output.IndexOfAny(['\r', '\n'], valueStart);
        var line = valueEnd < 0 ? output[valueStart..] : output[valueStart..valueEnd];
        var values = line.Split(',', StringSplitOptions.TrimEntries);

        Assert.Equal(2, values.Length);
        Assert.Equal(statistic, double.Parse(values[0], NumberStyles.Float, CultureInfo.InvariantCulture), 6);
        Assert.Equal(pValue, double.Parse(values[1], NumberStyles.Float, CultureInfo.InvariantCulture), 6);
    }

    private static string RunSample(string sample)
    {
        C99Emission.Require(sample);
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var assemblyBuilder = IlProgramCompiler.CompileToAssembly(
            program,
            $"SciPyAnova_{Guid.NewGuid():N}",
            out _);
        using var stream = new MemoryStream();
        assemblyBuilder.Save(stream);
        var assembly = Assembly.Load(stream.ToArray());
        var main = assembly.GetType("GeneratedProgram")?.GetMethod(
            "Main",
            BindingFlags.Public | BindingFlags.Static)
            ?? throw new InvalidOperationException("Main non trovato.");
        var originalOutput = Console.Out;
        var writer = new StringWriter(CultureInfo.InvariantCulture);

        try
        {
            Console.SetOut(writer);
            IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
            var task = Task.Run(() => main.Invoke(null, null));
            if (!task.Wait(TimeSpan.FromSeconds(20)))
                throw new TimeoutException($"STATS.Anova non ha completato. Output: {writer}");
            task.GetAwaiter().GetResult();
            return writer.ToString();
        }
        finally
        {
            Console.SetOut(originalOutput);
        }
    }
}
