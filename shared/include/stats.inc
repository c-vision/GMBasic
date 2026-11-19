' stats.inc — namespace STATS via FFI (lib `gmbstats`).
'
' Migrazione a FFI (2026-11-18). La fonte della matematica è la libreria: le funzioni elencate
' qui NON hanno più il builtin (un `.inc` non può convivere con un builtin della stessa arità).
'
' Firma BASIC storica: `x` (o `p`) PRIMO per le distribuzioni. I wrapper `gs_basic_*` di
' `shared/libs/stats` girano l'ordine verso `gs_dist_*` (ordine C, `x` ultimo).
'
' Le descrittive (ex namespace parallelo, soppresso) sono wrapper BASIC a 1 argomento che
' calcolano `n = UBOUND(a)+1` e chiamano `gs_stats_*`: la libreria è l'unica fonte.
'
' Restano nel builtin (non ancora migrate): le famiglie con parametri OPZIONALI
' (Normal/Lognormal/Logistic/Triangular/Uniform/SkewedNormal), `ZTPoisson`, e le funzioni DICT.

NAMESPACE STATS

PUBLIC DECLARE FUNCTION BetaCdf LIB "gmbstats" ALIAS "gs_basic_beta_cdf" (x AS DOUBLE, alpha AS DOUBLE, beta AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION BetaPdf LIB "gmbstats" ALIAS "gs_basic_beta_pdf" (x AS DOUBLE, alpha AS DOUBLE, beta AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION BetaPpf LIB "gmbstats" ALIAS "gs_basic_beta_ppf" (p AS DOUBLE, alpha AS DOUBLE, beta AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION BetaSf LIB "gmbstats" ALIAS "gs_basic_beta_sf" (x AS DOUBLE, alpha AS DOUBLE, beta AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ChiCdf LIB "gmbstats" ALIAS "gs_basic_chi_cdf" (x AS DOUBLE, k AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ChiPdf LIB "gmbstats" ALIAS "gs_basic_chi_pdf" (x AS DOUBLE, k AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ChiPpf LIB "gmbstats" ALIAS "gs_basic_chi_ppf" (p AS DOUBLE, k AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ChiSf LIB "gmbstats" ALIAS "gs_basic_chi_sf" (x AS DOUBLE, k AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ChiSquaredCdf LIB "gmbstats" ALIAS "gs_basic_chi2_cdf" (x AS DOUBLE, df AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ChiSquaredIsf LIB "gmbstats" ALIAS "gs_basic_chi2_isf" (p AS DOUBLE, df AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ChiSquaredPdf LIB "gmbstats" ALIAS "gs_basic_chi2_pdf" (x AS DOUBLE, df AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ChiSquaredPpf LIB "gmbstats" ALIAS "gs_basic_chi2_ppf" (p AS DOUBLE, df AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ChiSquaredSf LIB "gmbstats" ALIAS "gs_basic_chi2_sf" (x AS DOUBLE, df AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ExponentialCdf LIB "gmbstats" ALIAS "gs_basic_exp_cdf" (x AS DOUBLE, rate AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ExponentialIsf LIB "gmbstats" ALIAS "gs_basic_exp_isf" (p AS DOUBLE, rate AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ExponentialPdf LIB "gmbstats" ALIAS "gs_basic_exp_pdf" (x AS DOUBLE, rate AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ExponentialPpf LIB "gmbstats" ALIAS "gs_basic_exp_ppf" (p AS DOUBLE, rate AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ExponentialSf LIB "gmbstats" ALIAS "gs_basic_exp_sf" (x AS DOUBLE, rate AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION FCdf LIB "gmbstats" ALIAS "gs_basic_f_cdf" (x AS DOUBLE, d1 AS DOUBLE, d2 AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION FIsf LIB "gmbstats" ALIAS "gs_basic_f_isf" (p AS DOUBLE, d1 AS DOUBLE, d2 AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION FPdf LIB "gmbstats" ALIAS "gs_basic_f_pdf" (x AS DOUBLE, d1 AS DOUBLE, d2 AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION FPpf LIB "gmbstats" ALIAS "gs_basic_f_ppf" (p AS DOUBLE, d1 AS DOUBLE, d2 AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION FSf LIB "gmbstats" ALIAS "gs_basic_f_sf" (x AS DOUBLE, d1 AS DOUBLE, d2 AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION GammaCdf LIB "gmbstats" ALIAS "gs_dist_gamma_cdf" (shape AS DOUBLE, scale AS DOUBLE, x AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION GammaIsf LIB "gmbstats" ALIAS "gs_dist_gamma_isf" (shape AS DOUBLE, scale AS DOUBLE, p AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION GammaPdf LIB "gmbstats" ALIAS "gs_dist_gamma_pdf" (shape AS DOUBLE, scale AS DOUBLE, x AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION GammaPpf LIB "gmbstats" ALIAS "gs_dist_gamma_ppf" (shape AS DOUBLE, scale AS DOUBLE, p AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION GammaSf LIB "gmbstats" ALIAS "gs_dist_gamma_sf" (shape AS DOUBLE, scale AS DOUBLE, x AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION LogisticPdf LIB "gmbstats" ALIAS "gs_basic_logistic_pdf" (x AS DOUBLE, location AS DOUBLE, scale AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION LogisticPpf LIB "gmbstats" ALIAS "gs_basic_logistic_ppf" (p AS DOUBLE, location AS DOUBLE, scale AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NcBetaCdf LIB "gmbstats" ALIAS "gs_basic_nc_beta_cdf" (x AS DOUBLE, a AS DOUBLE, b AS DOUBLE, nc AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NcBetaIsf LIB "gmbstats" ALIAS "gs_basic_nc_beta_isf" (p AS DOUBLE, a AS DOUBLE, b AS DOUBLE, nc AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NcBetaPdf LIB "gmbstats" ALIAS "gs_basic_nc_beta_pdf" (x AS DOUBLE, a AS DOUBLE, b AS DOUBLE, nc AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NcBetaPpf LIB "gmbstats" ALIAS "gs_basic_nc_beta_ppf" (p AS DOUBLE, a AS DOUBLE, b AS DOUBLE, nc AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NcChi2Cdf LIB "gmbstats" ALIAS "gs_basic_nc_chi2_cdf" (x AS DOUBLE, df AS DOUBLE, nc AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NcChi2Isf LIB "gmbstats" ALIAS "gs_basic_nc_chi2_isf" (p AS DOUBLE, df AS DOUBLE, nc AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NcChi2Pdf LIB "gmbstats" ALIAS "gs_basic_nc_chi2_pdf" (x AS DOUBLE, df AS DOUBLE, nc AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NcChi2Ppf LIB "gmbstats" ALIAS "gs_basic_nc_chi2_ppf" (p AS DOUBLE, df AS DOUBLE, nc AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NcGammaCdf LIB "gmbstats" ALIAS "gs_basic_nc_gamma_cdf" (x AS DOUBLE, shape AS DOUBLE, nc AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NcGammaIsf LIB "gmbstats" ALIAS "gs_basic_nc_gamma_isf" (p AS DOUBLE, shape AS DOUBLE, nc AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NcGammaPdf LIB "gmbstats" ALIAS "gs_basic_nc_gamma_pdf" (x AS DOUBLE, shape AS DOUBLE, nc AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NcGammaPpf LIB "gmbstats" ALIAS "gs_basic_nc_gamma_ppf" (p AS DOUBLE, shape AS DOUBLE, nc AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ParetoCdf LIB "gmbstats" ALIAS "gs_basic_pareto_cdf" (x AS DOUBLE, scale AS DOUBLE, shape AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ParetoPdf LIB "gmbstats" ALIAS "gs_basic_pareto_pdf" (x AS DOUBLE, scale AS DOUBLE, shape AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ParetoPpf LIB "gmbstats" ALIAS "gs_basic_pareto_ppf" (p AS DOUBLE, scale AS DOUBLE, shape AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ParetoSf LIB "gmbstats" ALIAS "gs_basic_pareto_sf" (x AS DOUBLE, scale AS DOUBLE, shape AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION StudentTCdf LIB "gmbstats" ALIAS "gs_basic_student_t_cdf" (x AS DOUBLE, nu AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION StudentTIsf LIB "gmbstats" ALIAS "gs_basic_student_t_isf" (p AS DOUBLE, nu AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION StudentTPdf LIB "gmbstats" ALIAS "gs_basic_student_t_pdf" (x AS DOUBLE, nu AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION StudentTPpf LIB "gmbstats" ALIAS "gs_basic_student_t_ppf" (p AS DOUBLE, nu AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION StudentTSf LIB "gmbstats" ALIAS "gs_basic_student_t_sf" (x AS DOUBLE, nu AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION UniformCdf LIB "gmbstats" ALIAS "gs_basic_uniform_cdf" (x AS DOUBLE, a AS DOUBLE, b AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION UniformSf LIB "gmbstats" ALIAS "gs_basic_uniform_sf" (x AS DOUBLE, a AS DOUBLE, b AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION WeibullCdf LIB "gmbstats" ALIAS "gs_basic_weibull_cdf" (x AS DOUBLE, shape AS DOUBLE, scale AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION WeibullPdf LIB "gmbstats" ALIAS "gs_basic_weibull_pdf" (x AS DOUBLE, shape AS DOUBLE, scale AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION WeibullPpf LIB "gmbstats" ALIAS "gs_basic_weibull_ppf" (p AS DOUBLE, shape AS DOUBLE, scale AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION WeibullSf LIB "gmbstats" ALIAS "gs_basic_weibull_sf" (x AS DOUBLE, shape AS DOUBLE, scale AS DOUBLE) AS DOUBLE

' ── descrittive: wrapper BASIC sull'FFI gs_stats_* (una sola fonte: la lib) ──
PUBLIC DECLARE NATIVE FUNCTION SXN_MEAN LIB "gmbstats" ALIAS "gs_stats_mean" (a() AS DOUBLE, n AS LONG) AS DOUBLE
PUBLIC DECLARE NATIVE FUNCTION SXN_VARIANCE LIB "gmbstats" ALIAS "gs_stats_variance" (a() AS DOUBLE, n AS LONG, sample AS LONG) AS DOUBLE
PUBLIC DECLARE NATIVE FUNCTION SXN_STDDEV LIB "gmbstats" ALIAS "gs_stats_stddev" (a() AS DOUBLE, n AS LONG, sample AS LONG) AS DOUBLE
PUBLIC DECLARE NATIVE FUNCTION SXN_SKEWNESS LIB "gmbstats" ALIAS "gs_stats_skewness" (a() AS DOUBLE, n AS LONG) AS DOUBLE
PUBLIC DECLARE NATIVE FUNCTION SXN_KURTOSIS LIB "gmbstats" ALIAS "gs_stats_kurtosis" (a() AS DOUBLE, n AS LONG) AS DOUBLE
PUBLIC DECLARE NATIVE FUNCTION SXN_MIN LIB "gmbstats" ALIAS "gs_stats_min" (a() AS DOUBLE, n AS LONG) AS DOUBLE
PUBLIC DECLARE NATIVE FUNCTION SXN_MAX LIB "gmbstats" ALIAS "gs_stats_max" (a() AS DOUBLE, n AS LONG) AS DOUBLE
PUBLIC DECLARE NATIVE FUNCTION SXN_RANGE LIB "gmbstats" ALIAS "gs_stats_range" (a() AS DOUBLE, n AS LONG) AS DOUBLE
PUBLIC DECLARE NATIVE FUNCTION SXN_MEDIAN LIB "gmbstats" ALIAS "gs_stats_median" (a() AS DOUBLE, n AS LONG) AS DOUBLE
PUBLIC DECLARE NATIVE FUNCTION SXN_PERCENTILE LIB "gmbstats" ALIAS "gs_stats_percentile" (a() AS DOUBLE, n AS LONG, p AS DOUBLE) AS DOUBLE
PUBLIC DECLARE NATIVE FUNCTION SXN_MAD LIB "gmbstats" ALIAS "gs_stats_mad" (a() AS DOUBLE, n AS LONG) AS DOUBLE
PUBLIC DECLARE NATIVE FUNCTION SXN_COVARIANCE LIB "gmbstats" ALIAS "gs_stats_covariance" (a() AS DOUBLE, b() AS DOUBLE, n AS LONG, sample AS LONG) AS DOUBLE
PUBLIC DECLARE NATIVE FUNCTION SXN_CORRELATION LIB "gmbstats" ALIAS "gs_stats_correlation" (a() AS DOUBLE, b() AS DOUBLE, n AS LONG) AS DOUBLE

PUBLIC FUNCTION MEAN(a() AS DOUBLE) AS DOUBLE
    RETURN SXN_MEAN(a, UBOUND(a) + 1)
END FUNCTION
PUBLIC FUNCTION VARIANCE(a() AS DOUBLE, sample AS LONG) AS DOUBLE
    RETURN SXN_VARIANCE(a, UBOUND(a) + 1, sample)
END FUNCTION
PUBLIC FUNCTION STDDEV(a() AS DOUBLE, sample AS LONG) AS DOUBLE
    RETURN SXN_STDDEV(a, UBOUND(a) + 1, sample)
END FUNCTION
PUBLIC FUNCTION SKEWNESS(a() AS DOUBLE) AS DOUBLE
    RETURN SXN_SKEWNESS(a, UBOUND(a) + 1)
END FUNCTION
PUBLIC FUNCTION KURTOSIS(a() AS DOUBLE) AS DOUBLE
    RETURN SXN_KURTOSIS(a, UBOUND(a) + 1)
END FUNCTION
PUBLIC FUNCTION ARYMIN(a() AS DOUBLE) AS DOUBLE
    RETURN SXN_MIN(a, UBOUND(a) + 1)
END FUNCTION
PUBLIC FUNCTION ARYMAX(a() AS DOUBLE) AS DOUBLE
    RETURN SXN_MAX(a, UBOUND(a) + 1)
END FUNCTION
PUBLIC FUNCTION RANGE(a() AS DOUBLE) AS DOUBLE
    RETURN SXN_RANGE(a, UBOUND(a) + 1)
END FUNCTION
PUBLIC FUNCTION MEDIAN(a() AS DOUBLE) AS DOUBLE
    RETURN SXN_MEDIAN(a, UBOUND(a) + 1)
END FUNCTION
PUBLIC FUNCTION PERCENTILE(a() AS DOUBLE, p AS DOUBLE) AS DOUBLE
    RETURN SXN_PERCENTILE(a, UBOUND(a) + 1, p)
END FUNCTION
PUBLIC FUNCTION MAD(a() AS DOUBLE) AS DOUBLE
    RETURN SXN_MAD(a, UBOUND(a) + 1)
END FUNCTION
PUBLIC FUNCTION COVARIANCE(a() AS DOUBLE, b() AS DOUBLE, sample AS LONG) AS DOUBLE
    RETURN SXN_COVARIANCE(a, b, UBOUND(a) + 1, sample)
END FUNCTION
PUBLIC FUNCTION CORRELATION(a() AS DOUBLE, b() AS DOUBLE) AS DOUBLE
    RETURN SXN_CORRELATION(a, b, UBOUND(a) + 1)
END FUNCTION
