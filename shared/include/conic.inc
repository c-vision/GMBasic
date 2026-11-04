' ⛔ DIVIETO ASSOLUTO — NON modificare GMBasic (nessun file, nessun commit, nulla) senza
' PERMESSO ESPLICITO dell'utente. GMBasic è un progetto SEPARATO: se serve una modifica,
' FERMARSI e CHIEDERE.
'
' conic.inc — namespace OPTIMIZE: coni esponenziali/potenza (pacchetto FFI gmbconic).
' kind: 0=PEXP, 1=PPOW, 2=RPOW. Vettori dei membri: x()/s()/z() = 3.
' SOCP/SDP completi non sono esposti: le loro firme usano struct di coni e decine di
' parametri (non adatte al marshalling FFI a 8 argomenti).

NAMESPACE OPTIMIZE

' f() = valore della barriera (1 elemento).
DECLARE FUNCTION EXPCONEBARRIER LIB "gmbconic" ALIAS "ofx_expcone_barrier" (kind AS LONG, alpha AS DOUBLE, x() AS DOUBLE, f() AS DOUBLE) AS LONG
' g() = gradiente (3).
DECLARE FUNCTION EXPCONEGRAD LIB "gmbconic" ALIAS "ofx_expcone_grad" (kind AS LONG, alpha AS DOUBLE, x() AS DOUBLE, g() AS DOUBLE) AS LONG
' H() = Hessiana 3x3 row-major (9).
DECLARE FUNCTION EXPCONEHESS LIB "gmbconic" ALIAS "ofx_expcone_hess" (kind AS LONG, alpha AS DOUBLE, x() AS DOUBLE, H() AS DOUBLE) AS LONG
' 1 se s è strettamente interno al cono duale K*.
DECLARE FUNCTION EXPCONEDUALIN LIB "gmbconic" ALIAS "ofx_expcone_dual_in" (kind AS LONG, alpha AS DOUBLE, s() AS DOUBLE) AS LONG
' W() = punto di scaling (3).
DECLARE FUNCTION EXPCONEDUALPOINT LIB "gmbconic" ALIAS "ofx_expcone_dual_point" (kind AS LONG, alpha AS DOUBLE, s() AS DOUBLE, W() AS DOUBLE) AS LONG
' m() = 7 metriche (GAP, CONDZ, CONDS, REL, MZ, MS, WN).
DECLARE FUNCTION EXPCONENTMETRICS LIB "gmbconic" ALIAS "ofx_expcone_nt_metrics" (kind AS LONG, alpha AS DOUBLE, z() AS DOUBLE, s() AS DOUBLE, m() AS DOUBLE) AS LONG
' out() = Theta[9] | Thin[9] | rt[9] | scale (28). Ritorna il ramo (1 NT, 0 Hessiana, -1 errore).
DECLARE FUNCTION EXPCONESCALING LIB "gmbconic" ALIAS "ofx_expcone_scaling" (kind AS LONG, alpha AS DOUBLE, z() AS DOUBLE, s() AS DOUBLE, smu AS DOUBLE, out() AS DOUBLE) AS LONG
