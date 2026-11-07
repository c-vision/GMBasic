// FILE: CommonLib/Trading/Options.cs
// FACADE P/Invoke verso libgmbquant (gmbsuite/gmbquant + shared/libs/quant).
//
// La versione C# delle formule (Options.cs originale, 434 righe) e' archiviata in
// gmbasicmonorepo-deprecated/... . Stessa API pubblica; i bool diventano 0/1.
namespace CommonLib.Trading;

public static class Options
{
    public static double BsCall(double s, double k, double t, double r, double v)
        => QuantNative.BsCall(s, k, t, r, v);

    public static double BsPut(double s, double k, double t, double r, double v)
        => QuantNative.BsPut(s, k, t, r, v);

    public static double MertonCall(double s, double k, double t, double r, double q, double v)
        => QuantNative.MertonCall(s, k, t, r, q, v);

    public static double MertonPut(double s, double k, double t, double r, double q, double v)
        => QuantNative.MertonPut(s, k, t, r, q, v);

    public static double BinomialOption(double s, double k, double t, double r, double v, int steps,
        bool isPut, bool american = false)
        => QuantNative.BinomialEx(s, k, t, r, v, steps, isPut ? 1 : 0, american ? 1 : 0);

    public static double Delta(double s, double k, double t, double r, double v, bool isPut)
        => QuantNative.Delta(s, k, t, r, v, isPut ? 1 : 0);

    public static double Gamma(double s, double k, double t, double r, double v)
        => QuantNative.Gamma(s, k, t, r, v);

    public static double Vega(double s, double k, double t, double r, double v)
        => QuantNative.Vega(s, k, t, r, v);

    public static double Theta(double s, double k, double t, double r, double v, bool isPut)
        => QuantNative.Theta(s, k, t, r, v, isPut ? 1 : 0);

    public static double Rho(double s, double k, double t, double r, double v, bool isPut)
        => QuantNative.Rho(s, k, t, r, v, isPut ? 1 : 0);

    public static double Vanna(double s, double k, double t, double r, double v)
        => QuantNative.Vanna(s, k, t, r, v);

    public static double Volga(double s, double k, double t, double r, double v)
        => QuantNative.Volga(s, k, t, r, v);

    public static double Charm(double s, double k, double t, double r, double v, bool isPut)
        => QuantNative.Charm(s, k, t, r, v, isPut ? 1 : 0);

    public static double Speed(double s, double k, double t, double r, double v)
        => QuantNative.Speed(s, k, t, r, v);

    public static double Zomma(double s, double k, double t, double r, double v)
        => QuantNative.Zomma(s, k, t, r, v);

    public static double Color(double s, double k, double t, double r, double v)
        => QuantNative.Color(s, k, t, r, v);
}
