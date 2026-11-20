// FILE: CommonLib.Numerics.StatsDistNative.cs
// DISTRIBUZIONI: le gambe MANAGED (IL/PCodeVM) chiamano il runtime C via P/Invoke.
//
// PERCHÉ. `Stats.cs` calcolava pdf/cdf/pmf in C# (MathNet) mentre VM/C e C99Native usano
// `gmb_dist_*` di `gmb_math.c`. Qui il managed chiama la lib `gmb_dist_flat` (inoltro puro):
// una sola fonte numerica.
//
// ATTENZIONE ALL'ORDINE: i metodi qui seguono l'ordine dei KERNEL. Due famiglie differiscono dal
// C# pubblico e il riordino è nel chiamante (`Stats.cs`):
//   - triangolare: kernel (min, max, mode, x) / C# (min, mode, max, x);
//   - skew-normal: kernel (x, location, scale, shape) / C# (mean, sd, skew, x).
//
// ERRORI. Le guardie di dominio restano nel C#; lib assente → `DllNotFoundException`.

using System.Runtime.InteropServices;

namespace CommonLib;

internal static class StatsDistNative
{
    private const string Lib = "gmb_dist_flat";

    [DllImport(Lib, EntryPoint = "gmb_dstf_normal_pdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NormalPdf(double mean, double sd, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_normal_cdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NormalCdf(double mean, double sd, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_lognormal_pdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double LognormalPdf(double mu, double sigma, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_lognormal_cdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double LognormalCdf(double mu, double sigma, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_chi_pdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double ChiPdf(double df, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_chi_cdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double ChiCdf(double df, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_chi2_pdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Chi2Pdf(double df, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_chi2_cdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Chi2Cdf(double df, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_beta_pdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double BetaPdf(double a, double b, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_beta_cdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double BetaCdf(double a, double b, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_student_t_pdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double StudentTPdf(double df, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_student_t_cdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double StudentTCdf(double df, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_weibull_pdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double WeibullPdf(double shape, double scale, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_weibull_cdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double WeibullCdf(double shape, double scale, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_f_pdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double FPdf(double d1, double d2, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_f_cdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double FCdf(double d1, double d2, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_exp_pdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double ExpPdf(double rate, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_exp_cdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double ExpCdf(double rate, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_pareto_pdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double ParetoPdf(double xm, double alpha, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_pareto_cdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double ParetoCdf(double xm, double alpha, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_logistic_pdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double LogisticPdf(double mu, double s, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_logistic_cdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double LogisticCdf(double mu, double s, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_triangular_pdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double TriangularPdf(double min, double max, double mode, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_triangular_cdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double TriangularCdf(double min, double max, double mode, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_uniform_pdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double UniformPdf(double a, double b, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_skewnormal_pdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double SkewNormalPdf(double x, double location, double scale, double shape);
    [DllImport(Lib, EntryPoint = "gmb_dstf_bernoulli_pmf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double BernoulliPmf(double p, int k);
    [DllImport(Lib, EntryPoint = "gmb_dstf_binomial_pmf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double BinomialPmf(double p, int n, int k);
    [DllImport(Lib, EntryPoint = "gmb_dstf_poisson_pmf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double PoissonPmf(double lambda, int k);
    [DllImport(Lib, EntryPoint = "gmb_dstf_geometric_pmf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double GeometricPmf(double p, int k);
    [DllImport(Lib, EntryPoint = "gmb_dstf_negbinom_pmf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NegBinomPmf(double r, double p, int k);
    [DllImport(Lib, EntryPoint = "gmb_dstf_hypergeom_pmf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double HypergeomPmf(int n, int k, int draws, int hits);
}
