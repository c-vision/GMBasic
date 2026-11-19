' stats.inc — namespace STATS via FFI (lib `gmbstats`).
' Migrazione in corso (2026-11-18): le funzioni elencate qui NON hanno più il builtin; la fonte è
' la lib. Le altre restano nel builtin finché non vengono migrate (cut-over atomico: un `.inc`
' non può convivere con un builtin con la stessa arità).
' Firma BASIC storica = `x` (o `p`) per primo; i wrapper `gs_basic_*` in `shared/libs/stats`
' girano l'ordine verso `gs_dist_*` (ordine C).
NAMESPACE STATS

PUBLIC DECLARE FUNCTION CHISQUAREDPDF LIB "gmbstats" ALIAS "gs_basic_chi2_pdf" (x AS DOUBLE, df AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION CHISQUAREDCDF LIB "gmbstats" ALIAS "gs_basic_chi2_cdf" (x AS DOUBLE, df AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION CHISQUAREDSF  LIB "gmbstats" ALIAS "gs_basic_chi2_sf"  (x AS DOUBLE, df AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION CHISQUAREDPPF LIB "gmbstats" ALIAS "gs_basic_chi2_ppf" (p AS DOUBLE, df AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION CHISQUAREDISF LIB "gmbstats" ALIAS "gs_basic_chi2_isf" (p AS DOUBLE, df AS DOUBLE) AS DOUBLE
