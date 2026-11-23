// FILE: CommonLib.Numerics.Fitting.cs
// Curve fitting: le formule (Chebyshev, Padé, polinomiale, least squares, R²) vivono nel runtime
// C (`gmb_fit_*`) e qui si chiamano via P/Invoke (`gmb_analysis_flat`), come le gambe C.
namespace CommonLib;

public static partial class Numerics
{
    public static double ChebEval(double x, double[] coef)
    {
        if (coef.Length < 1) return double.NaN;
        return AnalysisNative.ChebEval(x, coef);
    }

    public static double PadeEval(double x, double[] a, double[] b)
    {
        return AnalysisNative.PadeEval(x, a, b);
    }

    public static double PolyEval(double x, double[] coef)
    {
        return AnalysisNative.PolyEval(x, coef);
    }

    public static double PolyLs(double[] x, double[] y, int m, double[] coef)
    {
        var n = Math.Min(x.Length, y.Length);
        var p = m + 1;
        if (n < 1 || m < 0 || p > 32) return 0.0;
        return AnalysisNative.PolyLs(x, y, m, coef);
    }

    public static (double[] Coef, double Ssq) PolyFit(double[] x, double[] y, int m)
    {
        var coef = new double[m + 1];
        var ssq = PolyLs(x, y, m, coef);
        return (coef, ssq);
    }

    public static (double R2, double Angle) RSquared(double[] data)
    {
        if (data.Length <= 2) return (0.0, 0.0);
        return AnalysisNative.RSquared(data);
    }

    public static double RSquaredTest(double[] data)
    {
        return AnalysisNative.RSquaredTest(data);
    }
}
