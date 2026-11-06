' simulation.inc — namespace SIMULATION via FFI (libgmbstats: gmbstats-simulation/base).
' RNG di gmbsuite (gs_rng, stato interno alla lib). Primitive con dimensioni esplicite
' + wrapper PUBLIC che le deducono: PARTICLEFILTER da obs, TPE da lo/hi.
NAMESPACE SIMULATION

DECLARE FUNCTION SIMSEED LIB "gmbstats" ALIAS "ofx_sim_seed" (seed AS DOUBLE) AS DOUBLE
DECLARE FUNCTION SIMUNIFORM LIB "gmbstats" ALIAS "ofx_sim_uniform" () AS DOUBLE
DECLARE FUNCTION SIMNORMAL LIB "gmbstats" ALIAS "ofx_sim_normal" () AS DOUBLE
DECLARE FUNCTION SIMEXP LIB "gmbstats" ALIAS "ofx_sim_exponential" (rate AS DOUBLE) AS DOUBLE
DECLARE FUNCTION SIMBERNOULLI LIB "gmbstats" ALIAS "ofx_sim_bernoulli" (p AS DOUBLE) AS DOUBLE
DECLARE FUNCTION SIMBINOMIAL LIB "gmbstats" ALIAS "ofx_sim_binomial" (n AS DOUBLE, p AS DOUBLE) AS DOUBLE

DECLARE SUB SXF_PARTICLEFILTER LIB "gmbstats" ALIAS "ofx_sim_particle_filter__ffi" (obs() AS DOUBLE, T AS LONG, a AS DOUBLE, q AS DOUBLE, r AS DOUBLE, N AS LONG, seed AS DOUBLE, out() AS DOUBLE)
DECLARE FUNCTION SXF_TPE LIB "gmbstats" ALIAS "ofx_sim_tpe__ffi" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, lo() AS DOUBLE, hi() AS DOUBLE, d AS LONG, nIter AS LONG, nInit AS LONG, seed AS DOUBLE, out() AS DOUBLE) AS DOUBLE

' out(2*T): prima meta' media filtrata, seconda meta' varianza. T = dimensione di obs.
PUBLIC SUB PARTICLEFILTER(obs() AS DOUBLE, a AS DOUBLE, q AS DOUBLE, r AS DOUBLE, N AS LONG, seed AS DOUBLE, out() AS DOUBLE)
    CALL SXF_PARTICLEFILTER(obs, UBOUND(obs) + 1, a, q, r, N, seed, out)
END SUB

' out(d): soluzione; ritorna il valore migliore. d = dimensione di lo/hi.
PUBLIC FUNCTION TPE(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, lo() AS DOUBLE, hi() AS DOUBLE, nIter AS LONG, nInit AS LONG, seed AS DOUBLE, out() AS DOUBLE) AS DOUBLE
    RETURN SXF_TPE(f, lo, hi, UBOUND(lo) + 1, nIter, nInit, seed, out)
END FUNCTION
