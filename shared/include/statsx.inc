' statsx.inc — namespace STATSX via FFI (libgmbstats: gmbsuite/gmbstats-stats).
' Le firme seguono l'ORDINE C di gmbstats (parametri della distribuzione prima di x);
' le varianti BASIC storiche con x per primo restano nel builtin STATS finché non
' vengono migrate. Le inverse prendono p per ultimo, come in C.
NAMESPACE STATSX

' --- chi-quadro non centrale ---
PUBLIC DECLARE FUNCTION NCCHI2PDF LIB "gmbstats" ALIAS "gs_dist_nc_chi2_pdf" (df AS DOUBLE, nc AS DOUBLE, x AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NCCHI2CDF LIB "gmbstats" ALIAS "gs_dist_nc_chi2_cdf" (df AS DOUBLE, nc AS DOUBLE, x AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NCCHI2SF LIB "gmbstats" ALIAS "gs_dist_nc_chi2_sf" (df AS DOUBLE, nc AS DOUBLE, x AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NCCHI2PPF LIB "gmbstats" ALIAS "gs_dist_nc_chi2_ppf" (df AS DOUBLE, nc AS DOUBLE, p AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NCCHI2ISF LIB "gmbstats" ALIAS "gs_dist_nc_chi2_isf" (df AS DOUBLE, nc AS DOUBLE, p AS DOUBLE) AS DOUBLE

' --- beta non centrale ---
PUBLIC DECLARE FUNCTION NCBETAPDF LIB "gmbstats" ALIAS "gs_dist_nc_beta_pdf" (a AS DOUBLE, b AS DOUBLE, nc AS DOUBLE, x AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NCBETACDF LIB "gmbstats" ALIAS "gs_dist_nc_beta_cdf" (a AS DOUBLE, b AS DOUBLE, nc AS DOUBLE, x AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NCBETASF LIB "gmbstats" ALIAS "gs_dist_nc_beta_sf" (a AS DOUBLE, b AS DOUBLE, nc AS DOUBLE, x AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NCBETAPPF LIB "gmbstats" ALIAS "gs_dist_nc_beta_ppf" (a AS DOUBLE, b AS DOUBLE, nc AS DOUBLE, p AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NCBETAISF LIB "gmbstats" ALIAS "gs_dist_nc_beta_isf" (a AS DOUBLE, b AS DOUBLE, nc AS DOUBLE, p AS DOUBLE) AS DOUBLE

' --- gamma non centrale ---
PUBLIC DECLARE FUNCTION NCGAMMAPDF LIB "gmbstats" ALIAS "gs_dist_nc_gamma_pdf" (shape AS DOUBLE, nc AS DOUBLE, x AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NCGAMMACDF LIB "gmbstats" ALIAS "gs_dist_nc_gamma_cdf" (shape AS DOUBLE, nc AS DOUBLE, x AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NCGAMMASF LIB "gmbstats" ALIAS "gs_dist_nc_gamma_sf" (shape AS DOUBLE, nc AS DOUBLE, x AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NCGAMMAPPF LIB "gmbstats" ALIAS "gs_dist_nc_gamma_ppf" (shape AS DOUBLE, nc AS DOUBLE, p AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NCGAMMAISF LIB "gmbstats" ALIAS "gs_dist_nc_gamma_isf" (shape AS DOUBLE, nc AS DOUBLE, p AS DOUBLE) AS DOUBLE

' --- F non centrale ---
PUBLIC DECLARE FUNCTION NCFPDF LIB "gmbstats" ALIAS "gs_dist_ncf_pdf" (d1 AS DOUBLE, d2 AS DOUBLE, nc AS DOUBLE, x AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NCFCDF LIB "gmbstats" ALIAS "gs_dist_ncf_cdf" (d1 AS DOUBLE, d2 AS DOUBLE, nc AS DOUBLE, x AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NCFSF LIB "gmbstats" ALIAS "gs_dist_ncf_sf" (d1 AS DOUBLE, d2 AS DOUBLE, nc AS DOUBLE, x AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NCFPPF LIB "gmbstats" ALIAS "gs_dist_ncf_ppf" (d1 AS DOUBLE, d2 AS DOUBLE, nc AS DOUBLE, p AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NCFISF LIB "gmbstats" ALIAS "gs_dist_ncf_isf" (d1 AS DOUBLE, d2 AS DOUBLE, nc AS DOUBLE, p AS DOUBLE) AS DOUBLE

' --- t non centrale ---
PUBLIC DECLARE FUNCTION NCTPDF LIB "gmbstats" ALIAS "gs_dist_nct_pdf" (df AS DOUBLE, nc AS DOUBLE, x AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NCTCDF LIB "gmbstats" ALIAS "gs_dist_nct_cdf" (df AS DOUBLE, nc AS DOUBLE, x AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NCTSF LIB "gmbstats" ALIAS "gs_dist_nct_sf" (df AS DOUBLE, nc AS DOUBLE, x AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NCTPPF LIB "gmbstats" ALIAS "gs_dist_nct_ppf" (df AS DOUBLE, nc AS DOUBLE, p AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION NCTISF LIB "gmbstats" ALIAS "gs_dist_nct_isf" (df AS DOUBLE, nc AS DOUBLE, p AS DOUBLE) AS DOUBLE

' --- zero-truncated Poisson ---
PUBLIC DECLARE FUNCTION ZTPOISSONPMF LIB "gmbstats" ALIAS "gs_dist_ztpoisson_pmf" (lambda AS DOUBLE, k AS LONG) AS DOUBLE
PUBLIC DECLARE FUNCTION ZTPOISSONCDF LIB "gmbstats" ALIAS "gs_dist_ztpoisson_cdf" (lambda AS DOUBLE, k AS LONG) AS DOUBLE
