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

    // ── Inverse (ppf/isf) e CDF uniforme — ordine dei kernel ──────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_dstf_normal_ppf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NormalPpf(double mean, double sd, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_normal_isf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NormalIsf(double mean, double sd, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_lognormal_ppf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double LognormalPpf(double mu, double sigma, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_lognormal_isf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double LognormalIsf(double mu, double sigma, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_chi_ppf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double ChiPpf(double df, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_chi_isf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double ChiIsf(double df, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_chi2_ppf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Chi2Ppf(double df, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_chi2_isf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Chi2Isf(double df, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_beta_ppf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double BetaPpf(double a, double b, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_beta_isf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double BetaIsf(double a, double b, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_student_t_ppf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double StudentTPpf(double df, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_student_t_isf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double StudentTIsf(double df, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_f_ppf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double FPpf(double d1, double d2, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_f_isf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double FIsf(double d1, double d2, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_exp_ppf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double ExpPpf(double rate, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_exp_isf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double ExpIsf(double rate, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_weibull_ppf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double WeibullPpf(double shape, double scale, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_weibull_isf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double WeibullIsf(double shape, double scale, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_logistic_ppf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double LogisticPpf(double loc, double scale, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_logistic_isf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double LogisticIsf(double loc, double scale, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_uniform_ppf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double UniformPpf(double a, double b, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_uniform_isf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double UniformIsf(double a, double b, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_uniform_cdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double UniformCdf(double a, double b, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_pareto_ppf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double ParetoPpf(double scale, double shape, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_pareto_isf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double ParetoIsf(double scale, double shape, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_triangular_ppf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double TriangularPpf(double min, double max, double mode, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_triangular_isf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double TriangularIsf(double min, double max, double mode, double p);

    // ── Gamma, ZTPoisson e descrittive residue ─────────────────────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_dstf_gamma_pdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double GammaPdf(double shape, double scale, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_gamma_cdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double GammaCdf(double shape, double scale, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_gamma_sf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double GammaSf(double shape, double scale, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_gamma_ppf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double GammaPpf(double shape, double scale, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_gamma_isf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double GammaIsf(double shape, double scale, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_ztpoisson_pmf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double ZTPoissonPmf(double lambda, int k);
    [DllImport(Lib, EntryPoint = "gmb_dstf_ztpoisson_cdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double ZTPoissonCdf(double lambda, int k);
    [DllImport(Lib, EntryPoint = "gmb_dstf_mode", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Mode(double[] a, int n);
    [DllImport(Lib, EntryPoint = "gmb_dstf_moment", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Moment(double[] a, int n, int order);
    // ── Non-centrali ───────────────────────────────────────────────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_chi2_pdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcChi2Pdf(double df, double nc, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_chi2_cdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcChi2Cdf(double df, double nc, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_chi2_sf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcChi2Sf(double df, double nc, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_chi2_ppf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcChi2Ppf(double df, double nc, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_chi2_isf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcChi2Isf(double df, double nc, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_beta_pdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcBetaPdf(double a, double b, double nc, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_beta_cdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcBetaCdf(double a, double b, double nc, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_beta_sf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcBetaSf(double a, double b, double nc, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_beta_ppf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcBetaPpf(double a, double b, double nc, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_beta_isf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcBetaIsf(double a, double b, double nc, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_gamma_pdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcGammaPdf(double shape, double nc, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_gamma_cdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcGammaCdf(double shape, double nc, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_gamma_sf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcGammaSf(double shape, double nc, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_gamma_ppf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcGammaPpf(double shape, double nc, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_gamma_isf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcGammaIsf(double shape, double nc, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_f_pdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcFPdf(double d1, double d2, double nc, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_f_cdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcFCdf(double d1, double d2, double nc, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_f_sf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcFSf(double d1, double d2, double nc, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_f_ppf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcFPpf(double d1, double d2, double nc, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_f_isf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcFIsf(double d1, double d2, double nc, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_t_pdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcTPdf(double df, double nc, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_t_cdf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcTCdf(double df, double nc, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_t_sf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcTSf(double df, double nc, double x);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_t_ppf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcTPpf(double df, double nc, double p);
    [DllImport(Lib, EntryPoint = "gmb_dstf_nc_t_isf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double NcTIsf(double df, double nc, double p);

    [DllImport(Lib, EntryPoint = "gmb_dstf_iqr", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Iqr(double[] a, int n);
}
