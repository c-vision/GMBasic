// FILE: CommonLib.Numerics.Simulation.cs
// SIMULATION: RNG deterministico (SplitMix64) — parità bit-exact con il runtime C.
namespace CommonLib;

public static partial class Numerics
{
    private static ulong _simState = 0x9E3779B97F4A7C15UL;

    public static double SimSeed(double seed)
    {
        var s = (ulong)seed;
        _simState = s != 0 ? s : 0x9E3779B97F4A7C15UL;
        return 0.0;
    }

    private static ulong SimNextU64()
    {
        unchecked
        {
            _simState += 0x9E3779B97F4A7C15UL;
            var z = _simState;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9UL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBUL;
            return z ^ (z >> 31);
        }
    }

    private static double SimNextDouble()
        => (double)(SimNextU64() >> 11) * (1.0 / 9007199254740992.0);

    public static double SimUniform() => SimNextDouble();

    public static double SimNormal()
    {
        double u, v, s;
        do
        {
            u = 2.0 * SimNextDouble() - 1.0;
            v = 2.0 * SimNextDouble() - 1.0;
            s = u * u + v * v;
        } while (s >= 1.0 || s == 0.0);
        return u * Math.Sqrt(-2.0 * Math.Log(s) / s);
    }

    public static double SimExponential(double rate)
        => rate <= 0.0 ? double.NaN : -Math.Log(1.0 - SimNextDouble()) / rate;

    public static double SimBernoulli(double p) => SimNextDouble() < p ? 1.0 : 0.0;

    public static double SimBinomial(double nIn, double p)
    {
        var n = (int)nIn;
        if (n < 0) return double.NaN;
        double c = 0.0;
        for (var i = 0; i < n; i++) if (SimNextDouble() < p) c += 1.0;
        return c;
    }
}
