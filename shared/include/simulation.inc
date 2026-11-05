' simulation.inc — namespace SIMULATION via FFI (libgmbstats: gmbstats-simulation/base).
' RNG di gmbsuite (gs_rng, stato interno alla lib). PARTICLEFILTER scrive mean()/var();
' TPE riceve la callback f(x()) e scrive la soluzione in out(d): ritorna il valore migliore.
NAMESPACE SIMULATION

PUBLIC DECLARE FUNCTION SIMSEED LIB "gmbstats" ALIAS "ofx_sim_seed" (seed AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION SIMUNIFORM LIB "gmbstats" ALIAS "ofx_sim_uniform" () AS DOUBLE
PUBLIC DECLARE FUNCTION SIMNORMAL LIB "gmbstats" ALIAS "ofx_sim_normal" () AS DOUBLE
PUBLIC DECLARE FUNCTION SIMEXP LIB "gmbstats" ALIAS "ofx_sim_exponential" (rate AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION SIMBERNOULLI LIB "gmbstats" ALIAS "ofx_sim_bernoulli" (p AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION SIMBINOMIAL LIB "gmbstats" ALIAS "ofx_sim_binomial" (n AS DOUBLE, p AS DOUBLE) AS DOUBLE

PUBLIC DECLARE SUB PARTICLEFILTER LIB "gmbstats" ALIAS "ofx_sim_particle_filter" (obs() AS DOUBLE, T AS LONG, par() AS DOUBLE, N AS LONG, seed AS DOUBLE, mean() AS DOUBLE, var() AS DOUBLE)

PUBLIC DECLARE FUNCTION TPE LIB "gmbstats" ALIAS "ofx_sim_tpe" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, lo() AS DOUBLE, hi() AS DOUBLE, d AS LONG, nIter AS LONG, nInit AS LONG, seed AS DOUBLE, out() AS DOUBLE) AS DOUBLE
